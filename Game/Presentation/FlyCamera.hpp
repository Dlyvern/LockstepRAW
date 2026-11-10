#ifndef LOCKSTEP_GAME_PRESENTATION_FLY_CAMERA_HPP
#define LOCKSTEP_GAME_PRESENTATION_FLY_CAMERA_HPP

#include <optional>
#include <span>
#include "Player/PlayerCommand.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Platform/Input.hpp>
#include <Lockstep/Render/Camera.hpp>

namespace game
{
    namespace math = lockstep::math;

    // Free-flying dev camera for inspecting levels, toggled with F7. Presentation only: while it
    // is active the local pawn gets idle input and stands still, and nothing is replicated.
    // isActive: the camera replaces the first-person view.
    // position: eye position.
    // yaw:      look angle around the up axis, 0 looks along -z.
    // pitch:    look angle up (positive) or down.
    // speed:    metres per second; the mouse wheel scales it.
    struct FlyCamera
    {
        bool isActive{false};
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        float yaw{0.0f};
        float pitch{0.0f};
        float speed{6.0f};
    };

    // Toggles the camera on F7 (V kicks). Turning it on starts it at the current view.
    // camera: the fly camera.
    // input:  this frame's input.
    // eye:    the current view's eye position.
    // yaw:    the current view's yaw.
    // pitch:  the current view's pitch.
    void toggleFlyCamera(FlyCamera& camera, const lockstep::platform::Input& input, const math::Vec3& eye, float yaw,
        float pitch);

    // Moves an active camera: mouse to look (while the cursor is captured), WASD to move along
    // the view, E/Q up and down, Shift for 4x speed, mouse wheel to change speed.
    // camera:          the fly camera.
    // input:           this frame's input.
    // settings:        mouse sensitivity and pitch limit.
    // isCursorCaptured: true while the window holds the mouse.
    // frameSeconds:    wall time of the frame (presentation only).
    void updateFlyCamera(FlyCamera& camera, const lockstep::platform::Input& input, const ControlSettings& settings,
        bool isCursorCaptured, float frameSeconds);

    // Dev (`--dev-camera x,y,z,yaw,pitch`): a fixed fly camera pose (radians; yaw 0 looks along -z)
    // held every frame, for reproducible render measurements.
    // arguments: the command line.
    // Returns:   the active camera at that pose, or nothing when the flag is absent or malformed.
    [[nodiscard]] std::optional<FlyCamera> devFixedCamera(std::span<const char* const> arguments);

    // camera:  the fly camera.
    // Returns: the render camera looking from it.
    [[nodiscard]] lockstep::render::Camera flyCameraView(const FlyCamera& camera);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_FLY_CAMERA_HPP
