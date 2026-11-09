#include "Presentation/FlyCamera.hpp"

#include "Player/Pawns.hpp"
#include "Presentation/Visuals.hpp"

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace platform = lockstep::platform;

        constexpr float k_fastMultiplier{4.0f};
        constexpr float k_speedStep{1.2f};
        constexpr float k_minSpeed{0.5f};
        constexpr float k_maxSpeed{200.0f};

        float axis(const platform::Input& input, platform::Key positive, platform::Key negative)
        {
            return (input.isKeyDown(positive) ? 1.0f : 0.0f) - (input.isKeyDown(negative) ? 1.0f : 0.0f);
        }
    } //namespace

    void toggleFlyCamera(FlyCamera& camera, const platform::Input& input, const math::Vec3& eye, float yaw, float pitch)
    {
        if (!input.wasKeyPressed(platform::Key::V))
        {
            return;
        }

        camera.isActive = !camera.isActive;

        if (camera.isActive)
        {
            camera.position = eye;
            camera.yaw = yaw;
            camera.pitch = pitch;
        }
    }

    void updateFlyCamera(FlyCamera& camera, const platform::Input& input, const ControlSettings& settings,
        bool isCursorCaptured, float frameSeconds)
    {
        if (isCursorCaptured)
        {
            const math::Vec2 delta = input.mouseDelta();
            camera.yaw = std::remainder(camera.yaw - delta.x * settings.mouseSensitivity, 2.0f * math::k_pi);
            camera.pitch = math::clamp(camera.pitch - delta.y * settings.mouseSensitivity, -settings.maxPitch,
                settings.maxPitch);
        }

        const float scroll = input.scrollDelta().y;

        if (scroll != 0.0f)
        {
            camera.speed = std::clamp(camera.speed * std::pow(k_speedStep, scroll), k_minSpeed, k_maxSpeed);
        }

        const math::Vec3 forward = lookDirection(camera.yaw, camera.pitch);
        const math::Vec3 right = math::normalize(math::cross(forward, math::Vec3{0.0f, 1.0f, 0.0f}));
        const math::Vec3 up{0.0f, 1.0f, 0.0f};
        const math::Vec3 move = forward * axis(input, platform::Key::W, platform::Key::S)
            + right * axis(input, platform::Key::D, platform::Key::A) + up * axis(input, platform::Key::E, platform::Key::Q);
        const float speed = camera.speed * (input.isKeyDown(platform::Key::LeftShift) ? k_fastMultiplier : 1.0f);

        if (math::lengthSquared(move) > 0.0f)
        {
            camera.position += math::normalize(move) * speed * frameSeconds;
        }
    }

    lockstep::render::Camera flyCameraView(const FlyCamera& camera)
    {
        return firstPersonCamera(camera.position, lookDirection(camera.yaw, camera.pitch), 0.0f);
    }
} //namespace game
