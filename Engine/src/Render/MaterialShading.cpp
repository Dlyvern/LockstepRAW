#include "MaterialShading.hpp"

#include "Lockstep/Render/Renderer.hpp"

#include "RendererState.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>

namespace lockstep::render
{
    namespace
    {
        constexpr std::size_t k_surfaceHeaderVectors{1};
        constexpr std::size_t k_vectorsPerStain{2};
        constexpr std::size_t k_vectorsPerMatrix{4};
        constexpr std::size_t k_surfaceVectors{k_surfaceHeaderVectors + k_maxSurfaceStains * k_vectorsPerStain};
        constexpr std::size_t k_surfaceMatrices{(k_surfaceVectors + k_vectorsPerMatrix - 1) / k_vectorsPerMatrix};
        constexpr float k_maxStrength{2.0f};
        constexpr float k_maxDecalRelief{0.5f};
        constexpr float k_maxScroll{4.0f};

        float finiteOr(float value, float fallback)
        {
            return std::isfinite(value) ? value : fallback;
        }

        float unit(float value)
        {
            return std::clamp(finiteOr(value, 0.0f), 0.0f, 1.0f);
        }

        void setVector(math::Mat4& matrix, std::size_t column, float x, float y, float z, float w)
        {
            matrix.setColumn(column, math::Vec4{x, y, z, w});
        }

        std::array<float, 4> drawShading(const DrawItem& item, const MaterialShadingSettings& settings)
        {
            std::array<float, 4> shading{};
            const MaterialShading& material = item.material->shadingModel;
            const float strength = std::clamp(finiteOr(material.strength, 0.0f), 0.0f, k_maxStrength);

            if (material.model == ShadingModel::Skin && settings.skin)
            {
                shading[0] = 1.0f;
                shading[1] = strength * std::clamp(settings.skinScatter, 0.0f, k_maxStrength);
            }
            else if (material.model == ShadingModel::Fabric && settings.fabricSheen)
            {
                shading[0] = 2.0f;
                shading[1] = strength * std::clamp(settings.sheenIntensity, 0.0f, k_maxStrength);
                shading[2] = std::clamp(finiteOr(material.sheenRoughness, 0.6f), 0.2f, 1.0f);
            }
            else if (material.model == ShadingModel::Screen)
            {
                shading[0] = material.terminal ? 4.0f : 3.0f;
                shading[1] = strength;
                shading[2] = std::clamp(finiteOr(material.scroll, 0.0f), -k_maxScroll, k_maxScroll);
            }
            else if (item.isDecal && settings.decalRelief)
            {
                shading[2] = std::clamp(finiteOr(material.decalRelief, 0.0f) * std::clamp(settings.decalReliefScale,
                    0.0f, 3.0f), -k_maxDecalRelief, k_maxDecalRelief);
            }

            if (settings.characterSurfaces && item.characterSurface != k_noCharacterSurface)
            {
                shading[3] = std::bit_cast<float>(item.characterSurface);
            }

            return shading;
        }
    } //namespace

    bool Renderer::setMaterialShading(MaterialHandle material, const MaterialShading& shading)
    {
        MaterialRecord* record = m_state->materials.find(material) != nullptr
            ? m_state->materials.atIndex(material.index) : nullptr;

        if (record == nullptr || record->shading != Shading::Lit)
        {
            return false;
        }

        record->shadingModel = shading;
        return true;
    }

    CharacterSurfaceId Renderer::submitCharacterSurface(const CharacterSurface& surface)
    {
        RendererState& state = *m_state;

        if (state.characterSurfaceCount >= k_maxCharacterSurfaces)
        {
            return k_noCharacterSurface;
        }

        const std::size_t first = state.jointData.size();
        state.jointData.resize(first + k_surfaceMatrices, math::Mat4{0.0f});
        const uint32_t count = std::min(surface.stainCount, k_maxSurfaceStains);
        std::array<math::Mat4, k_surfaceMatrices> block{};
        block.fill(math::Mat4{0.0f});

        auto store = [&block](std::size_t vector, float x, float y, float z, float w)
        {
            setVector(block[vector / k_vectorsPerMatrix], vector % k_vectorsPerMatrix, x, y, z, w);
        };

        store(0, unit(surface.soak), unit(surface.grime), static_cast<float>(count), finiteOr(surface.noiseSeed, 0.0f));

        for (uint32_t index = 0; index < count; ++index)
        {
            const SurfaceStain& stain = surface.stains[index];
            const std::size_t vector = k_surfaceHeaderVectors + index * k_vectorsPerStain;
            store(vector, finiteOr(stain.position.x, 0.0f), finiteOr(stain.position.y, 0.0f),
                finiteOr(stain.position.z, 0.0f), std::max(finiteOr(stain.radius, 0.0f), 1e-3f));
            store(vector + 1, unit(stain.blood), unit(stain.freshness), unit(stain.dirt), 0.0f);
        }

        std::copy(block.begin(), block.end(), state.jointData.begin() + static_cast<std::ptrdiff_t>(first));
        ++state.characterSurfaceCount;

        return static_cast<CharacterSurfaceId>(first * k_vectorsPerMatrix + 1);
    }

    void resolveMaterialShading(RendererState& state)
    {
        const MaterialShadingSettings& settings = state.settings.materials;

        for (DrawItem& item : state.draws)
        {
            if (item.material != nullptr && item.material->shading == Shading::Lit)
            {
                item.shading = drawShading(item, settings);
            }
        }
    }
} //namespace lockstep::render
