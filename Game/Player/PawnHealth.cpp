#include "Player/PawnHealth.hpp"

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        constexpr float k_yawSteps{256.0f};
        constexpr float k_twoPi{2.0f * math::k_pi};
    } //namespace

    PawnDamage applyPawnDamage(Pawn& pawn, int32_t damage, const math::Vec3& source, const math::Vec3& target,
        const DownedSettings& settings)
    {
        if (damage <= 0 || pawn.entity.isNull())
        {
            return PawnDamage::None;
        }

        const math::Vec3 toSource = source - target;
        pawn.damageYaw = quantizeYaw(std::atan2(-toSource.x, -toSource.z));
        ++pawn.damageCount;

        if (pawn.isDowned)
        {
            pawn.bleedSeconds = std::max(pawn.bleedSeconds - settings.downedBleedPerDamage * static_cast<float>(damage),
                0.0f);
            return pawn.bleedSeconds > 0.0f ? PawnDamage::Hurt : PawnDamage::BledOut;
        }

        pawn.health = std::max(pawn.health - damage, 0);

        if (pawn.health > 0)
        {
            return PawnDamage::Hurt;
        }

        pawn.isDowned = true;
        pawn.bleedSeconds = settings.bleedOutSeconds;
        pawn.reviveSeconds = 0.0f;
        pawn.ammo.reloadSeconds = 0.0f;

        return PawnDamage::Downed;
    }

    bool stepBleedOut(Pawn& pawn, float playerDelta)
    {
        if (!pawn.isDowned || pawn.bleedSeconds <= 0.0f)
        {
            return false;
        }

        pawn.bleedSeconds = std::max(pawn.bleedSeconds - playerDelta, 0.0f);
        return pawn.bleedSeconds <= 0.0f;
    }

    void revivePawn(Pawn& pawn, const DownedSettings& settings)
    {
        pawn.isDowned = false;
        pawn.bleedSeconds = 0.0f;
        pawn.reviveSeconds = 0.0f;
        pawn.health = settings.reviveHealth;
    }

    void clearDowned(Pawn& pawn)
    {
        pawn.isDowned = false;
        pawn.bleedSeconds = 0.0f;
        pawn.reviveSeconds = 0.0f;
    }

    uint8_t quantizeYaw(float yaw)
    {
        const float turns = std::remainder(yaw, k_twoPi) / k_twoPi;
        const auto step = static_cast<int32_t>(std::lround(turns * k_yawSteps));

        return static_cast<uint8_t>(step & 0xFF);
    }

    float dequantizeYaw(uint8_t yaw)
    {
        return std::remainder(static_cast<float>(yaw) / k_yawSteps * k_twoPi, k_twoPi);
    }
} //namespace game
