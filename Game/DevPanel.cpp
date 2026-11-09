#include "DevPanel.hpp"

#if LOCKSTEP_DEV_TOOLS

#include "GameState.hpp"
#include "Level/RoomDevPanel.hpp"
#include "Simulation/HostSimulation.hpp"
#include "Station/StationDevPanel.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#include <imgui.h>

#include <cstddef>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr float k_maxWorldScale{2.0f};
        constexpr float k_maxMoveSpeed{8.0f};
        constexpr float k_maxAcceleration{30.0f};
        constexpr float k_maxEnemySpeed{10.0f};
        constexpr float k_maxRespawnDelay{10.0f};
        constexpr int k_maxEnemyHealth{20};
        constexpr int k_maxPlayerHealth{255};
        constexpr float k_panelMargin{10.0f};
        constexpr double k_millisecondsPerTick{1000.0 / 60.0};
        constexpr float k_minCapsuleHeight{2.0f * k_pawnRadius + 0.05f};
        constexpr float k_maxSlideSeconds{3.0f};
        constexpr float k_maxSlideSteer{6.0f};

        const char* stanceName(const PawnMotion& motion)
        {
            if (isSliding(motion))
            {
                return "sliding";
            }

            switch (motion.stance)
            {
                case PawnStance::Crouch:
                    return "crouched";
                case PawnStance::Slide:
                    return "low";
                case PawnStance::Stand:
                    break;
            }

            return "standing";
        }

        void positionText(const char* label, const world::Entity* entity)
        {
            if (entity == nullptr)
            {
                ImGui::Text("%-10s -", label);
                return;
            }

            const math::Vec3& position = entity->transform().position;
            ImGui::Text("%-10s %6.2f %6.2f %6.2f", label, position.x, position.y, position.z);
        }

        void timeDomainSection(GameState& state, const EngineContext& engine)
        {
            const world::TimeDomains& domains = engine.timeDomains;

            ImGui::SliderFloat("World scale", &state.settings.worldScale, 0.0f, k_maxWorldScale);
            ImGui::SliderFloat("Bullet-time scale", &state.settings.bulletTimeScale, 0.0f, 1.0f);
            ImGui::Text("Tick        %llu", static_cast<unsigned long long>(state.lastTick));
            ImGui::Text("Player      x%.2f  %.2f s", domains.scale(world::TimeDomain::Player),
                domains.elapsed(world::TimeDomain::Player));
            ImGui::Text("World       x%.2f  %.2f s", domains.scale(world::TimeDomain::World),
                domains.elapsed(world::TimeDomain::World));
            ImGui::Text("Bullet-time local %s, remote %s", state.local.command.bulletTime ? "on" : "off",
                state.simulation.isRemoteBulletTime ? "on" : "off");
            ImGui::Text("Look        yaw %.2f  pitch %.2f  shots %u", state.local.command.yaw,
                state.local.command.pitch, static_cast<unsigned>(state.local.command.shotCount));
        }

        void armedSection(GameState& state)
        {
            const char* labels[] = {"Player1 armed", "Player2 armed"};

            for (std::size_t index = 0; index < state.simulation.pawns.size(); ++index)
            {
                if (index > 0)
                {
                    ImGui::SameLine();
                }

                ImGui::Checkbox(labels[index], &state.simulation.pawns[index].isArmed);
            }
        }

        void hostSection(GameState& state, const EngineContext& engine)
        {
            GameState::Settings& settings = state.settings;
            const GameState::Simulation& simulation = state.simulation;
            armedSection(state);
            ImGui::Checkbox("Combat active (safe zones off)", &state.simulation.isCombatActive);

            const bool isClientConnected = engine.session.state() == net::ConnectionState::Connected;

            ImGui::BeginDisabled(isClientConnected);
            ImGui::SliderFloat("Walk speed", &settings.pawns.walkSpeed, 0.0f, k_maxMoveSpeed);
            ImGui::SliderFloat("Run speed (Shift)", &settings.pawns.runSpeed, 0.0f, k_maxMoveSpeed);
            ImGui::SliderFloat("Run speed, weapon up", &settings.pawns.weaponUpRunSpeed, 0.0f, k_maxMoveSpeed);
            ImGui::SliderFloat("Back speed", &settings.pawns.backSpeed, 0.0f, k_maxMoveSpeed);
            ImGui::SliderFloat("Strafe speed", &settings.pawns.strafeSpeed, 0.0f, k_maxMoveSpeed);
            ImGui::SliderFloat("Run strafe speed", &settings.pawns.sprintStrafeSpeed, 0.0f, k_maxMoveSpeed);
            ImGui::SliderFloat("Aim speed", &settings.pawns.aimSpeed, 0.0f, k_maxMoveSpeed);
            ImGui::SliderFloat("Crouch speed", &settings.pawns.crouchSpeed, 0.0f, k_maxMoveSpeed);
            ImGui::SliderFloat("Crouch height (m)", &settings.pawns.crouchHeight, k_minCapsuleHeight, k_pawnStandHeight);
            ImGui::SliderFloat("Slide height (m)", &settings.pawns.slideHeight, k_minCapsuleHeight, k_pawnStandHeight);
            ImGui::SliderFloat("Slide start speed", &settings.pawns.slideSpeed, 0.0f, k_maxMoveSpeed);
            ImGui::SliderFloat("Slide end speed", &settings.pawns.slideEndSpeed, 0.0f, k_maxMoveSpeed);
            ImGui::SliderFloat("Slide time (s)", &settings.pawns.slideSeconds, 0.0f, k_maxSlideSeconds);
            ImGui::SliderFloat("Slide cooldown (s)", &settings.pawns.slideCooldown, 0.0f, k_maxSlideSeconds);
            ImGui::SliderFloat("Slide min speed", &settings.pawns.slideMinSpeed, 0.0f, k_maxMoveSpeed);
            ImGui::SliderFloat("Slide steer (rad/s)", &settings.pawns.slideSteerRate, 0.0f, k_maxSlideSteer);
            ImGui::SliderFloat("Acceleration", &settings.pawns.acceleration, 0.1f, k_maxAcceleration);
            ImGui::SliderFloat("Deceleration", &settings.pawns.deceleration, 0.1f, k_maxAcceleration);
            ImGui::SliderInt("Player health", &settings.pawns.maxHealth, 1, k_maxPlayerHealth);
            ImGui::EndDisabled();

            if (isClientConnected)
            {
                ImGui::TextDisabled("Movement and player health are locked while a client is connected.");
            }

            ImGui::SliderFloat("Enemy speed", &settings.enemy.walkSpeed, 0.0f, k_maxEnemySpeed);
            ImGui::SliderInt("Enemy health", &settings.enemy.maxHealth, 1, k_maxEnemyHealth);
            ImGui::SliderFloat("Respawn delay", &settings.enemy.respawnDelay, 0.0f, k_maxRespawnDelay);
            ImGui::SliderInt("Enemy damage", &settings.enemy.attackDamage, 0, k_maxPlayerHealth);
            ImGui::SliderFloat("Enemy cooldown", &settings.enemy.attackCooldown, 0.1f, k_maxRespawnDelay);
            ImGui::SliderFloat("Player respawn", &settings.pawns.respawnDelay, 0.0f, k_maxRespawnDelay);
            ImGui::Separator();

            const char* names[] = {"Player1", "Player2"};

            for (std::size_t index = 0; index < simulation.pawns.size(); ++index)
            {
                const Pawn& pawn = simulation.pawns[index];
                positionText(names[index], engine.entities.find(pawn.entity));
                ImGui::SameLine();
                ImGui::Text("  hp %d  %s  %s  shots %u  hits %u  deaths %u", pawn.health,
                    pawn.motion.isGrounded ? "ground" : "air", stanceName(pawn.motion), pawn.shots, pawn.hits,
                    pawn.deaths);

                if (pawn.isJoined && pawn.entity.isNull())
                {
                    ImGui::Text("            respawn in %.1f s", pawn.respawnTimer);
                }
            }

            const Enemy& enemy = simulation.enemy;
            positionText("Enemy", engine.entities.find(enemy.entity));
            ImGui::Text("Enemy       %s  health %d  respawn %.1f s  deaths %u", toString(enemy.phase), enemy.health,
                enemy.respawnTimer, enemy.deaths);
            ImGui::Text("Last shot   %s  %.2f m  rewound %.0f ms", toString(simulation.lastShot.outcome),
                simulation.lastShot.distance, simulation.lastRewindTicks * k_millisecondsPerTick);
        }

        void clientSection(const GameState& state, const EngineContext& engine)
        {
            const PlayerStatus& own = state.local.statuses[k_clientPawn];
            ImGui::Text("Health      %d%s  %s", own.health, own.isDead ? "  (dead)" : "",
                own.isArmed ? (own.isAiming ? "armed, aiming" : (own.isLowered ? "armed, lowered" : "armed")) : "unarmed");
            ImGui::TextDisabled("Arming is host-side (host dev panel or --armed on the host)");
            ImGui::Text("Combat      %s  stance %s", state.local.isCombatActive ? "active" : "off",
                stanceName(state.local.prediction.motion));

            const Prediction& prediction = state.local.prediction;
            positionText("Predicted", engine.entities.find(prediction.entity));
            ImGui::Text("Prediction  %zu unacked  %u corrections  last %.3f m, %zu replayed", prediction.history.size(),
                prediction.corrections, prediction.lastError, prediction.lastReplayed);
            ImGui::Text("Smoothing   %.3f m", math::length(prediction.visualOffset));

            for (const net::MirroredEntity& mirrored : engine.replication.mirrored())
            {
                const world::Entity* entity = engine.entities.find(mirrored.entity);

                if (entity == nullptr)
                {
                    continue;
                }

                const auto archetype = static_cast<Archetype>(mirrored.archetype);
                const char* label = "Other";

                if (archetype == Archetype::Enemy)
                {
                    label = "Enemy";
                }
                else if (archetype == Archetype::Pawn)
                {
                    label = entity->owner() == world::EntityOwner::Player2 ? "Player2" : "Player1";
                }

                positionText(label, entity);
            }

            ImGui::TextDisabled("Enemy health and hits are host-side only");
        }
    } //namespace

    void devToolsPanels(GameState& state, EngineContext& engine)
    {
        const ImVec2 topRight{ImGui::GetIO().DisplaySize.x - k_panelMargin, k_panelMargin};
        ImGui::SetNextWindowPos(topRight, ImGuiCond_FirstUseEver, ImVec2{1.0f, 0.0f});

        if (ImGui::Begin("Game", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            timeDomainSection(state, engine);
            ImGui::Separator();

            if (engine.session.role() == net::Role::Client)
            {
                clientSection(state, engine);
            }
            else
            {
                hostSection(state, engine);
            }

            roomDevSection(state);
            stationDevSection(state, engine);
            pawnCharactersDevSection(state.pawnCharacters ? &*state.pawnCharacters : nullptr);
            headMotionDevSection(state.headMotion);

            if (state.level)
            {
                sceneCharactersDevSection(state.level->scene, state.characters);
            }
            ImGui::Separator();
            ImGui::TextDisabled("WASD walk, hold Shift to run, mouse look, left click fire, E use");
            ImGui::TextDisabled("Hold Ctrl or toggle C to crouch; Ctrl or C while running slides");
            ImGui::TextDisabled("Armed: weapon up by default; hold right mouse to aim down sights (slower)");
            ImGui::TextDisabled("X lowers / raises the weapon (fire or aim raises it); safe zones keep it lowered");
            ImGui::TextDisabled("unless combat is active");
            ImGui::TextDisabled("Hold Space for bullet-time (either player)");
            ImGui::TextDisabled("Click to capture the mouse, Esc releases it");
        }

        ImGui::End();
    }
} //namespace game

#endif
