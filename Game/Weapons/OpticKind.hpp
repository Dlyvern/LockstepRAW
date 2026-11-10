#ifndef LOCKSTEP_GAME_WEAPONS_OPTIC_KIND_HPP
#define LOCKSTEP_GAME_WEAPONS_OPTIC_KIND_HPP

#include <cstdint>

namespace game
{
    // The sights a weapon's Optic rail socket can carry: none (iron sights), the 1x holographic
    // sight or the 4x scope. Replicated (PlayerCommand::optic, PlayerStatus::optic).
    enum class OpticKind : uint8_t
    {
        None = 0,
        Holographic,
        Scope4x
    };

    // value:   a replicated optic value.
    // Returns: the optic it names (None for an unknown value).
    [[nodiscard]] constexpr OpticKind opticFromValue(uint32_t value) noexcept
    {
        return value <= static_cast<uint32_t>(OpticKind::Scope4x) ? static_cast<OpticKind>(value) : OpticKind::None;
    }
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_OPTIC_KIND_HPP
