#ifndef LOCKSTEP_RENDER_DETAIL_SETTINGS_HPP
#define LOCKSTEP_RENDER_DETAIL_SETTINGS_HPP

#include <cstdint>

// Render settings for close-up surface detail and camera lens effects, part of RenderSettings.
namespace lockstep::render
{
    // Surface relief and micro-detail of lit materials (PbrMaterialDescription::height and ::detail).
    // parallax:          parallax occlusion mapping on materials with a height map: the texture lookup
    //                    is shifted along the view ray until it meets the height field, so bricks,
    //                    tiles and cracks look recessed. Skipped in mirrors.
    // parallaxDistance:  metres from the camera where parallax has faded out (it fades from half of it);
    //                    farther surfaces use their normal maps alone.
    //                    Pixels whose relief would shift by less than a pixel on screen (head-on or
    //                    far views) skip it.
    // parallaxMinSteps:  fewest height samples of the search, 4 to 64; the count grows by one per two
    //                    pixels of on-screen shift.
    // parallaxMaxSteps:  most height samples (grazing angles up close), parallaxMinSteps to 64.
    // parallaxShadows:   the relief shadows itself from the sun where parallax runs.
    // parallaxLocalShadows: with parallaxShadows, also from point and spot lights (a height-map
    //                    march per light in range).
    // detail:            layer the generic micro-detail textures on materials that ask for one.
    // detailDistance:    metres from the camera where the detail has faded out (from half of it).
    // detailStrength:    multiplier on the detail's normal, roughness and albedo variation, 0 to 2.
    struct SurfaceDetailSettings
    {
        bool parallax{true};
        float parallaxDistance{12.0f};
        uint32_t parallaxMinSteps{6};
        uint32_t parallaxMaxSteps{24};
        bool parallaxShadows{true};
        bool parallaxLocalShadows{true};
        bool detail{true};
        float detailDistance{8.0f};
        float detailStrength{1.0f};
    };

    // Per-pixel motion blur along the scene's motion vectors (camera and object motion since last
    // frame, the buffer TAA reprojects with), after anti-aliasing. The sky has no motion vectors and
    // stays sharp.
    // enabled:        apply it.
    // intensity:      share of the frame's motion smeared, like a shutter fraction (0.5 is a
    //                 180-degree shutter); 0 to 1.
    // maxPixels:      longest smear in pixels (at 1080p; scales with the window height).
    // samples:        taps along the smear, 4 to 16.
    // viewModelScale: multiplier on the smear of first-person viewmodel pixels, 0 to 1.
    struct MotionBlurSettings
    {
        bool enabled{false};
        float intensity{0.5f};
        float maxPixels{24.0f};
        uint32_t samples{8};
        float viewModelScale{0.15f};
    };

    // Red and blue split apart towards the screen edges, like a cheap lens.
    // enabled:   apply it.
    // intensity: separation of red and blue at the corners, in pixels at 1080p.
    struct ChromaticAberrationSettings
    {
        bool enabled{false};
        float intensity{1.5f};
    };

    // Smudges and specks on the lens lit up by bloom: bright lights make the dirt glow around them.
    // Uses the Renderer's built-in procedural dirt texture. Needs bloom on.
    // enabled:   apply it.
    // intensity: how strongly bloom lights the dirt, 0 to 2.
    struct LensDirtSettings
    {
        bool enabled{false};
        float intensity{0.4f};
    };

    // Depth of field after anti-aliasing: one gather pass at full resolution with a disc kernel;
    // nearer blurry pixels spill over blurry ones behind them; sharp pixels stay sharp, as do viewmodel
    // pixels.
    // enabled:        allow it.
    // strength:       0 (none) to 1 (full); Game blends this, e.g. in with bullet-time.
    // focusDistance:  metres to the focal plane; 0 focuses on the depth at the screen centre.
    // focusTolerance: band around the focal plane that stays sharp, as a share of focusDistance.
    // blurScale:      how quickly blur grows away from the band.
    // maxRadius:      largest blur radius in pixels (at 1080p; scales with the window height).
    // samples:        taps of the disc kernel, 8 to 48.
    // preview:        apply at full strength whatever `strength` is (dev tools, for tuning).
    // apertureBlades: bokeh shape: 0 a round disc, 5 to 8 a polygon with that many blades
    //                 (6 gives hexagonal highlights).
    // apertureRotation: turn of the polygon, radians.
    // bokehHighlights: how much bright pixels outweigh dim ones in the blur, 0 (plain average) to
    //                 8, so out-of-focus lights spread into crisp bright shapes instead of greying
    //                 out.
    struct DepthOfFieldSettings
    {
        bool enabled{false};
        float strength{0.0f};
        float focusDistance{0.0f};
        float focusTolerance{0.15f};
        float blurScale{1.5f};
        float maxRadius{8.0f};
        uint32_t samples{16};
        bool preview{false};
        uint32_t apertureBlades{6};
        float apertureRotation{0.2f};
        float bokehHighlights{3.0f};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_DETAIL_SETTINGS_HPP
