#ifndef LOCKSTEP_GAME_WEAPONS_WEAPON_KIND_HPP
#define LOCKSTEP_GAME_WEAPONS_WEAPON_KIND_HPP

#include <cstddef>
#include <cstdint>

namespace game
{
    // The weapons of the loadout, in key order: 1 the M4 (primary), 2 the Glock 17 (secondary),
    // 3 the KRISS Vector. Replicated in two bits (PlayerCommand, PlayerStatus).
    enum class WeaponKind : uint8_t
    {
        M4 = 0,
        Glock,
        Kriss,
        Count
    };

    // Number of WeaponKind values.
    inline constexpr std::size_t k_weaponCount{static_cast<std::size_t>(WeaponKind::Count)};

    // kind:    a weapon.
    // Returns: its index into per-weapon arrays.
    [[nodiscard]] constexpr std::size_t weaponIndex(WeaponKind kind) noexcept
    {
        return static_cast<std::size_t>(kind) < k_weaponCount ? static_cast<std::size_t>(kind) : 0;
    }

    // bits:    two bits of a message.
    // Returns: the weapon they name (M4 for an unknown value).
    [[nodiscard]] constexpr WeaponKind weaponFromBits(uint32_t bits) noexcept
    {
        return bits < k_weaponCount ? static_cast<WeaponKind>(bits) : WeaponKind::M4;
    }

    // kind:    a weapon.
    // Returns: its HUD name ("M4", "GLOCK 17", "KRISS").
    [[nodiscard]] constexpr const char* weaponName(WeaponKind kind) noexcept
    {
        switch (kind)
        {
            case WeaponKind::Glock:
            {
                return "GLOCK 17";
            }
            case WeaponKind::Kriss:
            {
                return "KRISS";
            }
            case WeaponKind::M4:
            case WeaponKind::Count:
            {
                return "M4";
            }
        }

        return "M4";
    }
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_WEAPON_KIND_HPP
