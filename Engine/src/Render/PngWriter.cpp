#include "PngWriter.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <string>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr std::size_t k_maxStoredBlock{65535};

        std::array<uint32_t, 256> crcTable()
        {
            std::array<uint32_t, 256> table{};

            for (uint32_t index = 0; index < 256; ++index)
            {
                uint32_t value = index;

                for (int bit = 0; bit < 8; ++bit)
                {
                    value = (value & 1u) != 0 ? 0xEDB88320u ^ (value >> 1) : value >> 1;
                }

                table[index] = value;
            }

            return table;
        }

        void appendBigEndian(std::vector<uint8_t>& out, uint32_t value)
        {
            out.push_back(static_cast<uint8_t>(value >> 24));
            out.push_back(static_cast<uint8_t>(value >> 16));
            out.push_back(static_cast<uint8_t>(value >> 8));
            out.push_back(static_cast<uint8_t>(value));
        }

        void appendChunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data)
        {
            static const std::array<uint32_t, 256> table = crcTable();
            appendBigEndian(out, static_cast<uint32_t>(data.size()));
            const std::size_t start = out.size();
            out.insert(out.end(), type, type + 4);
            out.insert(out.end(), data.begin(), data.end());
            uint32_t crc = 0xFFFFFFFFu;

            for (std::size_t index = start; index < out.size(); ++index)
            {
                crc = table[(crc ^ out[index]) & 0xFFu] ^ (crc >> 8);
            }

            appendBigEndian(out, crc ^ 0xFFFFFFFFu);
        }

        std::vector<uint8_t> storedZlib(const std::vector<uint8_t>& raw)
        {
            std::vector<uint8_t> out{0x78, 0x01};
            out.reserve(raw.size() + raw.size() / k_maxStoredBlock * 5 + 16);
            uint32_t a = 1;
            uint32_t b = 0;

            for (std::size_t offset = 0; offset < raw.size() || offset == 0; offset += k_maxStoredBlock)
            {
                const std::size_t length = std::min(k_maxStoredBlock, raw.size() - offset);
                const bool isLast = offset + length >= raw.size();
                out.push_back(isLast ? 1 : 0);
                out.push_back(static_cast<uint8_t>(length));
                out.push_back(static_cast<uint8_t>(length >> 8));
                out.push_back(static_cast<uint8_t>(~length));
                out.push_back(static_cast<uint8_t>(~length >> 8));
                out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                    raw.begin() + static_cast<std::ptrdiff_t>(offset + length));

                if (isLast)
                {
                    break;
                }
            }

            for (const uint8_t value : raw)
            {
                a = (a + value) % 65521u;
                b = (b + a) % 65521u;
            }

            appendBigEndian(out, (b << 16) | a);
            return out;
        }
    } //namespace

    Status writePng(std::string_view path, std::span<const std::byte> pixels, uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0 || pixels.size() != static_cast<std::size_t>(width) * height * 4)
        {
            return Error{ErrorCode::InvalidArgument, "writePng: pixel data does not match the size"};
        }

        std::vector<uint8_t> raw;
        raw.reserve(static_cast<std::size_t>(width * 3 + 1) * height);

        for (uint32_t row = 0; row < height; ++row)
        {
            raw.push_back(0);

            for (uint32_t column = 0; column < width; ++column)
            {
                const std::size_t texel = (static_cast<std::size_t>(row) * width + column) * 4;

                for (std::size_t channel = 0; channel < 3; ++channel)
                {
                    raw.push_back(static_cast<uint8_t>(pixels[texel + channel]));
                }
            }
        }

        std::vector<uint8_t> file{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
        std::vector<uint8_t> header;
        appendBigEndian(header, width);
        appendBigEndian(header, height);
        header.insert(header.end(), {8, 2, 0, 0, 0});
        appendChunk(file, "IHDR", header);
        appendChunk(file, "IDAT", storedZlib(raw));
        appendChunk(file, "IEND", {});

        std::ofstream stream{std::string(path), std::ios::binary | std::ios::trunc};
        stream.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));

        if (!stream)
        {
            return Error{ErrorCode::PlatformFailure, "writePng: could not write '" + std::string(path) + "'"};
        }

        return {};
    }
} //namespace lockstep::render
