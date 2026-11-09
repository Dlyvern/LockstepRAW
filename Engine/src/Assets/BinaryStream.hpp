#ifndef LOCKSTEP_ASSETS_BINARY_STREAM_HPP
#define LOCKSTEP_ASSETS_BINARY_STREAM_HPP

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace lockstep::assets
{
    static_assert(std::endian::native == std::endian::little, "cooked files are little-endian");

    // Appends plain values to a byte buffer in their in-memory (little-endian) layout, for
    // cooked files. Strings are a uint32 length followed by the characters.
    class BinaryWriter
    {
    public:
        // Appends a trivially copyable value as is.
        // value: the value.
        template<typename T>
        void write(const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            writeBytes(std::as_bytes(std::span<const T>(&value, 1)));
        }

        // Appends raw bytes.
        // bytes: the bytes.
        void writeBytes(std::span<const std::byte> bytes)
        {
            m_bytes.insert(m_bytes.end(), bytes.begin(), bytes.end());
        }

        // Appends a length-prefixed string.
        // text: the string.
        void writeString(std::string_view text)
        {
            write(static_cast<uint32_t>(text.size()));
            writeBytes(std::as_bytes(std::span<const char>(text.data(), text.size())));
        }

        // Returns: everything written; the writer is empty afterwards.
        [[nodiscard]] std::vector<std::byte> take() noexcept
        {
            return std::move(m_bytes);
        }

    private:
        std::vector<std::byte> m_bytes;
    };

    // Reads what a BinaryWriter wrote. Reading past the end fails softly: the value is left
    // unchanged and ok() turns false for good, so a caller checks once at the end.
    class BinaryReader
    {
    public:
        // bytes: the data; must outlive the reader.
        explicit BinaryReader(std::span<const std::byte> bytes) noexcept
            : m_bytes(bytes)
        {
        }

        // Reads a trivially copyable value.
        // value:   receives it.
        // Returns: false if not enough bytes were left.
        template<typename T>
        bool read(T& value) noexcept
        {
            static_assert(std::is_trivially_copyable_v<T>);
            return readBytes(std::as_writable_bytes(std::span<T>(&value, 1)));
        }

        // Reads raw bytes.
        // bytes:   receives them; its size is how many to read.
        // Returns: false if not enough bytes were left.
        bool readBytes(std::span<std::byte> bytes) noexcept
        {
            if (!m_ok || m_bytes.size() - m_offset < bytes.size())
            {
                m_ok = false;
                return false;
            }

            if (!bytes.empty())
            {
                std::memcpy(bytes.data(), m_bytes.data() + m_offset, bytes.size());
            }

            m_offset += bytes.size();
            return true;
        }

        // Reads a length-prefixed string.
        // text:    receives it.
        // Returns: false if not enough bytes were left.
        bool readString(std::string& text)
        {
            uint32_t size = 0;

            if (!read(size) || m_bytes.size() - m_offset < size)
            {
                m_ok = false;
                return false;
            }

            text.assign(reinterpret_cast<const char*>(m_bytes.data() + m_offset), size);
            m_offset += size;
            return true;
        }

        // Reads a count and checks it against what is left, so a damaged count can't make the
        // caller allocate a huge array.
        // count:        receives the count.
        // minimumBytes: the least each element takes in the stream.
        // Returns:      false if the count can't be read or the elements can't fit.
        bool readCount(uint32_t& count, std::size_t minimumBytes) noexcept
        {
            if (!read(count) || static_cast<uint64_t>(count) * minimumBytes > m_bytes.size() - m_offset)
            {
                m_ok = false;
                return false;
            }

            return true;
        }

        // Returns: true while every read so far succeeded.
        [[nodiscard]] bool ok() const noexcept
        {
            return m_ok;
        }

        // Returns: true if every byte was consumed.
        [[nodiscard]] bool atEnd() const noexcept
        {
            return m_offset == m_bytes.size();
        }

    private:
        std::span<const std::byte> m_bytes;
        std::size_t m_offset{0};
        bool m_ok{true};
    };
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_BINARY_STREAM_HPP
