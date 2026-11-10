#include "Weapons/FireModes.hpp"

#include <algorithm>

namespace game
{
    namespace
    {
        constexpr float k_secondsPerMinute{60.0f};
        constexpr float k_minRoundsPerMinute{60.0f};
        constexpr float k_cooldownEpsilon{1e-4f};
    } //namespace

    float shotInterval(const FireModeSettings& settings)
    {
        return k_secondsPerMinute / std::max(settings.roundsPerMinute, k_minRoundsPerMinute);
    }

    FireMode nextFireMode(FireMode mode)
    {
        switch (mode)
        {
            case FireMode::Semi:
                return FireMode::Burst;
            case FireMode::Burst:
                return FireMode::Auto;
            case FireMode::Auto:
                return FireMode::Semi;
        }

        return FireMode::Semi;
    }

    const char* fireModeName(FireMode mode)
    {
        switch (mode)
        {
            case FireMode::Semi:
                return "SEMI";
            case FireMode::Burst:
                return "BURST";
            case FireMode::Auto:
                return "AUTO";
        }

        return "?";
    }

    void syncTriggerRounds(Trigger& trigger, int32_t magazine, bool isReloading)
    {
        if (trigger.seenMagazine < 0 || magazine > trigger.seenMagazine || isReloading)
        {
            trigger.rounds = isReloading ? 0 : magazine;
        }
        else
        {
            trigger.rounds = std::min(trigger.rounds, magazine);
        }

        trigger.seenMagazine = magazine;
    }

    uint8_t stepTrigger(Trigger& trigger, const FireModeSettings& settings, const TriggerInput& input)
    {
        trigger.cooldown = std::max(trigger.cooldown - input.deltaSeconds, 0.0f);

        if (!input.canFire)
        {
            trigger.queued = 0;
            return 0;
        }

        switch (trigger.mode)
        {
            case FireMode::Semi:
                trigger.queued = input.isPressed ? uint8_t{1} : trigger.queued;
                break;
            case FireMode::Burst:
                trigger.queued = input.isPressed && trigger.queued == 0 ? settings.burstLength : trigger.queued;
                break;
            case FireMode::Auto:
                trigger.queued = input.isHeld || input.isPressed ? uint8_t{1} : uint8_t{0};
                break;
        }

        if (trigger.rounds <= 0)
        {
            trigger.queued = 0;
            return 0;
        }

        if (trigger.queued == 0 || trigger.cooldown > k_cooldownEpsilon)
        {
            return 0;
        }

        --trigger.queued;
        --trigger.rounds;
        trigger.cooldown += shotInterval(settings);

        if (trigger.mode == FireMode::Burst && trigger.queued == 0)
        {
            trigger.cooldown += settings.burstPause;
        }

        if (trigger.mode == FireMode::Auto)
        {
            trigger.queued = 0;
        }

        return 1;
    }

    bool acceptShotCadence(float& cooldown, const FireModeSettings& settings)
    {
        const float interval = shotInterval(settings);

        if (cooldown > interval * settings.rateTolerance)
        {
            return false;
        }

        cooldown = std::max(cooldown, 0.0f) + interval;
        return true;
    }
} //namespace game
