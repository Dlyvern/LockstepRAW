#include "Characters/WeaponSway.hpp"

#include "Presentation/Springs.hpp"

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        constexpr float k_rollBias{0.7f};
        constexpr float k_trembleRate{9.0f};
        constexpr float k_minRecovery{0.6f};

        uint32_t nextRandom(uint32_t& state)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        float signedUnit(uint32_t& state)
        {
            return static_cast<float>(nextRandom(state) % 20001u) / 10000.0f - 1.0f;
        }

        float follow(float value, float target, float rate, float seconds)
        {
            return target + (value - target) * std::exp(-rate * seconds);
        }

        float wrapAngle(float angle)
        {
            return std::remainder(angle, 2.0f * math::k_pi);
        }

        void updateHold(BreathState& breath, const BreathTuning& tuning, const BreathInput& input)
        {
            breath.recovery = std::max(breath.recovery - input.seconds, 0.0f);
            const bool canStart = breath.recovery <= 0.0f && input.aimed > 0.5f;

            if (input.wantsHold && !breath.isHolding && canStart)
            {
                breath.isHolding = true;
                breath.heldSeconds = 0.0f;
                ++breath.holds;
            }
            else if (breath.isHolding && (!input.wantsHold || input.aimed < 0.5f))
            {
                const float spent = tuning.holdSeconds > 0.0f ? std::min(breath.heldSeconds / tuning.holdSeconds, 1.0f)
                                                              : 1.0f;
                breath.isHolding = false;
                breath.recovery = std::max(tuning.recoverSeconds * spent, k_minRecovery);
                breath.stamina = std::max(breath.stamina - tuning.holdStamina * spent, 0.0f);
                ++breath.releases;
            }

            if (breath.isHolding)
            {
                breath.heldSeconds += input.seconds;
            }

            const bool isSpent = breath.isHolding && breath.heldSeconds >= tuning.holdSeconds;
            const bool isSteady = breath.isHolding && !isSpent;
            breath.steadiness = follow(breath.steadiness, isSteady ? 1.0f : 0.0f, tuning.holdBlendRate, input.seconds);
            breath.shake = follow(breath.shake, isSpent ? 1.0f : 0.0f, tuning.holdBlendRate, input.seconds);
        }
    } //namespace

    void kickWeapon(WeaponKickState& kick, const WeaponKickTuning& tuning, bool isAiming, bool isCrouched)
    {
        kick.streak = kick.sinceShot > tuning.streakSeconds ? 1 : kick.streak + 1;
        kick.sinceShot = 0.0f;

        const float sustained = std::min(1.0f + tuning.build * static_cast<float>(kick.streak - 1),
            std::max(tuning.maxBuild, 1.0f));
        const float build = kick.streak == 1 ? tuning.firstShotScale : sustained;
        const float scale = build * (isAiming ? tuning.aimScale : 1.0f) * (isCrouched ? tuning.crouchScale : 1.0f);
        const float roll = (k_rollBias + (1.0f - k_rollBias) * signedUnit(kick.random)) * tuning.roll;
        kick.rate = kick.rate + math::Vec3{tuning.climb, signedUnit(kick.random) * tuning.yaw, -roll} * scale;
        kick.backRate += tuning.back * scale;
    }

    void stepWeaponKick(WeaponKickState& kick, const WeaponKickTuning& tuning, float seconds)
    {
        kick.sinceShot += seconds;
        math::Vec2 aim{kick.angle.x, kick.angle.y};
        math::Vec2 aimRate{kick.rate.x, kick.rate.y};
        stepSpring(aim, aimRate, tuning.stiffness, tuning.damping, seconds);
        stepSpring(kick.angle.z, kick.rate.z, tuning.rollStiffness, tuning.rollDamping, seconds);
        stepSpring(kick.back, kick.backRate, tuning.backStiffness, tuning.backDamping, seconds);
        kick.angle = math::Vec3{std::clamp(aim.x, -tuning.maxAngle, tuning.maxAngle),
            std::clamp(aim.y, -tuning.maxAngle, tuning.maxAngle), std::clamp(kick.angle.z, -tuning.maxAngle,
                tuning.maxAngle)};
        kick.rate = math::Vec3{aimRate.x, aimRate.y, kick.rate.z};
        kick.back = std::clamp(kick.back, -tuning.maxBack, tuning.maxBack);
    }

    math::Vec2 stepBreath(BreathState& breath, const BreathTuning& tuning, const BreathInput& input)
    {
        const float drain = input.isSprinting ? -tuning.sprintDrain : tuning.recoverRate;
        breath.stamina = std::clamp(breath.stamina + drain * input.seconds, 0.0f, 1.0f);
        updateHold(breath, tuning, input);

        const float exhaustion = 1.0f - breath.stamina;
        const float rate = tuning.rate + (tuning.exhaustedRate - tuning.rate) * exhaustion;
        const float held = 1.0f + (tuning.holdScale - 1.0f) * breath.steadiness;
        breath.phase = wrapAngle(breath.phase + 2.0f * math::k_pi * rate * input.seconds * held);
        breath.tremblePhase = wrapAngle(breath.tremblePhase + 2.0f * math::k_pi * k_trembleRate * input.seconds);

        const float base = tuning.hipAmount + (tuning.aimAmount - tuning.hipAmount) * input.aimed;
        const float amount = base * (1.0f + (tuning.exhaustedScale - 1.0f) * exhaustion) * held
            * (1.0f + (tuning.shakeScale - 1.0f) * breath.shake);
        const float tremble = tuning.tremble * breath.shake;

        return math::Vec2{std::sin(breath.phase) * amount * 0.6f + std::sin(breath.tremblePhase * 1.3f) * tremble,
            std::sin(2.0f * breath.phase + 0.7f) * amount + std::sin(breath.tremblePhase + 1.1f) * tremble};
    }

