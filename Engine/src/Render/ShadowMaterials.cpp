#include "ShadowMaterials.hpp"

#include "RendererState.hpp"
#include "Skinning.hpp"
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

        Status bindJointPalettes(RendererState& state, vela::graphics::Material& material, MeshKind kind)
        {
            if (kind != MeshKind::Skinned)
            {
                return {};
            }

            if (auto bound = material.setStorageBuffer(k_jointPaletteBinding, state.jointBuffer); !bound)
            {
                return toError(bound.error());
            }

            return {};
        }
    } //namespace

    Result<vela::graphics::Material> createShadowMaterial(RendererState& state, MeshKind kind)
    {
        const bool isSkinned = kind == MeshKind::Skinned;
        auto material = vela::graphics::Material::create(state.context,
        {
            .vertexShader = isSkinned ? state.shaders.shadowSkinnedVertex : state.shaders.shadowVertex,
            .fragmentShader = state.shaders.shadowFragment,
            .vertexLayout = isSkinned ? skinnedShadowLayout() : positionOnlyLayout(),
            .renderState = shadowRenderState(state.settings.shadows.depthBias)
        });

        if (!material)
        {
            return toError(material.error());
        }

        if (auto bound = bindJointPalettes(state, material.value(), kind); !bound)
        {
            return bound.error();
        }

        return std::move(material).value();
    }

    Result<vela::graphics::Material> createMaskedShadowMaterial(RendererState& state,
        const vela::graphics::Texture& baseColor, MeshKind kind)
    {
        const bool isSkinned = kind == MeshKind::Skinned;
        auto material = vela::graphics::Material::create(state.context,
        {
            .vertexShader = isSkinned ? state.shaders.shadowMaskSkinnedVertex : state.shaders.shadowMaskVertex,
            .fragmentShader = state.shaders.shadowMaskFragment,
            .vertexLayout = isSkinned ? skinnedMaskedShadowLayout() : positionUvLayout(),
            .renderState = shadowRenderState(state.settings.shadows.depthBias)
        });

        if (!material)
        {
            return toError(material.error());
        }

        if (auto bound = bindJointPalettes(state, material.value(), kind); !bound)
        {
            return bound.error();
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

        auto opaque = createShadowMaterial(state, MeshKind::Static);

        if (!opaque)
        {
            return opaque.error();
        }

        auto skinned = createShadowMaterial(state, MeshKind::Skinned);

        if (!skinned)
        {
            return skinned.error();
        }

        std::vector<std::pair<MaterialRecord*, vela::graphics::Material>> masked;
        std::optional<Error> failure;

        state.materials.forEach([&state, &masked, &failure](MaterialRecord& record)
        {
            if (failure || record.maskedShadowBaseColor == nullptr)
            {
                return;
            }

            auto material = createMaskedShadowMaterial(state, *record.maskedShadowBaseColor, record.kind);

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
        state.skinnedShadowMaterial.emplace(std::move(skinned).value());

        for (auto& [record, material] : masked)
        {
            record->maskedShadow.emplace(std::move(material));
        }

        state.shadowMaterialBias = depthBias;

        return {};
    }
} //namespace lockstep::render
