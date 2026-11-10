#include "Weapons/Recoil.hpp"

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
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
    } //namespace

    void kickRecoil(RecoilState& recoil, const RecoilSettings& settings, bool isAiming, bool isCrouched)
    {
        recoil.streak = recoil.sinceShot > settings.streakSeconds ? 1 : recoil.streak + 1;
        recoil.sinceShot = 0.0f;

        const float stance = (isAiming ? settings.aimScale : 1.0f) * (isCrouched ? settings.crouchScale : 1.0f);
        const float sustained = std::min(1.0f + settings.build * static_cast<float>(recoil.streak - 1),
            std::max(settings.maxBuild, 1.0f));
        const float build = recoil.streak == 1 ? settings.firstShotScale : sustained;
        const float horizontal = recoil.offset.x + recoil.pending.x;
        float step = signedUnit(recoil.random) * settings.wander - horizontal * settings.wanderPull;

        if (std::abs(horizontal + step) > settings.wanderLimit)
        {
            step = -step;
        }

        recoil.pending = recoil.pending + math::Vec2{step, settings.climb * build} * stance;
    }

    math::Vec2 stepRecoil(RecoilState& recoil, const RecoilSettings& settings, float deltaSeconds)
    {
        recoil.sinceShot += deltaSeconds;
        const float apply = settings.kickSeconds > 0.0f ? std::min(deltaSeconds / settings.kickSeconds, 1.0f) : 1.0f;
        const math::Vec2 applied = recoil.pending * apply;
        recoil.pending = recoil.pending - applied;
        recoil.offset = recoil.offset + applied;

        if (recoil.sinceShot < settings.recoverDelay)
        {
            return math::Vec2{0.0f, 0.0f};
        }

        const float settle = 1.0f - std::exp(-settings.recoverRate * deltaSeconds);
        const math::Vec2 settled = recoil.offset * settle;
        recoil.offset = recoil.offset - settled;

        return settled * (1.0f - std::clamp(settings.recoverAmount, 0.0f, 1.0f));
    }
} //namespace game
