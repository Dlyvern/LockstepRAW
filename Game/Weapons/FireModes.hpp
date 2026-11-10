#ifndef LOCKSTEP_GAME_WEAPONS_FIRE_MODES_HPP
#define LOCKSTEP_GAME_WEAPONS_FIRE_MODES_HPP

#include <cstdint>

namespace game
{
    // How the trigger fires the weapon. B cycles Semi -> Burst -> Auto.
    // Semi:  one shot per click.
    // Burst: a click fires burstLength shots at the cyclic rate.
    // Auto:  fires at the cyclic rate while the button is held.
    enum class FireMode : uint8_t
    {
        Semi,
        Burst,
        Auto
    };

    // Fire rate and burst rules, the same on host and client (the host rejects shots faster than
    // the cyclic rate). Editable from the dev panel.
    // roundsPerMinute: cyclic rate.
    // burstLength:     shots a burst fires.
    // burstPause:      extra seconds after a burst before the next one may start.
    // rateTolerance:   fraction of the shot interval the host forgives (tick rounding).
    struct FireModeSettings
    {
        float roundsPerMinute{900.0f};
        uint8_t burstLength{3};
        float burstPause{0.12f};
        float rateTolerance{0.25f};
    };

    // The local trigger: fire mode, cadence and the rounds this machine thinks are loaded.
    // mode:      the fire mode.
    // cooldown:  Player seconds until the next shot may fire.
    // queued:    shots a burst (or a click during the cooldown) still has to fire.
    // rounds:    predicted rounds in the magazine (local shots taken off the newest status).
    // seenMagazine: the status magazine last seen (-1 before the first).
    struct Trigger
    {
        FireMode mode{FireMode::Auto};
        float cooldown{0.0f};
        uint8_t queued{0};
        int32_t rounds{0};
        int32_t seenMagazine{-1};
    };

    // What the trigger reads on one tick.
    // isPressed:  fire was pressed this tick.
    // isHeld:     fire is held.
    // canFire:    a shot would fire now (armed, raised, ready, not reloading, no toy held).
    // deltaSeconds: the Player domain's delta of this tick.
    struct TriggerInput
    {
        bool isPressed{false};
        bool isHeld{false};
        bool canFire{false};
        float deltaSeconds{0.0f};
    };

    // settings: the fire rate.
    // Returns:  seconds between two shots at the cyclic rate.
    [[nodiscard]] float shotInterval(const FireModeSettings& settings);

    // mode:    a fire mode.
    // Returns: the next mode in the B cycle.
    [[nodiscard]] FireMode nextFireMode(FireMode mode);

    // mode:    a fire mode.
    // Returns: its HUD name ("SEMI", "BURST", "AUTO").
    [[nodiscard]] const char* fireModeName(FireMode mode);

    // Takes the replicated magazine into the predicted rounds: a reload that finished (the
    // magazine grew) resets them, otherwise they never exceed what the host reported.
    // trigger:     the trigger, updated.
    // magazine:    the magazine in the newest status.
    // isReloading: a reload runs.
    void syncTriggerRounds(Trigger& trigger, int32_t magazine, bool isReloading);

    // Advances the trigger by one tick and decides the shots it fires: a click queues one shot
    // (Semi) or a burst, Auto keeps one queued while held; a queued shot fires once the cooldown
    // is over and rounds are left. Releasing in Auto, or not being able to fire, drops the queue.
    // trigger:  the trigger, updated.
    // settings: the fire rate.
    // input:    this tick's button and weapon state.
    // Returns:  shots fired this tick (0 or 1 at rates below the tick rate).
    [[nodiscard]] uint8_t stepTrigger(Trigger& trigger, const FireModeSettings& settings, const TriggerInput& input);

    // Host: the cadence check of one pawn's shots. Each processed input lowers the cooldown; a
    // shot faster than the cyclic rate (less the tolerance) is refused.
    // cooldown: the pawn's cooldown in Player seconds, updated.
    // settings: the fire rate.
    // Returns:  true if the shot may fire (then the cooldown restarts).
    [[nodiscard]] bool acceptShotCadence(float& cooldown, const FireModeSettings& settings);
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_FIRE_MODES_HPP
