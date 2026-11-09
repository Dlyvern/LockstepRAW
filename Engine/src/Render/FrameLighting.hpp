#ifndef LOCKSTEP_RENDER_FRAME_LIGHTING_HPP
#define LOCKSTEP_RENDER_FRAME_LIGHTING_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Render/Lighting.hpp"
#include "Lockstep/Render/RenderSettings.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace lockstep::render
{
    // Side length of the square shadow map, in texels.
    inline constexpr uint32_t k_shadowMapSize{1024};

    // One point or spot light as lit.frag reads it (std430, 64 bytes).
    // positionRange:     xyz world position, w range.
    // color:             rgb colour times intensity.
    // directionCosOuter: xyz spot direction, w cosine of the outer angle.
    // cosInnerKind:      x cosine of the inner angle, y 0 for point or 1 for spot.
    struct GpuLocalLight
    {
        float positionRange[4];
        float color[4];
        float directionCosOuter[4];
        float cosInnerKind[4];
    };

    // The per-frame lighting block lit.frag reads from its storage buffer (std430). Field
    // meanings are documented in lit.frag next to their uses; the layout must match it.
    struct GpuFrameLighting
    {
        math::Mat4 shadowViewProjection{1.0f};
        float ambient[4];
        float directionalDirection[4];
        float directionalColor[4];
        float fogColor[4];
        float fogParams[4];
        float shadowParams[4];
        uint32_t counts[4];
        GpuLocalLight lights[k_maxLocalLights];
    };

    static_assert(sizeof(GpuLocalLight) == 64);
    static_assert(sizeof(GpuFrameLighting) == 64 + 7 * 16 + k_maxLocalLights * 64);

    // The lights submitted for one frame.
    // directional: the directional light, if one was submitted.
    // points:      point lights, at most k_maxLocalLights together with `spots`.
    // spots:       spot lights.
    // dropped:     lights rejected because the limit was reached.
    struct FrameLights
    {
        std::optional<DirectionalLight> directional;
        std::vector<PointLight> points;
        std::vector<SpotLight> spots;
        uint32_t dropped{0};

        // Returns: true if another point or spot light still fits.
        [[nodiscard]] bool hasRoom() const;

        // Forgets every light, ready for the next frame.
        void clear();
    };

    // Which light casts the frame's shadow map, and from where.
    // None: no shadows this frame. Directional: the directional light. Spot: a spot light.
    enum class ShadowCaster : uint8_t
    {
        None = 0,
        Directional,
        Spot
    };

    // Everything the shadow pass and the lit pass need for one frame.
    // lighting:              the block uploaded to lit.frag.
    // caster:                which light casts the shadow map.
    // shadowViewProjection:  world to shadow clip space of that light.
    struct PreparedLighting
    {
        GpuFrameLighting lighting{};
        ShadowCaster caster{ShadowCaster::None};
        math::Mat4 shadowViewProjection{1.0f};
    };

    // Packs a frame's lights and settings into the GPU block and picks the shadow caster: the
    // first shadow-casting spot light, else a shadow-casting directional light.
    // lights:   the frame's submitted lights.
    // settings: the renderer settings.
    // Returns:  the prepared lighting.
    [[nodiscard]] PreparedLighting prepareLighting(const FrameLights& lights, const RenderSettings& settings);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_FRAME_LIGHTING_HPP
