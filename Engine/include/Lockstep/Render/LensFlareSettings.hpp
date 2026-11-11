#ifndef LOCKSTEP_RENDER_LENS_FLARE_SETTINGS_HPP
#define LOCKSTEP_RENDER_LENS_FLARE_SETTINGS_HPP

namespace lockstep::render
{
    // Lens flares and sun glare, part of RenderSettings. Rendered at quarter resolution from the
    // bloom chain and added to the bloom result, so they need bloom on (and light the lens dirt).
    // Ghosts: pixels much brighter than the current exposure (lamps, the sun, a muzzle flash) are
    // mirrored through the screen centre at several scales as tinted, defocused ghosts, with a
    // faint rainbow halo ring. Viewmodel pixels never flare.
    // Sun glare: a soft veil and a faint six-ray starburst around the sun while it is on screen,
    // faded by how much of the sun the depth buffer shows (walls and window frames hide it), and
    // stronger when the sun is low.
    // enabled:          draw ghosts and the halo.
    // intensity:        brightness of the ghosts, 0 to 4.
    // threshold:        exposed brightness (1 = white after exposure) a pixel must exceed to flare.
    // haloIntensity:    brightness of the halo ring, 0 to 4.
    // haloRadius:       radius of the halo ring, as a share of the screen height.
    // chromaticShift:   red/blue separation of the halo, as a share of the screen height.
    // sunGlare:         draw the sun glare.
    // sunGlareIntensity: brightness of the sun glare, 0 to 4.
    // sunsetBoost:      extra glare when the sun is near the horizon (0 none, 3 four times as much).
    // starburst:        share of the sun glare that goes into the starburst rays, 0 to 1.
    struct LensFlareSettings
    {
        bool enabled{true};
        float intensity{1.0f};
        float threshold{2.5f};
        float haloIntensity{0.6f};
        float haloRadius{0.42f};
        float chromaticShift{0.008f};
        bool sunGlare{true};
        float sunGlareIntensity{1.0f};
        float sunsetBoost{1.5f};
        float starburst{0.35f};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_LENS_FLARE_SETTINGS_HPP
