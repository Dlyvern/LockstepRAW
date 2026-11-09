#include "CookedFormat.hpp"

#include "BinaryStream.hpp"

namespace lockstep::assets
{
    namespace
    {
        constexpr std::size_t k_minimumStringBytes{sizeof(uint32_t)};
        constexpr std::size_t k_minimumMaterialBytes{70};
        constexpr std::size_t k_minimumMeshBytes{41};
        constexpr std::size_t k_minimumInstanceBytes{68};
        constexpr std::size_t k_minimumNodeBytes{44};
        constexpr std::size_t k_hierarchyIndexBytes{sizeof(uint32_t)};
        constexpr std::size_t k_minimumLightBytes{73};

        Error levelError(std::string_view path, std::string_view what)
        {
            return Error{ErrorCode::AssetLoadFailed, "level '" + std::string(path) + "': " + std::string(what)};
        }

        void writeVec3(BinaryWriter& writer, const math::Vec3& vector)
        {
            writer.write(vector.x);
            writer.write(vector.y);
            writer.write(vector.z);
        }

        void readVec3(BinaryReader& reader, math::Vec3& vector)
        {
            reader.read(vector.x);
            reader.read(vector.y);
            reader.read(vector.z);
        }

        void writeBounds(BinaryWriter& writer, const math::Aabb& bounds)
        {
            writeVec3(writer, bounds.min);
            writeVec3(writer, bounds.max);
        }

        void readBounds(BinaryReader& reader, math::Aabb& bounds)
        {
            readVec3(reader, bounds.min);
            readVec3(reader, bounds.max);
        }

        void writeTransform(BinaryWriter& writer, const math::Transform& transform)
        {
            writeVec3(writer, transform.position);
            writer.write(transform.rotation.x);
            writer.write(transform.rotation.y);
            writer.write(transform.rotation.z);
            writer.write(transform.rotation.w);
            writeVec3(writer, transform.scale);
        }

        void readTransform(BinaryReader& reader, math::Transform& transform)
        {
            readVec3(reader, transform.position);
            reader.read(transform.rotation.x);
            reader.read(transform.rotation.y);
            reader.read(transform.rotation.z);
            reader.read(transform.rotation.w);
            readVec3(reader, transform.scale);
        }

        void writeMaterial(BinaryWriter& writer, const CookedMaterial& material)
        {
            const render::PbrMaterialDescription& factors = material.factors;
            writer.write(factors.baseColorFactor.x);
            writer.write(factors.baseColorFactor.y);
            writer.write(factors.baseColorFactor.z);
            writer.write(factors.baseColorFactor.w);
            writer.write(factors.normalScale);
            writer.write(factors.metallicFactor);
            writer.write(factors.roughnessFactor);
            writer.write(factors.occlusionStrength);
            writeVec3(writer, factors.emissiveFactor);
            writer.write(static_cast<uint8_t>(factors.alphaMode));
            writer.write(factors.alphaCutoff);
            writer.write(static_cast<uint8_t>(factors.doubleSided ? 1 : 0));
            writer.write(material.baseColor);
            writer.write(material.normal);
            writer.write(material.metallicRoughness);
            writer.write(material.occlusion);
            writer.write(material.emissive);
        }

        void readMaterial(BinaryReader& reader, CookedMaterial& material)
        {
            render::PbrMaterialDescription& factors = material.factors;
            uint8_t alphaMode = 0;
            uint8_t doubleSided = 0;
            reader.read(factors.baseColorFactor.x);
            reader.read(factors.baseColorFactor.y);
            reader.read(factors.baseColorFactor.z);
            reader.read(factors.baseColorFactor.w);
            reader.read(factors.normalScale);
            reader.read(factors.metallicFactor);
            reader.read(factors.roughnessFactor);
            reader.read(factors.occlusionStrength);
            readVec3(reader, factors.emissiveFactor);
            reader.read(alphaMode);
            reader.read(factors.alphaCutoff);
            reader.read(doubleSided);
            reader.read(material.baseColor);
            reader.read(material.normal);
            reader.read(material.metallicRoughness);
            reader.read(material.occlusion);
            reader.read(material.emissive);
            factors.alphaMode = static_cast<render::AlphaMode>(alphaMode);
            factors.doubleSided = doubleSided != 0;
        }

