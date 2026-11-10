#include "Weather/RainParticles.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace math = lockstep::math;
        namespace render = lockstep::render;
        namespace physics = lockstep::physics;

        constexpr float k_emitterHeight{6.5f};
        constexpr float k_leadSeconds{0.6f};
        constexpr float k_maxLead{6.0f};
        constexpr float k_minStretchScale{0.12f};
        constexpr float k_stretchEpsilon{1e-4f};
        constexpr float k_splashRadius{9.0f};
        constexpr float k_splashRayHeight{3.0f};
        constexpr float k_splashRayLength{10.0f};
        constexpr float k_minSplashUp{0.6f};
        constexpr float k_splashLift{0.01f};
        constexpr uint32_t k_maxSplashesPerFrame{24};
        constexpr math::Vec3 k_down{0.0f, -1.0f, 0.0f};

        float unit(RainParticles& rain)
        {
            rain.random ^= rain.random << 13;
            rain.random ^= rain.random >> 17;
            rain.random ^= rain.random << 5;
            return static_cast<float>(rain.random >> 8) * (1.0f / 16777216.0f);
        }

        bool isInside(std::span<const InteriorBox> boxes, const math::Vec3& point)
        {
            return std::any_of(boxes.begin(), boxes.end(), [&point](const InteriorBox& box)
            {
                return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y && point.y <= box.max.y
                    && point.z >= box.min.z && point.z <= box.max.z;
            });
        }

        math::Vec3 cameraLead(RainParticles& rain, const RainFrame& frame)
        {
            math::Vec3 lead{0.0f, 0.0f, 0.0f};

            if (rain.lastEye && frame.frameSeconds > 0.0f)
            {
                const math::Vec3 moved = frame.eye - *rain.lastEye;
                lead = math::Vec3{moved.x, 0.0f, moved.z} * (k_leadSeconds / frame.frameSeconds);
                const float length = math::length(lead);

                if (length > k_maxLead)
                {
                    lead = lead * (k_maxLead / length);
                }
            }

            rain.lastEye = frame.eye;
            return lead;
        }

        void updateStreaks(RainParticles& rain, render::ParticleSystem& particles, const EffectIds& ids,
            const RainFrame& frame)
        {
            if (!rain.streaks)
            {
                if (auto emitter = particles.createEmitter(ids.rainStreaks, math::Transform{}))
                {
                    rain.streaks = emitter.value();
                }
            }

            if (!rain.streaks)
            {
                return;
            }

            const math::Vec3 lead = cameraLead(rain, frame);
            particles.setEmitterTransform(*rain.streaks, math::Transform{
                .position = frame.eye + math::Vec3{0.0f, k_emitterHeight, 0.0f} + lead,
                .rotation = math::angleAxis(math::k_pi, math::Vec3{1.0f, 0.0f, 0.0f})});
            particles.setEmitterRate(*rain.streaks, frame.streaks);

            const render::ParticleEffectDescription* effect = particles.findEffect(ids.rainStreaks);
            const float stretch = rainStreakStretch() * std::clamp(frame.worldScale, k_minStretchScale, 1.0f);

            if (effect != nullptr && std::abs(stretch - rain.streakStretch) > k_stretchEpsilon)
            {
                render::ParticleEffectDescription changed = *effect;
                changed.stretch = stretch;

                if (particles.setEffect(ids.rainStreaks, changed))
                {
                    rain.streakStretch = stretch;
                }
            }
        }

        void spawnSplashes(RainParticles& rain, render::ParticleSystem& particles, const EffectIds& ids,
            const RainFrame& frame)
        {
            rain.splashCarry += frame.splashes * frame.worldSeconds;
            const auto count = std::min(static_cast<uint32_t>(std::floor(rain.splashCarry)), k_maxSplashesPerFrame);
            rain.splashCarry -= std::floor(rain.splashCarry);

            if (frame.physics == nullptr)
            {
                return;
            }

            for (uint32_t splash = 0; splash < count; ++splash)
            {
                const float angle = unit(rain) * 2.0f * math::k_pi;
                const float radius = k_splashRadius * std::sqrt(unit(rain));
                const math::Vec3 origin = frame.eye
                    + math::Vec3{std::cos(angle) * radius, k_splashRayHeight, std::sin(angle) * radius};
                const std::optional<physics::RaycastHit> hit = frame.physics->raycast(origin, k_down, k_splashRayLength);

                if (!hit || !hit->controller.isNull() || hit->normal.y < k_minSplashUp
                    || isInside(frame.interiors, hit->point))
                {
                    continue;
                }

                static_cast<void>(particles.burst(ids.rainSplash, render::ParticleBurst{
                    .position = hit->point + hit->normal * k_splashLift,
                    .direction = hit->normal,
                    .countScale = 0.6f + 0.8f * unit(rain)}));
            }
        }
    } //namespace

    void updateRainParticles(RainParticles& rain, render::ParticleSystem& particles, const EffectIds& ids,
        const RainFrame& frame)
    {
        updateStreaks(rain, particles, ids, frame);

        if (frame.splashes > 0.0f)
        {
            spawnSplashes(rain, particles, ids, frame);
        }
        else
        {
            rain.splashCarry = 0.0f;
        }
    }
} //namespace game
