#include "TextureCaps.hpp"

#include <Lockstep/Core/Bounds.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace lockstep::cooker
{
    namespace
    {
        float uvExtent(const assets::PrimitiveGeometry& geometry)
        {
            if (geometry.vertices.empty())
            {
                return 0.0f;
            }

            std::array<float, 2> low{geometry.vertices[0].uv[0], geometry.vertices[0].uv[1]};
            std::array<float, 2> high = low;

            for (const render::PbrVertex& vertex : geometry.vertices)
            {
                for (std::size_t axis = 0; axis < 2; ++axis)
                {
                    low[axis] = std::min(low[axis], vertex.uv[axis]);
                    high[axis] = std::max(high[axis], vertex.uv[axis]);
                }
            }

            return std::max(high[0] - low[0], high[1] - low[1]);
        }

        uint32_t sizeRequest(const math::Aabb& bounds, const TextureCapOptions& options)
        {
            const uint32_t floor = std::min(k_minAutomaticTextureCap, options.maxSize);

            if (math::isEmpty(bounds))
            {
                return floor;
            }

            const float diagonal = math::length(bounds.max - bounds.min);
            const double texels = std::ceil(static_cast<double>(diagonal) * options.texelsPerMetre);

            if (!(texels < static_cast<double>(options.maxSize)))
            {
                return options.maxSize;
            }

            const uint32_t request = std::bit_ceil(std::max(1u, static_cast<uint32_t>(texels)));

            return std::clamp(request, floor, options.maxSize);
        }

        void raise(std::vector<uint32_t>& caps, uint32_t texture, uint32_t request)
        {
            if (texture != assets::k_noTexture)
            {
                caps[texture] = std::max(caps[texture], request);
            }
        }
    } //namespace

    std::vector<uint32_t> textureCaps(const SourceScene& scene, const TextureCapOptions& options)
    {
        if (!options.automatic)
        {
            return std::vector<uint32_t>(scene.textures.size(), options.maxSize);
        }

        std::vector<uint32_t> caps(scene.textures.size(), 0);
        std::vector<float> extents(scene.meshes.size());

        for (std::size_t mesh = 0; mesh < scene.meshes.size(); ++mesh)
        {
            extents[mesh] = uvExtent(scene.meshes[mesh].geometry);
        }

        for (const assets::SceneInstance& instance : scene.instances)
        {
            const SourceMesh& mesh = scene.meshes[instance.mesh];
            const uint32_t request = extents[instance.mesh] > k_tilingUvExtent ? options.maxSize
                : sizeRequest(instance.worldBounds, options);
            const assets::CookedMaterial& material = scene.materials[mesh.material];
            raise(caps, material.baseColor, request);
            raise(caps, material.normal, request);
            raise(caps, material.metallicRoughness, request);
            raise(caps, material.occlusion, request);
            raise(caps, material.emissive, request);
        }

        for (uint32_t& cap : caps)
        {
            if (cap == 0)
            {
                cap = options.maxSize;
            }
        }

        return caps;
    }
} //namespace lockstep::cooker
