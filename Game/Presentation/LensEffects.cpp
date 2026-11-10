#include "Presentation/LensEffects.hpp"

#include <Lockstep/Render/RenderSettings.hpp>

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        constexpr float k_bulletTimeScale{0.99f};
        constexpr float k_focusInRate{5.0f};
        constexpr float k_focusOutRate{3.0f};
    } //namespace

    void presentLensEffects(lockstep::render::RenderSettings& settings, float worldScale, float seconds)
    {
        float& strength = settings.depthOfField.strength;
        const float target = worldScale < k_bulletTimeScale ? 1.0f : 0.0f;
        const float rate = target > strength ? k_focusInRate : k_focusOutRate;
        strength += (target - strength) * (1.0f - std::exp(-rate * std::max(seconds, 0.0f)));
        strength = std::clamp(strength, 0.0f, 1.0f);
    }
} //namespace game
