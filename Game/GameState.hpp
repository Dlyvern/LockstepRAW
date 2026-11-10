#ifndef LOCKSTEP_GAME_GAME_STATE_HPP
#define LOCKSTEP_GAME_GAME_STATE_HPP

#include "Audio/Footsteps.hpp"
#include "Characters/NpcPlaceholders.hpp"
#include "Characters/PawnCharacters.hpp"
#include "Dialogue/Dialogue.hpp"
#include "Echo/EchoRecording.hpp"
#include "Enemies/Enemy.hpp"
#include "Hospital/HospitalMission.hpp"
#include "Level/ApartmentRoom.hpp"
#include "Level/Arena.hpp"
#include "Level/SceneLevel.hpp"
#include "Npcs/NavGrid.hpp"
#include "Npcs/Npc.hpp"
#include "Npcs/NpcMessages.hpp"
#include "Npcs/NpcPresentation.hpp"
#include "Player/PawnHealth.hpp"
#include "Player/Pawns.hpp"
#include "Player/PlayerCommand.hpp"
#include "Player/PlayerStatus.hpp"
#include "Player/Prediction.hpp"
#include "Presentation/CombatHud.hpp"
#include "Presentation/FlyCamera.hpp"
#include "Presentation/HeadMotion.hpp"
#include "Presentation/Hud.hpp"
#include "Presentation/Lighting.hpp"
#include "Presentation/SceneCharacters.hpp"
#include "Presentation/Visuals.hpp"
#include "Simulation/LevelFlow.hpp"
#include "Station/StationMission.hpp"
#include "Weapons/Ammo.hpp"
#include "Weapons/Hitscan.hpp"
#include "Weapons/LagCompensation.hpp"
#include "Weapons/WeaponStance.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace lockstep::app
{
    // All of the game's state, behind the one pointer the Engine passes back into Game.
    struct GameState
    {
        // Tunables, editable from the dev panel. Only the host's values drive the simulation.
        // worldScale:      World time scale without bullet-time.
        // bulletTimeScale: World time scale while either player holds bullet-time.
        // spawnPoints:     foot positions where Player1's and Player2's pawns (re)spawn.
        // hasEnemy:        the host spawns the enemy (off in the apartment room).
        // startArmed:      players join carrying the weapon (--armed, or the arena without a level);
        //                  otherwise they are armed later by the story (or the dev panel).
        struct Settings
        {
            float worldScale{1.0f};
            float bulletTimeScale{0.2f};
            std::array<math::Vec3, game::k_playerCount> spawnPoints{{{-2.0f, 0.0f, 8.0f}, {2.0f, 0.0f, 8.0f}}};
            bool hasEnemy{true};
            bool startArmed{false};
            game::ControlSettings controls{};
            game::PawnSettings pawns{};
            game::EnemySettings enemy{};
            game::HitscanSettings hitscan{};
            game::DownedSettings downed{};
            game::AmmoSettings ammo{};
            game::NpcSettings npcs{};
        };

        // This machine's player.
        // controls:   accumulated look, shot counter and lower toggle.
        // command:    the command built from local input on the last tick.
        // statuses:   both players' health as this machine knows it (the client's comes from the
        //             host's status messages).
        // prediction: the client's predicted pawn. Unused on the host.
        // weaponZone: the weapon zone rule in effect where the local pawn stood on the last tick
        //             (a safe zone counts as open during combat).
        // isCombatActive: a combat situation runs, as this machine knows it (the client's comes
        //             from the host's status messages).
        // pendingLook: mouse movement (pixels) of frames since the newest tick, not yet in a
        //             command; the camera adds it so the view turns every frame (presentation only).
        // lookTicks:  FrameInfo::ticksCompleted when pendingLook was last reset.
        // isRestarting: the squad failed and the checkpoint restart is coming (fade out).
        // npcViews:   the client's newest NPC state message (the host reads its roster instead).
        // combatHud:  hit feedback, damage indicators and the restart fade.
        struct Local
        {
            game::LocalControls controls{};
            game::PlayerCommand command{};
            game::WeaponZone weaponZone{game::WeaponZone::Open};
            game::PlayerStatuses statuses{};
            game::Prediction prediction{};
            bool isCombatActive{false};
            math::Vec2 pendingLook{0.0f, 0.0f};
            uint64_t lookTicks{0};
            bool isRestarting{false};
            game::NpcStateMessage npcViews{};
            game::CombatHud combatHud{};
        };

        // The host's simulation. Unused on a client.
        // pawns:              Player1's (the host's) and Player2's (the client's) pawn.
        // enemy:              the enemy.
        // lastShot:           what the most recent shot struck.
        // isRemoteBulletTime: true while the client holds bullet-time.
        // sentStatuses:       the status message last sent to the client; nothing forces a resend.
        // lagHistory:         where pawns and the enemy stood on recent ticks, for client shots.
        // lastRewindTicks:    how far back the most recent shot was checked.
        // isCombatActive:     a combat situation runs: safe zones stop lowering weapons. The
        //                     authority; missions set it (the hospital does) and the dev panel.
        // npcs:               the armed NPCs (mercs and squad).
        // nav:                the level's navigation grid, built when a level has NPC markers.
        // sentNpcs:           the NPC state message last sent to the client.
        // checkpoint:         where both players restart after the squad fails, if a mission set
        //                     it (else the level's spawn points).
        // restartSeconds:     Player seconds until the checkpoint restart, while isRestarting.
        // isRestarting:       the squad failed (both down, or one bled out).
        struct Simulation
        {
            std::array<game::Pawn, game::k_playerCount> pawns{};
            game::Enemy enemy{};
            game::ShotResult lastShot{};
            bool isRemoteBulletTime{false};
            std::optional<game::StatusMessage> sentStatuses{};
            game::LagHistory lagHistory{};
            double lastRewindTicks{0.0};
            bool isCombatActive{false};
            game::NpcRoster npcs{};
            std::optional<game::NavGrid> nav{};
            std::optional<game::NpcStateMessage> sentNpcs{};
            std::optional<std::array<math::Vec3, game::k_playerCount>> checkpoint{};
            float restartSeconds{0.0f};
            bool isRestarting{false};
        };

        game::Visuals visuals{};
        game::Hud hud{};
        game::LightingState lighting{};
        game::Arena arena{};
        std::optional<game::SceneLevel> level{};
        std::vector<game::WeaponZoneBox> weaponZones{};
        game::SceneCharacters characters{};
        std::optional<game::PawnCharacters> pawnCharacters{};
        std::optional<game::ApartmentRoom> room{};
        std::optional<game::StationMission> station{};
        std::optional<game::HospitalMission> hospital{};
        game::DialogueBook dialogue{};
        game::Subtitles subtitles{};
        game::NpcPlaceholders npcs{};
        game::NpcLooks npcLooks{};
        game::LevelFlow levelFlow{};
        std::optional<game::EchoPlayback> echo{};
        game::FlyCamera flyCamera{};
        game::HeadMotionState headMotion{};
        game::Footsteps footsteps{};
        Settings settings{};
        Local local{};
        Simulation simulation{};
        uint64_t lastTick{0};
    };
} //namespace lockstep::app

#endif //LOCKSTEP_GAME_GAME_STATE_HPP
