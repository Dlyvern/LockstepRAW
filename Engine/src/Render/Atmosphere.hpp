#ifndef LOCKSTEP_RENDER_ATMOSPHERE_HPP
#define LOCKSTEP_RENDER_ATMOSPHERE_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/LensRain.hpp"
#include "Lockstep/Render/Lighting.hpp"
#include "Lockstep/Render/RenderSettings.hpp"

#include <Vela/Graphics/Texture.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace vela::graphics
{
    class RenderGraph;
} //namespace vela::graphics

namespace lockstep::render
{
    struct RendererState;
    struct GpuFrameLighting;

    // Set-1 slot the sky, environment and cloud shaders read the cloud noise from (shaders/clouds.glsl).
    inline constexpr uint32_t k_cloudNoiseSlot{1};

    // Renderer state of the atmosphere features (wind, clouds, lightning, lens rain).
    // cloudNoise:      the tiling cloud noise texture (CloudNoise.hpp), created with the graph.
    // previousWind:    the wind block of the previous frame (motion vectors of swaying foliage).
    // hasPreviousWind: previousWind is valid.
    // cloudScale:      the scale the volumetric cloud pass was last described with.
    // lensDroplets:    the drops submitted for the next frame (Renderer::submitLensDroplets()).
    // lensDropletFrame: RendererState::frameIndex they were submitted for; older drops are not drawn.
    struct AtmosphereState
    {
        std::optional<vela::graphics::Texture> cloudNoise;
        math::Vec4 previousWind{0.0f, 0.0f, 0.0f, 0.0f};
        bool hasPreviousWind{false};
        float cloudScale{0.5f};
        std::vector<LensDroplet> lensDroplets;
        uint32_t lensDropletFrame{0};
    };

    // Creates the cloud noise texture once.
    // state:   the renderer state.
    // Returns: success, or an error if the upload failed.
    [[nodiscard]] Status createAtmosphereResources(RendererState& state);

    // The frame's directional light with a lightning flash applied: while the flash outshines the
    // sun, the flash replaces it (direction away from the strike, the flash's colour, shadows as
    // LightningSettings::castsShadows says).
    // sun:      the frame's sun before the flash (submitted or the sky's), if any.
    // settings: the frame's settings (lighting switches and the flash).
    // Returns:  the light to use.
    [[nodiscard]] std::optional<DirectionalLight> lightningSun(const std::optional<DirectionalLight>& sun,
        const RenderSettings& settings);

    // Writes the wind, cloud and lightning fields of the frame's lighting block (prepareLighting()
    // writes the sky sun).
    // gpu:        the block.
    // settings:   the frame's settings.
    // atmosphere: wind history (updated).
    // frameIndex: the frame number (cloud jitter).
    void packAtmosphere(GpuFrameLighting& gpu, const RenderSettings& settings, AtmosphereState& atmosphere,
        uint32_t frameIndex);

    // Adds the volumetric cloud passes: the raymarch at reduced resolution and the composite over the
    // lit scene's sky (shaders/clouds.frag, clouds_composite.frag). Both skip their draw unless
    // volumetric clouds are on.
    // graph:   the frame graph, holding the lit scene pass.
    // state:   the renderer state. Must outlive the graph.
    // Returns: success, or an error if a shader is missing or the graph rejected a pass.
    [[nodiscard]] Status addCloudPasses(vela::graphics::RenderGraph& graph, RendererState& state);

    // Redescribes the cloud raymarch pass when CloudSettings::resolutionScale changed.
    // state:   the renderer state.
    // Returns: success, or an error if the graph rejected the new description (the previous scale
    //          is kept).
    [[nodiscard]] Status updateCloudScale(RendererState& state);

    // Adds the lens rain pass (shaders/lens_rain.vert/.frag): the frame's lens drops drawn over the
    // display image, refracting the anti-aliased image (the TAA result, else the tonemapped one).
    // graph:   the frame graph, holding the output pass.
    // state:   the renderer state. Must outlive the graph.
    // Returns: success, or an error if a shader is missing or the graph rejected the pass.
    [[nodiscard]] Status addLensRainPass(vela::graphics::RenderGraph& graph, RendererState& state);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_ATMOSPHERE_HPP
