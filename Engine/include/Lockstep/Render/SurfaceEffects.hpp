#ifndef LOCKSTEP_RENDER_SURFACE_EFFECTS_HPP
#define LOCKSTEP_RENDER_SURFACE_EFFECTS_HPP

#include "Lockstep/Core/Math.hpp"

#include <cstdint>

namespace lockstep::render
{
    // Most drip puddles one frame shades; the renderer keeps the ones nearest the camera.
    inline constexpr uint32_t k_maxDripPuddles{8};

    // A small patch of standing water where a leak drips (Renderer::submitDripPuddle()): upward-
    // facing surfaces within `radius` of it (and within 0.3 m of its height) turn wet, dark and
    // glossy with a ragged edge, and each drop sends ripple rings across it. Submitted every frame,
    // like lights.
    // position: centre of the puddle on the floor, world space.
    // radius:   puddle radius, metres.
    // dropAge:  seconds since the latest drop landed (the caller's clock; World time keeps it in
    //           step with bullet-time).
    // interval: seconds between drops; the rings of the two drops before are drawn as if they had
    //           landed `interval` apart.
    // strength: 0 (dry, no rings) to 1.
    struct DripPuddle
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        float radius{0.3f};
        float dropAge{10.0f};
        float interval{1.5f};
        float strength{1.0f};
    };

    // Animated surfaces, part of RenderSettings.
    // screens:        animate ShadingModel::Screen materials (scanlines, a slow rolling band, a
    //                 faint noise shimmer and, when the material says so, scrolling content);
    //                 off: they glow statically.
    // scanlines:      depth of the screen scanlines and rolling band, 0 to 1.
    // dripPuddles:    shade the submitted drip puddles.
    // rippleStrength: slope of the drip ripple rings, 0 (still water) to 2.
    // time:           seconds driving the screen animation; the caller advances it (World time, so
    //                 bullet-time slows it).
    struct SurfaceEffectSettings
    {
        bool screens{true};
        float scanlines{0.5f};
        bool dripPuddles{true};
        float rippleStrength{1.0f};
        float time{0.0f};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SURFACE_EFFECTS_HPP
