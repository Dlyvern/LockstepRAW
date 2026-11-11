#include "Weather/LensDrops.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace math = lockstep::math;

        constexpr float k_roofTestStart{0.4f};
        constexpr float k_roofTestLength{40.0f};
        constexpr float k_windReference{10.0f};
        constexpr float k_driftPerWind{0.0015f};
        constexpr float k_levelShare{0.55f};
        constexpr float k_downShare{0.08f};

        bool isInside(std::span<const InteriorBox> boxes, const math::Vec3& point)
        {
            return std::any_of(boxes.begin(), boxes.end(), [&point](const InteriorBox& box)
            {
                return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y && point.y <= box.max.y
                    && point.z >= box.min.z && point.z <= box.max.z;
            });
        }

        bool isUnderOpenSky(const LensDropFrame& frame)
        {
            if (isInside(frame.interiors, frame.eye))
            {
                return false;
            }

            if (frame.physics == nullptr)
            {
                return true;
            }

            const math::Vec3 start = frame.eye + math::Vec3{0.0f, k_roofTestStart, 0.0f};
            const auto hit = frame.physics->raycast(start, math::Vec3{0.0f, 1.0f, 0.0f}, k_roofTestLength);
            return !hit || !hit->controller.isNull();
        }

        float smoothstep(float edge0, float edge1, float value)
        {
            const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        float viewFactor(const LensDropTuning& tuning, const LensDropFrame& frame)
        {
            const float up = frame.forward.y;
            const float level = k_downShare + (k_levelShare - k_downShare) * smoothstep(-0.7f, 0.0f, up);
            const float lookingUp = tuning.lookUpBoost * smoothstep(0.1f, 0.9f, up);
            const math::Vec3 flat{frame.forward.x, 0.0f, frame.forward.z};
            const float facing = math::length(flat) > 1e-3f ? -math::dot(math::normalize(flat), frame.wind) : 0.0f;
            const float intoWind = tuning.windBoost * std::max(facing, 0.0f) / k_windReference;

            return level + lookingUp + intoWind;
        }

        math::Vec2 screenDrift(const LensDropFrame& frame)
        {
            const math::Vec3 right = math::normalize(math::cross(frame.forward, math::Vec3{0.0f, 1.0f, 0.0f}));
            return math::Vec2{math::dot(right, frame.wind) * k_driftPerWind, 0.0f};
        }
    } //namespace

    void updateLensDrops(LensDrops& lens, const LensDropTuning& tuning, const LensDropFrame& frame,
        lockstep::render::Renderer& renderer)
    {
        if (!tuning.enabled || !renderer.graphicsQuality().lensRain)
        {
            lens.drops.clear();
            lens.exposure = 0.0f;
            return;
        }

        lens.isOpen = frame.rain > 0.0f && isUnderOpenSky(frame);
        const float target = lens.isOpen ? 1.0f : 0.0f;
        const float step = tuning.exposureSeconds > 0.0f ? frame.seconds / tuning.exposureSeconds : 1.0f;
        lens.exposure += std::clamp(target - lens.exposure, -step, step);

        const float up = std::max(frame.forward.y, 0.0f);
        lens.drops.update(lockstep::render::LensRainFrame{
            .seconds = frame.seconds,
            .landRate = tuning.rate * frame.rain * lens.exposure * viewFactor(tuning, frame),
            .gravity = 1.0f - 0.85f * up,
            .evaporation = 1.0f + (std::max(tuning.insideDrying, 1.0f) - 1.0f) * (1.0f - lens.exposure),
            .aspectRatio = frame.aspectRatio,
            .drift = screenDrift(frame)});
        lens.drops.submit(renderer);
    }
} //namespace game
