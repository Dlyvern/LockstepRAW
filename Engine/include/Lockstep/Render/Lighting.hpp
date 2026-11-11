#ifndef LOCKSTEP_RENDER_LIGHTING_HPP
#define LOCKSTEP_RENDER_LIGHTING_HPP

#include "Lockstep/Core/Bounds.hpp"
#include "Lockstep/Core/Math.hpp"

#include <cstdint>

namespace lockstep::render
{
    // Most point and spot lights one frame can use, together. Lights submitted past this are dropped.
    inline constexpr uint32_t k_maxLocalLights{16};

    // Most ambient zones one frame shades with; the renderer keeps the ones nearest the camera.
    inline constexpr uint32_t k_maxFrameAmbientZones{16};

    // An emission profile shaping how much light a point or spot light sends in each direction
    // around its axis, like a photometric (IES) profile or a cookie: the light's intensity is the
    // peak, the profile scales it down elsewhere. Engine-generated, see LightProfileSettings.
    // None:     emits evenly (or the plain spot cone).
    // Downlight: a recessed ceiling can: most light within ~50 degrees of the axis, a soft edge,
    //           a little spill to the sides and none behind.
    // WallWash: an asymmetric wash: thrown to the +X side of the light's frame (towards a wall),
    //           little to the other side.
    // Grille:   a caged or louvred fixture: the bars of a grille cast stripes and a ring pattern.
    // Fluorescent: a bare tube in a shallow reflector: wide batwing spread, darker straight
    //           below and dim above.
    enum class LightProfile : uint8_t
    {
        None = 0,
        Downlight,
        WallWash,
        Grille,
        Fluorescent
    };

    // Number of LightProfile values other than None.
    inline constexpr uint32_t k_lightProfileCount{4};

    // A light infinitely far away, lighting everything from one direction: the sun. One per frame;
    // when Game submits none, the sky's sun is used (see SkySettings).
    // direction:    unit direction the light travels in (from the light towards the scene).
    // color:        linear RGB colour.
    // intensity:    brightness multiplier on `color`; 1 lights a white surface facing it to 1.
    // castsShadows: true to cast cascaded shadow maps.
    struct DirectionalLight
    {
        math::Vec3 direction{0.0f, -1.0f, 0.0f};
        math::Vec3 color{1.0f, 1.0f, 1.0f};
        float intensity{1.0f};
        bool castsShadows{false};
    };

    // A light shining in every direction from a point (a bulb, a lamp).
    // position:  world position.
    // color:     linear RGB colour.
    // intensity: brightness multiplier on `color`. Falls off with the inverse square of distance.
    // range:     distance at which the light fades out completely, in metres.
    // castsShadows: true to cast a shadow (six shadow atlas tiles, one per cube face) when it is
    //              among the nearest shadow casters this frame (see ShadowSettings::maxLocalLights).
    // volumetric:  multiplier on the light it scatters in volumetric fog; 0 for none (a muzzle
    //              flash, a light at the camera that would wash out the view).
    // tubeLength:  0 for a point; above 0 the light is a line (a fluorescent tube) this many metres
    //              long, centred on `position` along `tubeAxis`, with the same total intensity
    //              (diffuse integrated over the line, specular highlights stretched along it).
    //              Its shadow is cast from the centre.
    // tubeAxis:    unit direction of the tube.
    // profile:     emission profile around `profileAxis` (LightProfile::None for even light).
    // profileAxis: unit direction the profile's main axis points in (straight down for a ceiling
    //              fixture).
    // profileSide: unit direction perpendicular to `profileAxis` giving the profile's +X side
    //              (where a wall wash throws its light, along which grille bars run).
    struct PointLight
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Vec3 color{1.0f, 1.0f, 1.0f};
        float intensity{10.0f};
        float range{10.0f};
        bool castsShadows{false};
        float volumetric{1.0f};
        float tubeLength{0.0f};
        math::Vec3 tubeAxis{1.0f, 0.0f, 0.0f};
        LightProfile profile{LightProfile::None};
        math::Vec3 profileAxis{0.0f, -1.0f, 0.0f};
        math::Vec3 profileSide{1.0f, 0.0f, 0.0f};
    };

    // An axis-aligned world box whose inside is sheltered from the sky (a building interior): lit
    // surfaces in it get the sky's image-based light scaled down and a fill light added instead,
    // as set by InteriorAmbientSettings, fading in over a blend distance inside its faces. Surfaces
    // outside every zone (a street seen through a door) keep the full sky.
    // bounds: the box in world space; cover the outer faces of the walls around the interior.
    struct AmbientZone
    {
        math::Aabb bounds{};
    };

    // A cone of light from a point (a flashlight, a ceiling spot).
    // position:     world position.
    // direction:    unit direction the cone points in.
    // color:        linear RGB colour.
    // intensity:    brightness multiplier on `color`. Falls off with the inverse square of distance.
    // range:        distance at which the light fades out completely, in metres.
    // innerAngle:   half-angle of the fully lit core, in radians.
    // outerAngle:   half-angle where the cone has faded to nothing, in radians. Must be > innerAngle.
    // castsShadows: true to cast a shadow (one shadow atlas tile) when it is among the nearest
    //               shadow casters this frame (see ShadowSettings::maxLocalLights).
    // volumetric:   multiplier on the light it scatters in volumetric fog; 0 for none (the local
    //               player's own flashlight, if its cone fills the view too much).
    // profile:      emission profile around `direction`, on top of the cone (a cookie).
    // profileSide:  unit direction perpendicular to `direction` giving the profile's +X side.
    struct SpotLight
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
        math::Vec3 color{1.0f, 1.0f, 1.0f};
        float intensity{20.0f};
        float range{20.0f};
        float innerAngle{math::radians(12.0f)};
        float outerAngle{math::radians(25.0f)};
        bool castsShadows{false};
        float volumetric{1.0f};
        LightProfile profile{LightProfile::None};
        math::Vec3 profileSide{1.0f, 0.0f, 0.0f};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_LIGHTING_HPP
