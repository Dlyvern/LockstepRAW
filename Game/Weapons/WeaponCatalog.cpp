#include "Weapons/WeaponCatalog.hpp"

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

namespace game
{
    namespace
    {
        std::size_t modeIndex(FireMode mode)
        {
            return static_cast<std::size_t>(mode);
        }

        WeaponStats m4Stats()
        {
            WeaponStats stats{};
            stats.ammo = AmmoSettings{.magazineSize = 30, .maxReserve = 150, .issuedReserve = 120,
                .reloadSeconds = 3.633f, .emptyReloadSeconds = 3.8f};
            stats.fire.roundsPerMinute = 750.0f;
            stats.recoil.climb = 0.0085f;
            stats.recoil.firstShotScale = 1.8f;
            stats.recoil.build = 0.07f;
            stats.recoil.maxBuild = 1.9f;
            stats.recoil.wander = 0.0055f;
            stats.recoil.wanderLimit = 0.034f;
            stats.spread.hip = 0.02f;
            stats.spread.aim = 0.0008f;
            stats.penetration.power = 1.6f;
            stats.penetration.maxPenetrations = 3;
            stats.damage = 1.4f;
            stats.modes = {{true, false, true}};
            stats.holsterSeconds = 0.567f;
            stats.drawSeconds = 1.033f;
            stats.flashScale = 1.3f;

            return stats;
        }

        WeaponStats glockStats()
        {
            WeaponStats stats{};
            stats.ammo = AmmoSettings{.magazineSize = 17, .maxReserve = 85, .issuedReserve = 68,
                .reloadSeconds = 2.8f, .emptyReloadSeconds = 3.033f};
            stats.fire.roundsPerMinute = 450.0f;
            stats.recoil.climb = 0.011f;
            stats.recoil.firstShotScale = 1.2f;
            stats.recoil.build = 0.02f;
            stats.recoil.maxBuild = 1.3f;
            stats.recoil.wander = 0.004f;
            stats.recoil.recoverRate = 9.0f;
            stats.spread.hip = 0.016f;
            stats.spread.aim = 0.0018f;
            stats.spread.moving = 0.012f;
            stats.penetration.power = 0.6f;
            stats.penetration.maxPenetrations = 1;
            stats.damage = 0.75f;
            stats.modes = {{true, false, false}};
            stats.holsterSeconds = 0.467f;
            stats.drawSeconds = 0.75f;
            stats.flashScale = 0.8f;

            return stats;
        }

        WeaponStats krissStats()
        {
            WeaponStats stats{};
            stats.holsterSeconds = 0.35f;
            stats.drawSeconds = 0.5f;

            return stats;
        }
    } //namespace

    WeaponStatsTable defaultWeaponStats()
    {
        return WeaponStatsTable{{m4Stats(), glockStats(), krissStats()}};
    }

    const WeaponStats& weaponStats(const WeaponStatsTable& stats, WeaponKind kind)
    {
        return stats[weaponIndex(kind)];
    }

    WeaponStats& weaponStats(WeaponStatsTable& stats, WeaponKind kind)
    {
        return stats[weaponIndex(kind)];
    }

    FireMode nextWeaponFireMode(const WeaponStats& stats, FireMode mode)
    {
        FireMode next = mode;

        for (std::size_t step = 0; step < stats.modes.size(); ++step)
        {
            next = nextFireMode(next);

            if (stats.modes[modeIndex(next)])
            {
                return next;
            }
        }

        return mode;
    }

    FireMode allowedFireMode(const WeaponStats& stats, FireMode mode)
    {
        if (stats.modes[modeIndex(mode)])
        {
            return mode;
        }

        for (const FireMode candidate : {FireMode::Auto, FireMode::Burst, FireMode::Semi})
        {
            if (stats.modes[modeIndex(candidate)])
            {
                return candidate;
            }
        }

        return FireMode::Semi;
    }

