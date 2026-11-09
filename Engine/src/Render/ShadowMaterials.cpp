#include "ShadowMaterials.hpp"

#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/VertexLayout.hpp>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr uint32_t k_maskedShadowBaseColorSlot{0};

        vela::graphics::VertexLayout positionOnlyLayout()
        {
            return vela::graphics::VertexLayout
            {
                sizeof(PbrVertex),
                {
                    {0, vela::graphics::VertexAttributeFormat::Float3, static_cast<uint32_t>(offsetof(PbrVertex, position))}
                }
            };
        }

        vela::graphics::VertexLayout positionUvLayout()
        {
            return vela::graphics::VertexLayout
            {
                sizeof(PbrVertex),
                {
                    {0, vela::graphics::VertexAttributeFormat::Float3, static_cast<uint32_t>(offsetof(PbrVertex, position))},
                    {1, vela::graphics::VertexAttributeFormat::Float2, static_cast<uint32_t>(offsetof(PbrVertex, uv))}
                }
            };
        }

        vela::graphics::RenderState shadowRenderState(float depthBias)
        {
            vela::graphics::RenderState renderState{};
            renderState.depthBiasSlope = depthBias;

            return renderState;
        }
    } //namespace

    Result<vela::graphics::Material> createShadowMaterial(RendererState& state)
    {
        auto material = vela::graphics::Material::create(state.context,
        {
            .vertexShader = state.shaders.shadowVertex,
            .fragmentShader = state.shaders.shadowFragment,
            .vertexLayout = positionOnlyLayout(),
            .renderState = shadowRenderState(state.settings.shadows.depthBias)
        });

        if (!material)
        {
            return toError(material.error());
        }

        return std::move(material).value();
    }

    Result<vela::graphics::Material> createMaskedShadowMaterial(RendererState& state,
        const vela::graphics::Texture& baseColor)
    {
        auto material = vela::graphics::Material::create(state.context,
        {
            .vertexShader = state.shaders.shadowMaskVertex,
            .fragmentShader = state.shaders.shadowMaskFragment,
            .vertexLayout = positionUvLayout(),
            .renderState = shadowRenderState(state.settings.shadows.depthBias)
        });

        if (!material)
        {
            return toError(material.error());
        }

        if (auto bound = material.value().setTexture(k_maskedShadowBaseColorSlot, baseColor); !bound)
        {
            return toError(bound.error());
        }

        return std::move(material).value();
    }

    Status updateShadowMaterials(RendererState& state)
    {
        const float depthBias = state.settings.shadows.depthBias;

        if (state.shadowMaterial && state.shadowMaterialBias == depthBias)
        {
            return {};
        }

        auto opaque = createShadowMaterial(state);

        if (!opaque)
        {
            return opaque.error();
        }

        std::vector<std::pair<MaterialRecord*, vela::graphics::Material>> masked;
        std::optional<Error> failure;

        state.materials.forEach([&state, &masked, &failure](MaterialRecord& record)
        {
            if (failure || record.maskedShadowBaseColor == nullptr)
            {
                return;
            }

            auto material = createMaskedShadowMaterial(state, *record.maskedShadowBaseColor);

            if (!material)
            {
                failure = material.error();
                return;
            }

            masked.emplace_back(&record, std::move(material).value());
        });

        if (failure)
        {
            return *failure;
        }

        state.shadowMaterial.emplace(std::move(opaque).value());

        for (auto& [record, material] : masked)
        {
            record->maskedShadow.emplace(std::move(material));
        }

        state.shadowMaterialBias = depthBias;

        return {};
    }
} //namespace lockstep::render
