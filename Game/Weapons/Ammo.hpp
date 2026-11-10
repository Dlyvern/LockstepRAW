#ifndef LOCKSTEP_GAME_WEAPONS_AMMO_HPP
#define LOCKSTEP_GAME_WEAPONS_AMMO_HPP

#include <cstdint>

namespace game
{
    // Magazine and reload tuning of the V-45. Host-side rules; the client only shows the result.
    // magazineSize:   rounds in a full magazine.
    // maxReserve:     most rounds carried besides the magazine.
    // issuedReserve:  reserve handed out with the weapon (armory, --armed, a combat level).
    // reloadSeconds:  Player seconds from pressing R to a full magazine, with rounds left.
    // emptyReloadSeconds: the same from an empty magazine (the charging handle is pulled too).
    struct AmmoSettings
    {
        int32_t magazineSize{30};
        int32_t maxReserve{120};
        int32_t issuedReserve{90};
        float reloadSeconds{2.0f};
        float emptyReloadSeconds{2.6f};
    };

    // One player's rounds.
    // magazine:       rounds in the weapon.
    // reserve:        rounds carried besides it.
    // reloadSeconds:  Player seconds left of a running reload; 0 while not reloading.
    // processedReloads: the command reload counter the host already handled.
    // isIssued:       rounds were handed out with the weapon (reset when the player is unarmed).
    struct Ammo
    {
        int32_t magazine{0};
        int32_t reserve{0};
        float reloadSeconds{0.0f};
        uint8_t processedReloads{0};
        bool isIssued{false};
    };

    // ammo:    a player's rounds.
    // Returns: true while a reload runs.
    [[nodiscard]] bool isReloading(const Ammo& ammo);

    // Gives a full magazine and the issued reserve (at least what is already carried).
    // ammo:     the rounds, changed.
    // settings: magazine size and issued reserve.
    void issueAmmo(Ammo& ammo, const AmmoSettings& settings);

    // Fills the reserve to its maximum (an ammo pickup such as the armory's).
    // ammo:     the rounds, changed.
    // settings: the reserve maximum.
    void refillReserve(Ammo& ammo, const AmmoSettings& settings);

    // Starts a reload when the magazine is not full, rounds are left and none runs yet; from an
    // empty magazine it takes emptyReloadSeconds.
    // ammo:     the rounds, changed.
    // settings: magazine size and reload times.
    // Returns:  true if a reload started.
    bool startReload(Ammo& ammo, const AmmoSettings& settings);

    // Advances a running reload; when it ends, moves rounds from the reserve into the magazine.
    // ammo:        the rounds, changed.
    // settings:    magazine size.
    // playerDelta: the Player domain's delta of this tick.
    // Returns:     true if a reload finished this tick.
    bool stepReload(Ammo& ammo, const AmmoSettings& settings, float playerDelta);

    // Takes one round for a shot, unless a reload runs or the magazine is empty (then a reload
    // starts by itself).
    // ammo:     the rounds, changed.
    // settings: magazine size and reload time.
    // Returns:  true if the shot may fire.
    bool takeRound(Ammo& ammo, const AmmoSettings& settings);
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_AMMO_HPP
