#include "Player/WeaponDrill.hpp"

#if LOCKSTEP_DEV_TOOLS

#include "Weapons/WeaponCatalog.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace game
{
    namespace
    {
        constexpr std::string_view k_weaponFlag{"--dev-weapon"};
        constexpr std::string_view k_switchFlag{"--dev-switch-drill"};

        std::optional<std::string_view> valueOf(std::span<const char* const> arguments, std::string_view flag)
        {
            for (std::size_t index = 0; index + 1 < arguments.size(); ++index)
            {
                if (std::string_view(arguments[index]) == flag)
                {
                    return std::string_view(arguments[index + 1]);
                }
            }

            return std::nullopt;
        }

        std::optional<WeaponKind> weaponNamed(std::string_view name)
        {
            if (name == "m4")
            {
                return WeaponKind::M4;
            }

            if (name == "glock")
            {
                return WeaponKind::Glock;
            }

            if (name == "kriss")
            {
                return WeaponKind::Kriss;
            }

            return std::nullopt;
        }
    } //namespace

    void applyWeaponDrill(LocalControls& controls, const WeaponStatsTable& weapons,
        std::span<const char* const> arguments, uint64_t tick, float delta)
    {
        if (const std::optional<std::string_view> period = valueOf(arguments, k_switchFlag); period && delta > 0.0f)
        {
            const double seconds = std::strtod(std::string(*period).c_str(), nullptr);
            const auto ticks = static_cast<uint64_t>(std::max(std::lround(seconds / delta), 1L));

            if (tick % ticks == 0 && tick > 0)
            {
                const auto next = static_cast<WeaponKind>((weaponIndex(controls.weapon) + 1) % k_weaponCount);
                controls.switchSeconds = weaponSwitchSeconds(weapons, controls.weapon, next);
                controls.previousWeapon = controls.weapon;
                controls.weapon = next;
            }

            return;
        }

        if (const std::optional<std::string_view> name = valueOf(arguments, k_weaponFlag))
        {
            if (const std::optional<WeaponKind> weapon = weaponNamed(*name); weapon && controls.weapon != *weapon)
            {
                controls.weapon = *weapon;
                controls.previousWeapon = *weapon;
                controls.switchSeconds = 0.0f;
            }
        }
    }
} //namespace game

#endif
