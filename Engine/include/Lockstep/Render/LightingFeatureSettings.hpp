#ifndef LOCKSTEP_RENDER_LIGHTING_FEATURE_SETTINGS_HPP
#define LOCKSTEP_RENDER_LIGHTING_FEATURE_SETTINGS_HPP

#include <cstdint>

namespace lockstep::render
{
    // Shapes of local lights beyond a point (see PointLight::tubeLength, LightProfile).
    // tubeLights: shade lights with a tube length as lines (long highlights, diffuse integrated
    //             over the line); off treats them as points at their centre.
    // profiles:   apply emission profiles (LightProfile) to point and spot lights.
    struct LightShapeSettings
    {
        bool tubeLights{true};
        bool profiles{true};
    };

    // Contact-hardening soft shadows (percentage-closer soft shadows): a blocker search finds how
    // far the caster is from the receiver, and the filter widens with that distance, so shadows
    // are sharp where an object touches the ground and soften away from it.
    // enabled:       use them; off is the fixed-size filter.
    // sunSize:       penumbra width in metres per metre between caster and receiver for the sun
    //                (its angular diameter; 0.0093 is the real sun, larger for overcast softness).
    // localRadius:   radius in metres of the light source of point and spot lights (tube lights use
    //                at least a quarter of their length).
    // blockerTaps:   blocker search samples, 4 to 32.
    // filterTaps:    filter samples, 4 to 32.
    // maxPenumbraTexels: widest filter in shadow texels (bounds cost and light bleeding).
    struct SoftShadowSettings
    {
        bool enabled{true};
        float sunSize{0.02f};
        float localRadius{0.08f};
        uint32_t blockerTaps{8};
        uint32_t filterTaps{12};
        float maxPenumbraTexels{12.0f};
    };

    // Sampling of the baked irradiance volume set with Renderer::setIrradianceVolume() (bounce
    // light of a static level).
    // enabled:       light surfaces inside the volume with it.
    // intensity:     multiplier on the baked bounce light.
    // skyVisibility: how strongly the baked sky visibility darkens the sky's diffuse light inside
    //                the volume, 0 (not at all) to 1; it replaces the interior zones' dimming there.
    // fillKept:      share of the interior zones' fill light kept inside the volume (the bounce
    //                light replaces most of it).
    // normalOffset:  metres the lookup moves along the surface normal (less light leaking through
    //                walls).
    // fadeDistance:  metres inside the volume's faces over which it fades in.
    // debugView:     show only the baked bounce light on white surfaces.
    struct IrradianceVolumeSettings
    {
        bool enabled{true};
        float intensity{1.0f};
        float skyVisibility{1.0f};
        float fillKept{0.5f};
        float normalOffset{0.35f};
        float fadeDistance{1.0f};
        bool debugView{false};
    };

    // Shadow map caster limits beyond culling by light reach.
    // minCasterTexels: a static caster whose bounding sphere covers fewer shadow texels than this
    //                  (diameter) in a tile is not drawn into it; 0 draws every caster.
    struct ShadowDetailSettings
    {
        float minCasterTexels{2.0f};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_LIGHTING_FEATURE_SETTINGS_HPP