        void writeMesh(BinaryWriter& writer, const CookedMesh& mesh)
        {
            writer.writeString(mesh.path);
            writer.write(mesh.material);
            writer.write(static_cast<uint8_t>(mesh.alphaMode));
            writeBounds(writer, mesh.localBounds);
            writer.write(mesh.triangleCount);
            writer.write(mesh.collisionShape);
        }

        void readMesh(BinaryReader& reader, CookedMesh& mesh)
        {
            uint8_t alphaMode = 0;
            reader.readString(mesh.path);
            reader.read(mesh.material);
            reader.read(alphaMode);
            readBounds(reader, mesh.localBounds);
            reader.read(mesh.triangleCount);
            reader.read(mesh.collisionShape);
            mesh.alphaMode = static_cast<render::AlphaMode>(alphaMode);
        }

        void writeLight(BinaryWriter& writer, const SceneLight& light)
        {
            writer.write(static_cast<uint8_t>(light.type));
            writer.writeString(light.name);
            writeVec3(writer, light.color);
            writer.write(light.intensity);
            writer.write(light.range);
            writer.write(light.innerAngle);
            writer.write(light.outerAngle);
            writeTransform(writer, light.transform);
        }

        void readLight(BinaryReader& reader, SceneLight& light)
        {
            uint8_t type = 0;
            reader.read(type);
            reader.readString(light.name);
            readVec3(reader, light.color);
            reader.read(light.intensity);
            reader.read(light.range);
            reader.read(light.innerAngle);
            reader.read(light.outerAngle);
            readTransform(reader, light.transform);
            light.type = static_cast<SceneLightType>(type);
        }

        template<typename T, typename ReadOne>
        void readArray(BinaryReader& reader, std::vector<T>& items, std::size_t minimumBytes, ReadOne readOne)
        {
            uint32_t count = 0;

            if (!reader.readCount(count, minimumBytes))
            {
                return;
            }

            items.resize(count);

            for (T& item : items)
            {
                readOne(reader, item);
            }
        }

        bool validTexture(uint32_t index, const CookedLevel& level)
        {
            return index == k_noTexture || index < level.textures.size();
        }

        bool validMaterial(const CookedMaterial& material, const CookedLevel& level)
        {
            return validTexture(material.baseColor, level) && validTexture(material.normal, level)
                && validTexture(material.metallicRoughness, level) && validTexture(material.occlusion, level)
                && validTexture(material.emissive, level)
                && material.factors.alphaMode <= render::AlphaMode::Blend;
        }

        bool validReferences(const CookedLevel& level)
        {
            for (const CookedMaterial& material : level.materials)
            {
                if (!validMaterial(material, level))
                {
                    return false;
                }
            }

            for (const CookedMesh& mesh : level.meshes)
            {
                if (mesh.material >= level.materials.size() || mesh.alphaMode > render::AlphaMode::Blend
                    || (mesh.collisionShape != k_noCollisionShape && mesh.collisionShape >= level.collisionShapes.size()))
                {
                    return false;
                }
            }

            for (const SceneInstance& instance : level.instances)
            {
                if (instance.mesh >= level.meshes.size()
                    || (instance.node != k_noNode && instance.node >= level.nodes.size()))
                {
                    return false;
                }
            }

            for (std::size_t index = 0; index < level.nodes.size(); ++index)
            {
                if (level.nodes[index].parent != k_noNode && level.nodes[index].parent >= index)
                {
                    return false;
                }
            }

            for (const SceneLight& light : level.lights)
            {
                if (light.type > SceneLightType::Spot)
                {
                    return false;
                }
            }

            return true;
        }
    } //namespace

