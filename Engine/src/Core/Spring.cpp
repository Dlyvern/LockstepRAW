#include "Lockstep/Core/Spring.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::math
{
    namespace
    {
        constexpr float k_ln2{0.69314718f};
        constexpr float k_maxSpringStep{1.0f / 240.0f};
        constexpr int k_maxSpringSteps{64};

        float halfDamping(float halflife)
        {
            return 2.0f * k_ln2 / halflife;
        }
    } //namespace

    void decaySpring(float& value, float& velocity, float halflife, float deltaSeconds) noexcept
    {
        if (halflife <= 0.0f)
        {
            value = 0.0f;
            velocity = 0.0f;
            return;
        }

        const float damping = halfDamping(halflife);
        const float drift = velocity + value * damping;
        const float decay = std::exp(-damping * deltaSeconds);
        value = decay * (value + drift * deltaSeconds);
        velocity = decay * (velocity - drift * damping * deltaSeconds);
    }

    void decaySpring(Vec3& value, Vec3& velocity, float halflife, float deltaSeconds) noexcept
    {
        decaySpring(value.x, velocity.x, halflife, deltaSeconds);
        decaySpring(value.y, velocity.y, halflife, deltaSeconds);
        decaySpring(value.z, velocity.z, halflife, deltaSeconds);
    }

    void criticalSpring(float& value, float& velocity, float goal, float halflife, float deltaSeconds) noexcept
    {
        float offset = value - goal;
        decaySpring(offset, velocity, halflife, deltaSeconds);
        value = goal + offset;
    }

    void dampedSpring(float& value, float& velocity, float goal, float frequency, float dampingRatio,
        float deltaSeconds) noexcept
    {
        if (deltaSeconds <= 0.0f)
        {
            return;
        }

        const float omega = 2.0f * k_pi * std::max(frequency, 0.0f);
        const int steps = std::clamp(static_cast<int>(std::ceil(deltaSeconds / k_maxSpringStep)), 1, k_maxSpringSteps);
        const float step = deltaSeconds / static_cast<float>(steps);

        for (int index = 0; index < steps; ++index)
        {
            const float acceleration = omega * omega * (goal - value) - 2.0f * dampingRatio * omega * velocity;
            velocity += acceleration * step;
            value += velocity * step;
        }
    }
} //namespace lockstep::math
