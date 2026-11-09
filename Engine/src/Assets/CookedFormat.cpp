#include "CookedFormat.hpp"

#include "BinaryStream.hpp"

#include <algorithm>
#include <cstring>

namespace lockstep::assets
{
    namespace
    {
        constexpr uint32_t k_lastTextureFormat{static_cast<uint32_t>(render::TextureFormat::Bc5Unorm)};

        Error fileError(std::string_view kind, std::string_view path, std::string_view what)
        {
            return Error{ErrorCode::AssetLoadFailed, std::string(kind) + " '" + std::string(path) + "': "
                + std::string(what)};
        }

        template<typename Header>
        Result<Header> readHeader(std::span<const std::byte> bytes, const std::array<char, 4>& magic,
            std::string_view kind, std::string_view path)
        {
            Header header{};

            if (bytes.size() < sizeof(Header))
            {
                return fileError(kind, path, "not a cooked file");
            }

            std::memcpy(&header, bytes.data(), sizeof(Header));

            if (header.magic != magic)
            {
                return fileError(kind, path, "not a cooked file");
            }

            if (header.version != k_cookedVersion)
            {
                return fileError(kind, path, "format version " + std::to_string(header.version)
                    + " is not supported; re-cook it");
            }

            return header;
        }

        uint64_t textureBytes(const CookedTextureHeader& header)
        {
            const auto format = static_cast<render::TextureFormat>(header.format);
            uint64_t total = 0;

            for (uint32_t level = 0; level < header.levels; ++level)
            {
                total += render::textureLevelBytes(format, std::max(1u, header.width >> level),
                    std::max(1u, header.height >> level));
            }

            return total;
        }
    } //namespace

    std::vector<std::byte> writeCookedMesh(std::span<const render::PbrVertex> vertices,
        std::span<const uint32_t> indices, const math::Aabb& bounds)
    {
        CookedMeshHeader header{};
        header.vertexCount = static_cast<uint32_t>(vertices.size());
        header.indexCount = static_cast<uint32_t>(indices.size());
        header.bounds = {bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y, bounds.max.z};

        BinaryWriter writer;
        writer.write(header);
        writer.writeBytes(std::as_bytes(vertices));
        writer.writeBytes(std::as_bytes(indices));

        return writer.take();
    }

    Result<CookedMeshData> readCookedMesh(std::span<const std::byte> bytes, std::string_view path)
    {
        auto header = readHeader<CookedMeshHeader>(bytes, k_meshMagic, "mesh", path);

        if (!header)
        {
            return header.error();
        }

        const CookedMeshHeader& fields = header.value();
        const uint64_t vertexBytes = static_cast<uint64_t>(fields.vertexCount) * sizeof(render::PbrVertex);
        const uint64_t indexBytes = static_cast<uint64_t>(fields.indexCount) * sizeof(uint32_t);

        if (sizeof(CookedMeshHeader) + vertexBytes + indexBytes != bytes.size() || fields.indexCount % 3 != 0)
        {
            return fileError("mesh", path, "size does not match its header");
        }

        CookedMeshData mesh{};
        mesh.vertices.resize(fields.vertexCount);
        mesh.indices.resize(fields.indexCount);
        std::memcpy(mesh.vertices.data(), bytes.data() + sizeof(CookedMeshHeader), static_cast<std::size_t>(vertexBytes));
        std::memcpy(mesh.indices.data(), bytes.data() + sizeof(CookedMeshHeader) + vertexBytes,
            static_cast<std::size_t>(indexBytes));
        mesh.bounds.min = math::Vec3{fields.bounds[0], fields.bounds[1], fields.bounds[2]};
        mesh.bounds.max = math::Vec3{fields.bounds[3], fields.bounds[4], fields.bounds[5]};

        if (std::any_of(mesh.indices.begin(), mesh.indices.end(),
                [&fields](uint32_t index) { return index >= fields.vertexCount; }))
        {
            return fileError("mesh", path, "an index is out of range");
        }

        return mesh;
    }

    std::vector<std::byte> writeCookedTexture(CookedTextureHeader header, std::span<const std::byte> pixels)
    {
        header.magic = k_textureMagic;
        header.version = k_cookedVersion;

        BinaryWriter writer;
        writer.write(header);
        writer.writeBytes(pixels);

        return writer.take();
    }

    Result<CookedTextureView> readCookedTexture(std::span<const std::byte> bytes, std::string_view path)
    {
        auto header = readHeader<CookedTextureHeader>(bytes, k_textureMagic, "texture", path);

        if (!header)
        {
            return header.error();
        }

        const CookedTextureHeader& fields = header.value();

        if (fields.format > k_lastTextureFormat || fields.width == 0 || fields.height == 0 || fields.levels == 0
            || fields.levels > render::fullMipLevelCount(fields.width, fields.height))
        {
            return fileError("texture", path, "header is damaged");
        }

        if (sizeof(CookedTextureHeader) + textureBytes(fields) != bytes.size())
        {
            return fileError("texture", path, "size does not match its header");
        }

        return CookedTextureView{.header = fields, .pixels = bytes.subspan(sizeof(CookedTextureHeader))};
    }

    std::vector<std::byte> writeCookedCollision(uint32_t triangleCount, std::span<const std::byte> cooked)
    {
        CookedCollisionHeader header{};
        header.triangleCount = triangleCount;

        BinaryWriter writer;
        writer.write(header);
        writer.writeBytes(cooked);

        return writer.take();
    }

    Result<std::vector<std::byte>> readCookedCollision(std::vector<std::byte> bytes, std::string_view path)
    {
        auto header = readHeader<CookedCollisionHeader>(bytes, k_collisionMagic, "collision", path);

        if (!header)
        {
            return header.error();
        }

        bytes.erase(bytes.begin(), bytes.begin() + sizeof(CookedCollisionHeader));

        return bytes;
    }
} //namespace lockstep::assets
