#ifndef LOCKSTEP_GAME_PLAYER_PAWN_HEALTH_HPP
#define LOCKSTEP_GAME_PLAYER_PAWN_HEALTH_HPP

#include "Player/Pawns.hpp"

#include <Lockstep/Core/Math.hpp>

#include <cstdint>

namespace game
{
    // Tuning of being downed, reviving and the checkpoint restart. Host-side rules.
    // bleedOutSeconds: Player seconds a downed player lasts without a revive.
    // reviveSeconds:   how long the partner holds F next to them to revive.
    // reviveRange:     farthest the reviver's feet may be from the downed player's, metres.
    // reviveHealth:    health after a revive.
    // crawlScale:      longest move input while downed (0 to 1; crouch speed times this).
    // downedBleedPerDamage: bleed-out seconds a hit on a downed player takes per hit point.
    // restartDelay:    Player seconds from the squad's failure (both down, or a bleed-out) to the
    //                  checkpoint restart; the screen fades out meanwhile.
    struct DownedSettings
    {
        float bleedOutSeconds{30.0f};
        float reviveSeconds{3.0f};
        float reviveRange{2.0f};
        int32_t reviveHealth{40};
        float crawlScale{0.6f};
        float downedBleedPerDamage{0.15f};
        float restartDelay{3.0f};
    };

    // What one hit did to a pawn.
    // None:    nothing (no pawn, or no damage).
    // Hurt:    health went down; still standing (or still downed with bleed-out time left).
    // Downed:  health reached 0: the player is downed now.
    // BledOut: a downed player ran out of bleed-out time.
    enum class PawnDamage : uint8_t
    {
        None,
        Hurt,
        Downed,
        BledOut
    };

    // Host: applies a hit to a pawn: takes health, records where it came from (for the HUD damage
    // indicator) and downs it at 0 health. A downed pawn loses bleed-out time instead.
    // pawn:     the pawn; must be alive (have an entity).
    // damage:   hit points.
    // source:   where the shot came from (the shooter's eyes).
    // target:   the pawn's centre.
    // settings: downed tuning.
    // Returns:  what the hit did.
    PawnDamage applyPawnDamage(Pawn& pawn, int32_t damage, const math::Vec3& source, const math::Vec3& target,
        const DownedSettings& settings);

    // Host: advances a downed pawn's bleed-out.
    // pawn:        the pawn.
    // playerDelta: the Player domain's delta of this tick.
    // Returns:     true if it bled out this tick.
    bool stepBleedOut(Pawn& pawn, float playerDelta);

    // Host: brings a downed pawn back up with DownedSettings::reviveHealth.
    // pawn:     the pawn.
    // settings: downed tuning.
    void revivePawn(Pawn& pawn, const DownedSettings& settings);

    // Host: clears the downed state, the revive progress and the health (for a respawn).
    // pawn: the pawn.
    void clearDowned(Pawn& pawn);

    // Packs a world yaw (radians) into a byte, for the HUD damage indicator.
    // yaw:     the angle.
    // Returns: the byte.
    [[nodiscard]] uint8_t quantizeYaw(float yaw);

    // yaw:     a byte from quantizeYaw().
    // Returns: the angle in radians, -pi to pi.
    [[nodiscard]] float dequantizeYaw(uint8_t yaw);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PAWN_HEALTH_HPP
