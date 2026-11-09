#ifndef LOCKSTEP_GAME_GAME_STATE_HPP
#define LOCKSTEP_GAME_GAME_STATE_HPP

#include "Characters/NpcPlaceholders.hpp"
#include "Characters/PawnCharacters.hpp"
#include "Dialogue/Dialogue.hpp"
#include "Echo/EchoRecording.hpp"
#include "Enemies/Enemy.hpp"
#include "Level/ApartmentRoom.hpp"
#include "Level/Arena.hpp"
#include "Level/SceneLevel.hpp"
#include "Player/Pawns.hpp"
#include "Player/PlayerCommand.hpp"
#include "Player/PlayerStatus.hpp"
#include "Player/Prediction.hpp"
#include "Presentation/FlyCamera.hpp"
#include "Presentation/HeadMotion.hpp"
#include "Presentation/Hud.hpp"
#include "Presentation/Lighting.hpp"
#include "Presentation/SceneCharacters.hpp"
#include "Presentation/Visuals.hpp"
#include "Simulation/LevelFlow.hpp"
#include "Station/StationMission.hpp"
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
        struct Local
        {
            game::LocalControls controls{};
            game::PlayerCommand command{};
            game::WeaponZone weaponZone{game::WeaponZone::Open};
            game::PlayerStatuses statuses{};
            game::Prediction prediction{};
            bool isCombatActive{false};
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
        //                     authority; missions will set it (for now only the dev panel does).
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
        game::DialogueBook dialogue{};
        game::Subtitles subtitles{};
        game::NpcPlaceholders npcs{};
        game::LevelFlow levelFlow{};
        std::optional<game::EchoPlayback> echo{};
        game::FlyCamera flyCamera{};
        game::HeadMotionState headMotion{};
        Settings settings{};
        Local local{};
        Simulation simulation{};
        uint64_t lastTick{0};
    };
} //namespace lockstep::app

#endif //LOCKSTEP_GAME_GAME_STATE_HPP
