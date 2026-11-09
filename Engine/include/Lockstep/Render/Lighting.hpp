#ifndef LOCKSTEP_RENDER_LIGHTING_HPP
#define LOCKSTEP_RENDER_LIGHTING_HPP

#include "Lockstep/Core/Math.hpp"

#include <cstdint>

namespace lockstep::render
{
    // Most point and spot lights one frame can use, together. Lights submitted past this are dropped.
    inline constexpr uint32_t k_maxLocalLights{16};

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
    struct PointLight
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Vec3 color{1.0f, 1.0f, 1.0f};
        float intensity{10.0f};
        float range{10.0f};
    };

    // A cone of light from a point (a flashlight, a ceiling spot).
    // position:     world position.
    // direction:    unit direction the cone points in.
    // color:        linear RGB colour.
    // intensity:    brightness multiplier on `color`. Falls off with the inverse square of distance.
    // range:        distance at which the light fades out completely, in metres.
    // innerAngle:   half-angle of the fully lit core, in radians.
    // outerAngle:   half-angle where the cone has faded to nothing, in radians. Must be > innerAngle.
    // castsShadows: true to cast a shadow map. Only the first such spot light of a frame does; it
    //               takes the last tile of the shadow atlas, leaving the sun one cascade fewer.
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
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_LIGHTING_HPP
