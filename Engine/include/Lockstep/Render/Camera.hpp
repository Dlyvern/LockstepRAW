#ifndef LOCKSTEP_RENDER_CAMERA_HPP
#define LOCKSTEP_RENDER_CAMERA_HPP

#include "Lockstep/Core/Math.hpp"

namespace lockstep::render
{
    // The point of view a frame is rendered from. The Renderer derives the view and projection
    // matrices from it, using the current window aspect ratio.
    struct Camera
    {
        math::Vec3 position{0.0f, 0.0f, 3.0f};
        math::Vec3 target{0.0f, 0.0f, 0.0f};
        math::Vec3 up{0.0f, 1.0f, 0.0f};
        float verticalFov{math::radians(60.0f)};
        float nearPlane{0.1f};
        float farPlane{1000.0f};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_CAMERA_HPP
