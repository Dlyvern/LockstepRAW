#ifndef LOCKSTEP_RENDER_IRRADIANCE_VOLUME_STATE_HPP
#define LOCKSTEP_RENDER_IRRADIANCE_VOLUME_STATE_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/IrradianceVolume.hpp"

#include <Vela/Graphics/Material.hpp>
#include <Vela/Graphics/Texture.hpp>

#include <array>
#include <optional>
#include <vector>

namespace lockstep::render
{
    struct RendererState;
    struct EnvironmentResources;

    // First material binding (set 1) of the light profile atlas; the four irradiance textures
    // follow it (shaders/light_shapes.glsl, shaders/irradiance_volume.glsl).
    inline constexpr uint32_t k_lightProfileBinding{16};
    inline constexpr uint32_t k_irradianceBinding{17};
    inline constexpr uint32_t k_irradianceTextures{4};
    inline constexpr uint32_t k_brdfLutBinding{21};

    // The irradiance volume the renderer samples: the baked terms on the CPU, and the GPU textures
    // holding them combined for the current sun and sky.
    // volume:    the volume set with Renderer::setIrradianceVolume(), if any.
    // textures:  red, green, blue bounce light and the sky visibility (see irradiance_volume.glsl).
    // composed:  sun radiance (xyz) and sky radiance scale (w) the textures were combined with.
    // isComposed: the textures hold the volume combined with `composed`.
    struct IrradianceVolumeState
    {
        std::optional<IrradianceVolume> volume;
        std::vector<vela::graphics::Texture> textures;
        math::Vec4 composed{0.0f, 0.0f, 0.0f, 0.0f};
        bool isComposed{false};
    };

    // Binds the light profile atlas, the current irradiance textures (or black ones) and the BRDF
    // lookup table to a lit material.
    // state:    the renderer state.
    // material: a material using pbr.frag.
    // Returns:  success, or the binding error.
    [[nodiscard]] Status bindLightingTextures(const RendererState& state, vela::graphics::Material& material);

    // Replaces the irradiance volume: checks it, creates its textures and rebinds every lit material.
    // state:  the renderer state.
    // volume: the new volume, or null to remove it.
    // Returns: success, or InvalidArgument for a malformed volume, or an upload or binding error.
    [[nodiscard]] Status setIrradianceVolume(RendererState& state, const IrradianceVolume* volume);

    // Recombines the volume's terms when the sun or sky changed, and writes the frame's irradiance
    // parameters into the prepared lighting block. Call after prepareLighting().
    // state: the renderer state.
    void updateIrradianceVolume(RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_IRRADIANCE_VOLUME_STATE_HPP
