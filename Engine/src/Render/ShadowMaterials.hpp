#ifndef LOCKSTEP_RENDER_SHADOW_MATERIALS_HPP
#define LOCKSTEP_RENDER_SHADOW_MATERIALS_HPP

#include "Lockstep/Core/Result.hpp"

#include <Vela/Graphics/Material.hpp>
#include <Vela/Graphics/Texture.hpp>

namespace lockstep::render
{
    struct RendererState;

    // Creates the depth-only material opaque casters render into the shadow atlas with, using
    // the hardware depth bias of RenderSettings::shadows.depthBias.
    // state:   the renderer state holding the shadow shaders and settings.
    // Returns: the material, or an error if the pipeline could not be created.
    [[nodiscard]] Result<vela::graphics::Material> createShadowMaterial(RendererState& state);

    // Creates the shadow material of an alpha-masked lit material: depth only, discarding by the
    // base colour alpha, with the same depth bias as createShadowMaterial().
    // state:     the renderer state holding the shadow shaders and settings.
    // baseColor: the base colour texture whose alpha masks the caster. Must outlive the material.
    // Returns:   the material, or an error if the pipeline could not be created.
    [[nodiscard]] Result<vela::graphics::Material> createMaskedShadowMaterial(RendererState& state,
        const vela::graphics::Texture& baseColor);

    // Rebuilds every shadow material (the opaque one and each masked material's) when the shadow
    // depth bias setting changed since they were built, because depth bias is pipeline state.
    // Old materials are released once frames in flight are done with them.
    // state:   the renderer state.
    // Returns: success, or an error if a material could not be rebuilt (the old ones stay).
    [[nodiscard]] Status updateShadowMaterials(RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SHADOW_MATERIALS_HPP
