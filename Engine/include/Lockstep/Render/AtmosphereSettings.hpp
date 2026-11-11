#ifndef LOCKSTEP_RENDER_ATMOSPHERE_SETTINGS_HPP
#define LOCKSTEP_RENDER_ATMOSPHERE_SETTINGS_HPP

#include "Lockstep/Core/Math.hpp"

#include <cstdint>

// Render settings for the outdoor atmosphere (wind, clouds, lightning, rain on the lens), part of
// RenderSettings. Engine only renders them; Game animates them (wind gusts, cloud drift, flashes).
namespace lockstep::render
{
    // Wind bending alpha-masked foliage (MASK materials) in the vertex shader. Only static draws
    // outside the interior ambient zones sway (potted plants indoors stay still); shadows keep the
    // rest pose. The sway grows with height above the mesh's origin (trees and bushes have it at
    // their foot), with a slow bend along the wind and a fast leaf flutter.
    // enabled:   apply it.
    // direction: horizontal direction the wind blows towards (x, z), any length.
    // speed:     wind speed in m/s including the current gust (0 still air, 6 a breeze, 15 a storm).
    // sway:      multiplier on the bend and flutter (1 natural, 0 none).
    // time:      seconds of the sway animation; the caller advances it (World time, so bullet-time
    //            slows the foliage).
    struct WindSettings
    {
        bool enabled{true};
        math::Vec2 direction{1.0f, 0.0f};
        float speed{0.0f};
        float sway{1.0f};
        float time{0.0f};
    };

    // A cloud layer in the sky: a slab between `baseHeight` and `baseHeight + thickness` above the
    // camera's ground, shaped by tiling noise and lit by the sky's sun (forward scattering gives the
    // silver lining towards the sun) and the sky. Volumetric clouds are raymarched at reduced
    // resolution in their own pass and composited over the sky; the cheap fallback draws a flat
    // noise layer with the sky. Either way the clouds dim and grey the sky's image-based light
    // by their coverage (`iblInfluence`), and the reflection tile of the environment shows the flat
    // layer, so puddles and glass reflect clouds.
    // enabled:      draw clouds.
    // volumetric:   raymarch them (else the flat layer).
    // coverage:     share of the sky covered, 0 (clear) to 1 (overcast).
    // density:      extinction multiplier; higher is darker, more solid cloud.
    // baseHeight:   metres above the camera of the cloud base.
    // thickness:    metres from base to top.
    // scale:        multiplier on the size of cloud features (1: about 2 km wide clouds).
    // offset:       horizontal noise offset in metres (x, z); the caller drifts it with the wind.
    // ambient:      linear RGB multiplier on the sky light inside clouds (grey it for rain).
    // sunLight:     multiplier on the sunlight clouds scatter.
    // silverLining: forward scattering towards the sun, 0 to 0.9.
    // iblInfluence: how much the coverage dims and greys the sky's image-based light, 0 to 1.
    // steps:        raymarch samples per pixel (volumetric), 8 to 64.
    // resolutionScale: size of the volumetric pass relative to the scene, 0.25 to 1.
    struct CloudSettings
    {
        bool enabled{true};
        bool volumetric{true};
        float coverage{0.4f};
        float density{1.0f};
        float baseHeight{1400.0f};
        float thickness{1200.0f};
        float scale{1.0f};
        math::Vec2 offset{0.0f, 0.0f};
        math::Vec3 ambient{1.0f, 1.0f, 1.0f};
        float sunLight{1.0f};
        float silverLining{0.6f};
        float iblInfluence{0.8f};
        uint32_t steps{24};
        float resolutionScale{0.5f};
    };

    // A lightning flash, animated by the caller every frame. While `flash` is above zero the sky and
    // the clouds brighten around the strike and the frame's directional light becomes the flash
    // (with cascaded shadows when `castsShadows`) once it outshines the sun; the sky keeps drawing
    // its own sun. A bolt can be drawn in the sky below the clouds.
    // flash:        current brightness, 0 (none) to about 1 (a near strike at its peak).
    // direction:    unit direction from the camera towards the strike's origin in the sky.
    // color:        linear RGB of the flash.
    // lightIntensity: directional light intensity at flash 1.
    // skyIntensity: sky radiance added around the strike at flash 1.
    // castsShadows: the flash light casts cascaded shadows.
    // bolt:         visibility of the bolt in the sky, 0 to 1.
    // boltSeed:     shape of the bolt (a new value per strike).
    struct LightningSettings
    {
        float flash{0.0f};
        math::Vec3 direction{0.0f, 1.0f, 0.0f};
        math::Vec3 color{0.78f, 0.84f, 1.0f};
        float lightIntensity{5.0f};
        float skyIntensity{2.0f};
        bool castsShadows{true};
        float bolt{0.0f};
        float boltSeed{0.0f};
    };

    // Rain drops on the camera lens (LensRain, submitted with Renderer::submitLensDroplets()),
    // drawn over the finished image after anti-aliasing and before overlays: each drop refracts a
    // blurred, flipped piece of the image behind it, with a dark rim and a small highlight.
    // enabled:    draw them.
    // refraction: how far a drop bends the image, 0 (flat glass) to 2.
    // blur:       how blurry the image inside a drop is, 0 to 1.
    // opacity:    multiplier on every drop's opacity.
    struct LensRainSettings
    {
        bool enabled{true};
        float refraction{1.0f};
        float blur{0.5f};
        float opacity{1.0f};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_ATMOSPHERE_SETTINGS_HPP
