#ifndef LOCKSTEP_RENDER_WETNESS_SETTINGS_HPP
#define LOCKSTEP_RENDER_WETNESS_SETTINGS_HPP

// Render settings for wet surfaces (rain), part of RenderSettings.
namespace lockstep::render
{
    // Rain-wet surfaces in the lit pass, outdoors only: pixels inside the interior ambient zones
    // (Renderer::setAmbientZones) stay dry, so a zone covering a building's walls and roof keeps
    // its inside dry. Wet porous surfaces (rough dielectrics) darken, every wet surface gets
    // glossier, and flat upward-facing ground collects puddles from a world-space noise mask:
    // near-mirror water that screen-space reflections pick up, with rain ripples. Surfaces facing
    // down (the underside of an overhang) stay dry; walls get less wet than the ground.
    // enabled:     apply it.
    // amount:      how wet surfaces are, 0 (dry) to 1 (soaked); puddles fill in over the upper half.
    // puddles:     share of flat ground covered by puddles when soaked, 0 to 1.
    // puddleSize:  typical puddle size in metres (the noise mask's scale).
    // darkening:   albedo multiplier of fully wet porous surfaces (0.4 dark asphalt, 1 none).
    // glossiness:  how much of a wet surface's roughness the water film removes, 0 to 1.
    // ripples:     strength of the rain ripple normals in puddles, 0 (still water) to 1 (heavy rain).
    // rippleTime:  seconds of the ripple animation; the caller advances it (with game time, so a
    //              slowed clock slows the ripples).
    // debugView:   tint wet surfaces blue and puddles cyan.
    struct WetnessSettings
    {
        bool enabled{false};
        float amount{1.0f};
        float puddles{0.45f};
        float puddleSize{2.5f};
        float darkening{0.5f};
        float glossiness{0.6f};
        float ripples{1.0f};
        float rippleTime{0.0f};
        bool debugView{false};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_WETNESS_SETTINGS_HPP