#if LOCKSTEP_DEV_TOOLS
    void weaponSwayDevSection(WeaponKickTuning& kick, BreathTuning& breath, const BreathState& breathState)
    {
        if (ImGui::TreeNode("Weapon kick"))
        {
            ImGui::SliderFloat("Climb (rad/s)", &kick.climb, 0.0f, 4.0f);
            ImGui::SliderFloat("Sideways (rad/s)", &kick.yaw, 0.0f, 2.0f);
            ImGui::SliderFloat("Roll (rad/s)", &kick.roll, 0.0f, 4.0f);
            ImGui::SliderFloat("Back (m/s)", &kick.back, 0.0f, 3.0f);
            ImGui::SliderFloat("First shot x", &kick.firstShotScale, 0.5f, 3.0f);
            ImGui::SliderFloat("Build per shot", &kick.build, 0.0f, 0.2f);
            ImGui::SliderFloat("Max build x", &kick.maxBuild, 1.0f, 3.0f);
            ImGui::SliderFloat("String ends after (s)", &kick.streakSeconds, 0.05f, 1.0f);
            ImGui::SliderFloat("Stiffness", &kick.stiffness, 50.0f, 800.0f);
            ImGui::SliderFloat("Damping", &kick.damping, 1.0f, 60.0f);
            ImGui::SliderFloat("Roll stiffness", &kick.rollStiffness, 50.0f, 800.0f);
            ImGui::SliderFloat("Roll damping", &kick.rollDamping, 1.0f, 60.0f);
            ImGui::SliderFloat("Back stiffness", &kick.backStiffness, 50.0f, 1200.0f);
            ImGui::SliderFloat("Back damping", &kick.backDamping, 1.0f, 80.0f);
            ImGui::SliderFloat("Aiming x", &kick.aimScale, 0.0f, 1.0f);
            ImGui::SliderFloat("Crouched x", &kick.crouchScale, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Breathing"))
        {
            ImGui::Text("Stamina %.2f  %s  held %.1f s  recovery %.1f s  steady %.2f  shake %.2f", breathState.stamina,
                breathState.isHolding ? "holding" : "breathing", breathState.heldSeconds, breathState.recovery,
                breathState.steadiness, breathState.shake);
            ImGui::SliderFloat("Hip sway (rad)", &breath.hipAmount, 0.0f, 0.02f);
            ImGui::SliderFloat("Aim sway (rad)", &breath.aimAmount, 0.0f, 0.02f);
            ImGui::SliderFloat("Rate (Hz)", &breath.rate, 0.05f, 1.0f);
            ImGui::SliderFloat("Exhausted rate (Hz)", &breath.exhaustedRate, 0.05f, 2.0f);
            ImGui::SliderFloat("Exhausted sway x", &breath.exhaustedScale, 1.0f, 8.0f);
            ImGui::SliderFloat("Sprint drain (/s)", &breath.sprintDrain, 0.0f, 1.0f);
            ImGui::SliderFloat("Stamina recovery (/s)", &breath.recoverRate, 0.0f, 1.0f);
            ImGui::SliderFloat("Hold length (s)", &breath.holdSeconds, 0.5f, 8.0f);
            ImGui::SliderFloat("Held sway x", &breath.holdScale, 0.0f, 1.0f);
            ImGui::SliderFloat("Hold blend rate", &breath.holdBlendRate, 1.0f, 20.0f);
            ImGui::SliderFloat("Spent sway x", &breath.shakeScale, 1.0f, 5.0f);
            ImGui::SliderFloat("Spent tremble (rad)", &breath.tremble, 0.0f, 0.01f);
            ImGui::SliderFloat("Hold recovery (s)", &breath.recoverSeconds, 0.0f, 8.0f);
            ImGui::SliderFloat("Hold stamina cost", &breath.holdStamina, 0.0f, 1.0f);
            ImGui::TreePop();
        }
    }
#endif
} //namespace game
