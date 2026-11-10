#include "Weapons/Ammo.hpp"

#include <algorithm>

namespace game
{
    bool isReloading(const Ammo& ammo)
    {
        return ammo.reloadSeconds > 0.0f;
    }

    void issueAmmo(Ammo& ammo, const AmmoSettings& settings)
    {
        ammo.magazine = settings.magazineSize;
        ammo.reserve = std::max(ammo.reserve, settings.issuedReserve);
        ammo.reloadSeconds = 0.0f;
    }

    void refillReserve(Ammo& ammo, const AmmoSettings& settings)
    {
        ammo.reserve = settings.maxReserve;
    }

    bool startReload(Ammo& ammo, const AmmoSettings& settings)
    {
        if (isReloading(ammo) || ammo.magazine >= settings.magazineSize || ammo.reserve <= 0)
        {
            return false;
        }

        ammo.reloadSeconds = ammo.magazine <= 0 ? settings.emptyReloadSeconds : settings.reloadSeconds;
        return true;
    }

    bool stepReload(Ammo& ammo, const AmmoSettings& settings, float playerDelta)
    {
        if (!isReloading(ammo))
        {
            return false;
        }

        ammo.reloadSeconds = std::max(ammo.reloadSeconds - playerDelta, 0.0f);

        if (isReloading(ammo))
        {
            return false;
        }

        const int32_t moved = std::min(settings.magazineSize - ammo.magazine, ammo.reserve);
        ammo.magazine += std::max(moved, 0);
        ammo.reserve -= std::max(moved, 0);

        return true;
    }

    bool takeRound(Ammo& ammo, const AmmoSettings& settings)
    {
        if (isReloading(ammo))
        {
            return false;
        }

        if (ammo.magazine <= 0)
        {
            startReload(ammo, settings);
            return false;
        }

        --ammo.magazine;
        return true;
    }
} //namespace game