    float weaponSwitchSeconds(const WeaponStatsTable& stats, WeaponKind from, WeaponKind to)
    {
        return weaponStats(stats, from).holsterSeconds + weaponStats(stats, to).drawSeconds;
    }

#if LOCKSTEP_DEV_TOOLS
    void weaponStatsDevSection(WeaponStats& stats)
    {
        ImGui::SliderFloat("Damage x", &stats.damage, 0.1f, 4.0f);
        ImGui::Checkbox("Semi", &stats.modes[0]);
        ImGui::SameLine();
        ImGui::Checkbox("Burst", &stats.modes[1]);
        ImGui::SameLine();
        ImGui::Checkbox("Auto", &stats.modes[2]);
        ImGui::SliderFloat("Holster (s)", &stats.holsterSeconds, 0.05f, 2.0f);
        ImGui::SliderFloat("Draw (s)", &stats.drawSeconds, 0.05f, 2.0f);
        ImGui::SliderFloat("Flash size x", &stats.flashScale, 0.0f, 3.0f);

        AmmoSettings& ammo = stats.ammo;
        ImGui::SliderInt("Magazine", &ammo.magazineSize, 1, 100);
        ImGui::SliderInt("Max reserve", &ammo.maxReserve, 0, 255);
        ImGui::SliderInt("Issued reserve", &ammo.issuedReserve, 0, 255);
        ImGui::SliderFloat("Reload (s)", &ammo.reloadSeconds, 0.1f, 6.0f);
        ImGui::SliderFloat("Empty reload (s)", &ammo.emptyReloadSeconds, 0.1f, 6.0f);

        FireModeSettings& fire = stats.fire;
        ImGui::SliderFloat("Rounds per minute", &fire.roundsPerMinute, 60.0f, 1500.0f);
        int burst = fire.burstLength;

        if (ImGui::SliderInt("Burst length", &burst, 2, 5))
        {
            fire.burstLength = static_cast<uint8_t>(burst);
        }

        ImGui::SliderFloat("Burst pause (s)", &fire.burstPause, 0.0f, 0.5f);
        ImGui::SliderFloat("Host rate tolerance", &fire.rateTolerance, 0.0f, 1.0f);

        if (ImGui::TreeNode("Camera recoil"))
        {
            RecoilSettings& camera = stats.recoil;
            ImGui::SliderFloat("Climb per shot (rad)", &camera.climb, 0.0f, 0.04f, "%.4f");
            ImGui::SliderFloat("First shot x", &camera.firstShotScale, 0.5f, 3.0f);
            ImGui::SliderFloat("Build per shot", &camera.build, 0.0f, 0.2f);
            ImGui::SliderFloat("Max build x", &camera.maxBuild, 1.0f, 3.0f);
            ImGui::SliderFloat("Wander per shot (rad)", &camera.wander, 0.0f, 0.02f, "%.4f");
            ImGui::SliderFloat("Wander band (rad)", &camera.wanderLimit, 0.0f, 0.1f, "%.4f");
            ImGui::SliderFloat("Wander pull", &camera.wanderPull, 0.0f, 1.0f);
            ImGui::SliderFloat("Kick time (s)", &camera.kickSeconds, 0.0f, 0.2f);
            ImGui::SliderFloat("Recovery delay (s)", &camera.recoverDelay, 0.0f, 0.6f);
            ImGui::SliderFloat("Recovery rate", &camera.recoverRate, 0.0f, 20.0f);
            ImGui::SliderFloat("Recovered fraction", &camera.recoverAmount, 0.0f, 1.0f);
            ImGui::SliderFloat("String ends after (s)", &camera.streakSeconds, 0.05f, 1.0f);
            ImGui::SliderFloat("Aiming x", &camera.aimScale, 0.0f, 1.0f);
            ImGui::SliderFloat("Crouched x", &camera.crouchScale, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Spread"))
        {
            SpreadSettings& spread = stats.spread;
            ImGui::SliderFloat("Hip (rad)", &spread.hip, 0.0f, 0.1f, "%.4f");
            ImGui::SliderFloat("Aiming (rad)", &spread.aim, 0.0f, 0.02f, "%.4f");
            ImGui::SliderFloat("Moving (rad)", &spread.moving, 0.0f, 0.1f, "%.4f");
            ImGui::SliderFloat("Running (rad)", &spread.sprinting, 0.0f, 0.15f, "%.4f");
            ImGui::SliderFloat("Crouched x", &spread.crouchScale, 0.0f, 1.0f);
            ImGui::SliderFloat("Max sight offset (rad)", &spread.maxAimOffset, 0.0f, 0.4f);
            ImGui::TreePop();
        }

        penetrationDevSection(stats.penetration);
    }
#endif
} //namespace game
