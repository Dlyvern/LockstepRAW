#ifndef LOCKSTEP_GAME_PRESENTATION_COMBAT_HUD_HPP
#define LOCKSTEP_GAME_PRESENTATION_COMBAT_HUD_HPP

#include "Player/PlayerStatus.hpp"

#include <Lockstep/Core/Math.hpp>

#include <cstdint>
#include <vector>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    struct Visuals;

    // One damage direction mark around the aim dot.
    // yaw:     world yaw towards where the hit came from.
    // seconds: presentation seconds left on screen.
    struct DamageMark
    {
        float yaw{0.0f};
        float seconds{0.0f};
    };

    // The local player's combat feedback, driven by the replicated status counters.
    // Presentation only, never replicated.
    // damageCount, hitCount, killCount: the status counters last seen.
    // hasCounters:  the counters above are valid (false until the first status).
    // hurtFlash:    red screen flash left, 0 to 1.
    // marks:        damage direction marks.
    // hitSeconds:   hit marker time left.
    // isKill:       the hit marker shows a kill (red).
    // restartFade:  fade to black of the checkpoint restart, 0 to 1.
    struct CombatHud
    {
        uint8_t damageCount{0};
        uint8_t hitCount{0};
        uint8_t killCount{0};
        bool hasCounters{false};
        float hurtFlash{0.0f};
        std::vector<DamageMark> marks;
        float hitSeconds{0.0f};
        bool isKill{false};
        float restartFade{0.0f};
    };

    // What the combat HUD shows this frame.
    // own:          the local player's status.
    // partner:      the partner's status.
    // isRestarting: the checkpoint restart is coming.
    // canRevive:    the local player stands close enough to the downed partner to revive them.
    // isArmed:      show the ammo counter.
    // magazine:     rounds to show in the magazine (the local prediction, never above the status).
    // fireMode:     the fire mode name shown under the ammo counter, or null.
    // eye, direction: the camera, for the marks placed around the aim dot.
    // lookYaw:      the camera's yaw (damage marks turn with the view).
    // hudFade:      the HUD's own fade to black (Hud::fade), kept when stronger.
    // seconds:      presentation time since the last frame.
    struct CombatHudFrame
    {
        const PlayerStatus& own;
        const PlayerStatus& partner;
        bool isRestarting{false};
        bool canRevive{false};
        bool isArmed{false};
        int32_t magazine{0};
        const char* fireMode{nullptr};
        math::Vec3 eye{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
        float lookYaw{0.0f};
        float hudFade{0.0f};
        float seconds{0.0f};
    };

    // Updates the feedback from the status counters (a new hit taken flashes red and adds a
    // direction mark; a hit landed shows a hit marker, a kill a red one) and draws it: the marks
    // and the hit marker around the aim dot, the ammo counter, the downed and revive texts, and
    // the screen fade (the restart's or the HUD's black, else the hurt flash in red; dark red
    // while downed). Call after drawHud(); it sets the screen fade last.
    // renderer: the renderer.
    // visuals:  the HUD materials.
    // hud:      the combat HUD state.
    // frame:    this frame's statuses, camera and time.
    void presentCombatHud(lockstep::render::Renderer& renderer, const Visuals& visuals, CombatHud& hud,
        const CombatHudFrame& frame);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_COMBAT_HUD_HPP
