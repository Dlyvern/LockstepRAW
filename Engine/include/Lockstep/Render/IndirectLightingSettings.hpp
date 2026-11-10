#ifndef LOCKSTEP_RENDER_INDIRECT_LIGHTING_SETTINGS_HPP
#define LOCKSTEP_RENDER_INDIRECT_LIGHTING_SETTINGS_HPP

#include <cstdint>

namespace lockstep::render
{
    // Screen-space reflections of opaque surfaces: rays reflected off each pixel (jittered within
    // the roughness lobe) are marched through the depth buffer at half resolution, hits fetch last
    // frame's lit scene (reprojected), and the result is reused from neighbours by roughness,
    // accumulated over frames and blended over the environment reflection by confidence. Rays
    // that leave the screen, run out, hit back faces or point at the camera fade back to the
    // environment / interior fill reflection. Glass and other transparent surfaces keep the
    // environment reflection.
    // enabled:       trace and composite them.
    // intensity:     multiplier on the reflections (1 physically based).
    // maxRoughness:  surfaces rougher than this get none (fading in over the last 30 %).
    // maxDistance:   longest ray in metres (view space).
    // steps:         raymarch samples per ray, 8 to 64, plus a short binary refinement.
    // thickness:     metres a surface in the depth buffer is assumed thick; rays passing farther
    //                behind it miss.
    // edgeFade:      share of the screen at its borders over which reflections fade out.
    // temporal:      accumulate over frames (hides the jitter noise of rough reflections).
    // historyWeight: share of last frame kept by `temporal`, 0 to 0.97.
    // debugView:     show only the screen-space reflections (reflected light times confidence,
    //                before the surface's specular weight).
    struct ScreenSpaceReflectionSettings
    {
        bool enabled{true};
        float intensity{1.0f};
        float maxRoughness{0.55f};
        float maxDistance{25.0f};
        uint32_t steps{32};
        float thickness{0.3f};
        float edgeFade{0.08f};
        bool temporal{true};
        float historyWeight{0.88f};
        bool debugView{false};
    };

    // Contact shadows: a short screen-space ray from every pixel towards the sun through the depth
    // buffer, darkening the sun's share of its light where something close blocks it. Grounds
    // small props, feet and wall bases whose shadows the shadow atlas is too coarse for. Computed
    // with the ambient occlusion at half resolution.
    // enabled:   trace them (needs the sun).
    // length:    ray length in metres.
    // thickness: metres a surface in the depth buffer is assumed thick.
    // intensity: how much of the sun's light a hit removes, 0 to 1.
    // steps:     samples per ray, 4 to 24.
    struct ContactShadowSettings
    {
        bool enabled{true};
        float length{0.35f};
        float thickness{0.12f};
        float intensity{0.85f};
        uint32_t steps{12};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_INDIRECT_LIGHTING_SETTINGS_HPP