    std::vector<std::byte> writeCookedLevel(const CookedLevel& level)
    {
        BinaryWriter writer;
        writer.write(k_levelMagic);
        writer.write(k_levelVersion);

        writer.write(static_cast<uint32_t>(level.textures.size()));

        for (const std::string& texture : level.textures)
        {
            writer.writeString(texture);
        }

        writer.write(static_cast<uint32_t>(level.materials.size()));

        for (const CookedMaterial& material : level.materials)
        {
            writeMaterial(writer, material);
        }

        writer.write(static_cast<uint32_t>(level.meshes.size()));

        for (const CookedMesh& mesh : level.meshes)
        {
            writeMesh(writer, mesh);
        }

        writer.write(static_cast<uint32_t>(level.collisionShapes.size()));

        for (const std::string& shape : level.collisionShapes)
        {
            writer.writeString(shape);
        }

        writer.write(static_cast<uint32_t>(level.instances.size()));

        for (const SceneInstance& instance : level.instances)
        {
            writer.write(instance.mesh);
            writeTransform(writer, instance.transform);
            writeBounds(writer, instance.worldBounds);
            writer.write(instance.node);
        }

        writer.write(static_cast<uint32_t>(level.nodes.size()));

        for (const SceneNode& node : level.nodes)
        {
            writer.writeString(node.name);
            writeTransform(writer, node.transform);
            writer.write(node.parent);
        }

        writer.write(static_cast<uint32_t>(level.lights.size()));

        for (const SceneLight& light : level.lights)
        {
            writeLight(writer, light);
        }

        writeBounds(writer, level.bounds);

        return writer.take();
    }

    Result<CookedLevel> readCookedLevel(std::span<const std::byte> bytes, std::string_view path)
    {
        BinaryReader reader(bytes);
        std::array<char, 4> magic{};
        uint32_t version = 0;

        if (!reader.read(magic) || magic != k_levelMagic || !reader.read(version))
        {
            return levelError(path, "not a cooked level");
        }

        if (version != k_levelVersion && version != k_levelVersionWithoutHierarchy)
        {
            return levelError(path, "format version " + std::to_string(version) + " is not supported (expected "
                + std::to_string(k_levelVersion) + "); re-cook it");
        }

        const bool hasHierarchy = version == k_levelVersion;
        const std::size_t hierarchyBytes = hasHierarchy ? k_hierarchyIndexBytes : 0;
        CookedLevel level{};
        const auto readString = [](BinaryReader& source, std::string& text) { source.readString(text); };
        readArray(reader, level.textures, k_minimumStringBytes, readString);
        readArray(reader, level.materials, k_minimumMaterialBytes, readMaterial);
        readArray(reader, level.meshes, k_minimumMeshBytes, readMesh);
        readArray(reader, level.collisionShapes, k_minimumStringBytes, readString);
        readArray(reader, level.instances, k_minimumInstanceBytes + hierarchyBytes,
            [hasHierarchy](BinaryReader& source, SceneInstance& instance)
        {
            source.read(instance.mesh);
            readTransform(source, instance.transform);
            readBounds(source, instance.worldBounds);

            if (hasHierarchy)
            {
                source.read(instance.node);
            }
        });
        readArray(reader, level.nodes, k_minimumNodeBytes + hierarchyBytes,
            [hasHierarchy](BinaryReader& source, SceneNode& node)
        {
            source.readString(node.name);
            readTransform(source, node.transform);

            if (hasHierarchy)
            {
                source.read(node.parent);
            }
        });
        readArray(reader, level.lights, k_minimumLightBytes, readLight);
        readBounds(reader, level.bounds);

        if (!reader.ok() || !reader.atEnd())
        {
            return levelError(path, "the file is truncated or damaged");
        }

        if (!validReferences(level))
        {
            return levelError(path, "an index is out of range");
        }

        return level;
    }
} //namespace lockstep::assets
