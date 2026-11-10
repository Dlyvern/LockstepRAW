#ifndef LOCKSTEP_RENDER_SHADOW_MATERIALS_HPP
#define LOCKSTEP_RENDER_SHADOW_MATERIALS_HPP

#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/PbrMaterial.hpp"

#include <Vela/Graphics/Material.hpp>
#include <Vela/Graphics/Texture.hpp>

#include <cstdint>

namespace lockstep::render
{
    struct RendererState;

    // Set-1 binding of RendererState::shadowInstanceBuffer in the static shadow materials
    // (shadow.vert, shadow_mask.vert): per instance, the light matrix (tile view-projection times
    // model) of an instanced caster.
    inline constexpr uint32_t k_shadowInstanceBinding{1};

    // Creates the depth-only material opaque casters render into the shadow atlas with, using
    // the hardware depth bias of RenderSettings::shadows.depthBias. Static casters are drawn
    // instanced from the shadow instance buffer; skinned ones one by one with push constants.
    // state:   the renderer state holding the shadow shaders and settings.
    // kind:    static casters, or skinned ones (skinned on the GPU from the joint buffer).
    // Returns: the material, or an error if the pipeline could not be created.
    [[nodiscard]] Result<vela::graphics::Material> createShadowMaterial(RendererState& state, MeshKind kind);

    // Creates the shadow material of an alpha-masked lit material: depth only, discarding by the
    // base colour alpha, with the same depth bias as createShadowMaterial(). Static ones are drawn
    // instanced like createShadowMaterial()'s.
    // state:     the renderer state holding the shadow shaders and settings.
    // baseColor: the base colour texture whose alpha masks the caster. Must outlive the material.
    // kind:      the kind of mesh the lit material draws.
    // Returns:   the material, or an error if the pipeline could not be created.
    [[nodiscard]] Result<vela::graphics::Material> createMaskedShadowMaterial(RendererState& state,
        const vela::graphics::Texture& baseColor, MeshKind kind);

    // Rebuilds every shadow material (the opaque static and skinned ones and each masked material's) when the shadow
    // depth bias setting changed since they were built, because depth bias is pipeline state.
    // Old materials are released once frames in flight are done with them.
    // state:   the renderer state.
    // Returns: success, or an error if a material could not be rebuilt (the old ones stay).
    [[nodiscard]] Status updateShadowMaterials(RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SHADOW_MATERIALS_HPP
