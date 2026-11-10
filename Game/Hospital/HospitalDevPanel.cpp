#include "Hospital/HospitalDevPanel.hpp"

#if LOCKSTEP_DEV_TOOLS

#include "GameState.hpp"
#include "Npcs/NpcRoster.hpp"
#include "Simulation/HospitalSimulation.hpp"
#include "Simulation/HostSimulation.hpp"
#include "Simulation/PlayerCombat.hpp"

#include <Lockstep/Net/Session.hpp>
#include <Lockstep/World/Entities.hpp>

#include <imgui.h>

#include <limits>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr int k_maxHealth{500};
        constexpr int k_maxDamage{100};
        constexpr int k_maxRounds{255};
        constexpr int32_t k_lethalDamage{std::numeric_limits<int16_t>::max()};

        void mercControls(GameState& state, EngineContext& engine)
        {
            if (!state.hospital)
            {
                ImGui::TextDisabled("Merc spawning: only in hospital levels (Spawn_Merc_* markers)");
                return;
            }

            HospitalMission& mission = *state.hospital;
            bool isEnabled = mission.areMercsEnabled;

            if (ImGui::Checkbox("Spawn mercs (floors spawn as beats are reached)", &isEnabled))
            {
                setMercsEnabled(state, engine, isEnabled);
            }

            const char* floors[] = {"Spawn G", "Spawn F2", "Spawn F3"};

            for (std::size_t floor = 0; floor < k_mercFloors; ++floor)
            {
                if (floor > 0)
                {
                    ImGui::SameLine();
                }

                ImGui::BeginDisabled(mission.spawnedFloors[floor]);

                if (ImGui::Button(floors[floor]))
                {
                    spawnMercFloor(state, engine, floor);
                }

                ImGui::EndDisabled();
            }

            ImGui::SameLine();

            if (ImGui::Button("Kill mercs"))
            {
                NpcRoster& roster = state.simulation.npcs;

                for (std::size_t index = 0; index < roster.npcs.size(); ++index)
                {
                    if (roster.npcs[index].faction == NpcFaction::Merc && isNpcAlive(roster.npcs[index]))
                    {
                        damageNpc(roster, index, k_lethalDamage, roster.npcs[index].home, {}, engine.entities,
                            engine.physics, state.settings.npcs);
                    }
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Remove mercs"))
            {
                removeMercs(state, engine);
            }

            ImGui::Text("Beats reached 0x%04x  checkpoint %s", mission.state.reachedBeats,
                mission.checkpointBeat ? mission.beats[*mission.checkpointBeat].name.c_str() : "-");
        }

        void npcList(const GameState& state)
        {
            const NpcRoster& roster = state.simulation.npcs;

            if (!ImGui::TreeNode("NPCs", "NPCs (%zu)", roster.npcs.size()))
            {
                return;
            }

            for (const Npc& npc : roster.npcs)
            {
                ImGui::Text("%-18s %-6s %-6s %-6s %-9s hp %3d  rounds %2d  supp %.2f%s", npc.name.c_str(),
                    npc.faction == NpcFaction::Squad ? "squad" : "merc", toString(npc.mode), toString(npc.action),
                    toString(npc.moveReason), npc.health, npc.rounds, npc.suppression,
                    npc.canSeeTarget ? "  sees target" : "");
            }

            for (std::size_t group = 0; group < roster.groups.size(); ++group)
            {
                const NpcGroup& groupState = roster.groups[group];
                ImGui::Text("Group %zu: %u members, %u lost%s%s", group, groupState.members, groupState.losses,
                    groupState.isEngaged ? ", engaged" : "", groupState.isFallingBack ? ", falling back" : "");
            }

            ImGui::TreePop();
        }

        void npcTuning(NpcSettings& settings)
        {
            if (!ImGui::TreeNode("NPC tuning"))
            {
                return;
            }

            ImGui::SliderInt("Merc health", &settings.merc.health, 1, k_maxHealth);
            ImGui::SliderInt("Merc damage", &settings.merc.damage, 0, k_maxDamage);
            ImGui::SliderFloat("Merc spread (rad)", &settings.merc.spread, 0.0f, 0.2f);
            ImGui::SliderInt("Squad health", &settings.squad.health, 1, k_maxHealth);
            ImGui::SliderInt("Squad damage", &settings.squad.damage, 0, k_maxDamage);
            ImGui::SliderFloat("Squad spread (rad)", &settings.squad.spread, 0.0f, 0.2f);
            ImGui::SliderInt("Player damage to NPCs", &settings.playerDamage, 1, k_maxDamage);
            ImGui::SliderFloat("Hit flinch (s)", &settings.flinchSeconds, 0.0f, 1.0f);
            ImGui::SliderFloat("Stagger (s)", &settings.staggerSeconds, 0.0f, 2.0f);
            ImGui::SliderInt("Stagger damage", &settings.staggerDamage, 1, k_maxDamage);
            ImGui::SliderFloat("Stagger speed factor", &settings.staggerSpeed, 0.0f, 1.0f);
            ImGui::SliderFloat("Sight range (m)", &settings.sightRange, 1.0f, 80.0f);
            ImGui::SliderFloat("Sight half angle (rad)", &settings.sightHalfAngle, 0.1f, 3.14f);
            ImGui::SliderFloat("Hearing range (m)", &settings.hearingRange, 0.0f, 80.0f);
            ImGui::SliderFloat("Reaction min (s)", &settings.reactionMin, 0.0f, 3.0f);
            ImGui::SliderFloat("Reaction max (s)", &settings.reactionMax, 0.0f, 3.0f);
            ImGui::SliderFloat("Aim settle (s)", &settings.aimSettleSeconds, 0.0f, 3.0f);
            ImGui::SliderFloat("Shot interval (s)", &settings.shotInterval, 0.03f, 1.0f);
            ImGui::SliderInt("Burst min", &settings.burstMin, 1, 10);
            ImGui::SliderInt("Burst max", &settings.burstMax, 1, 15);
            ImGui::SliderFloat("Cover min (s)", &settings.coverMin, 0.0f, 6.0f);
            ImGui::SliderFloat("Cover max (s)", &settings.coverMax, 0.0f, 8.0f);
            ImGui::SliderFloat("Peek min (s)", &settings.peekMin, 0.2f, 6.0f);
            ImGui::SliderFloat("Peek max (s)", &settings.peekMax, 0.2f, 8.0f);
            ImGui::SliderFloat("Suppression threshold", &settings.suppressionThreshold, 0.1f, 3.0f);
            ImGui::SliderFloat("Flank every min (s)", &settings.flankMin, 2.0f, 60.0f);
            ImGui::SliderFloat("Flank every max (s)", &settings.flankMax, 2.0f, 90.0f);
            ImGui::SliderFloat("Fall back at losses", &settings.fallBackLosses, 0.1f, 1.0f);
            ImGui::SliderFloat("Walk speed", &settings.walkSpeed, 0.5f, 4.0f);
            ImGui::SliderFloat("Run speed", &settings.runSpeed, 1.0f, 6.0f);
            ImGui::SliderFloat("Squad follow (m)", &settings.squadFollowDistance, 1.0f, 10.0f);
            ImGui::TreePop();
        }

        void playerTuning(GameState& state)
        {
            if (!ImGui::TreeNode("Downed and ammo tuning"))
            {
                return;
            }

            DownedSettings& downed = state.settings.downed;
            AmmoSettings& ammo = state.settings.ammo;
            ImGui::SliderFloat("Bleed out (s)", &downed.bleedOutSeconds, 1.0f, 120.0f);
            ImGui::SliderFloat("Revive time (s)", &downed.reviveSeconds, 0.2f, 10.0f);
            ImGui::SliderFloat("Revive reach (m)", &downed.reviveRange, 0.5f, 5.0f);
            ImGui::SliderInt("Revive health", &downed.reviveHealth, 1, k_maxHealth);
            ImGui::SliderFloat("Restart delay (s)", &downed.restartDelay, 0.5f, 10.0f);
            ImGui::SliderInt("Magazine", &ammo.magazineSize, 1, k_maxRounds);
            ImGui::SliderInt("Max reserve", &ammo.maxReserve, 0, k_maxRounds);
            ImGui::SliderInt("Issued reserve", &ammo.issuedReserve, 0, k_maxRounds);
            ImGui::SliderFloat("Reload time (s)", &ammo.reloadSeconds, 0.1f, 5.0f);
            ImGui::SliderFloat("Empty reload time (s)", &ammo.emptyReloadSeconds, 0.1f, 5.0f);
            ImGui::TextDisabled("Crawl scale is shared with the client's prediction: change it offline.");
            ImGui::TreePop();
        }

        void testActions(GameState& state, EngineContext& engine)
        {
            if (ImGui::Button("Down Player1"))
            {
                hurtPawn(state, engine, k_hostPawn, k_lethalDamage, math::Vec3{0.0f, 0.0f, 0.0f});
            }

            ImGui::SameLine();

            if (ImGui::Button("Down Player2"))
            {
                hurtPawn(state, engine, k_clientPawn, k_lethalDamage, math::Vec3{0.0f, 0.0f, 0.0f});
            }

            for (std::size_t index = 0; index < state.simulation.pawns.size(); ++index)
            {
                const Pawn& pawn = state.simulation.pawns[index];
                ImGui::Text("Player%zu  %s  bleed %.1f  revive %.1f  ammo %d / %d%s", index + 1,
                    pawn.isDowned ? "DOWN" : "up", pawn.bleedSeconds, pawn.reviveSeconds, pawn.ammo.magazine,
                    pawn.ammo.reserve, isReloading(pawn.ammo) ? "  reloading" : "");
            }

            if (state.simulation.isRestarting)
            {
                ImGui::Text("Restarting in %.1f s", state.simulation.restartSeconds);
            }
        }
    } //namespace

    void combatDevSection(GameState& state, EngineContext& engine)
    {
        if (!ImGui::CollapsingHeader("Combat and NPCs"))
        {
            return;
        }

        if (engine.session.role() == net::Role::Client)
        {
            const PlayerStatus& own = state.local.statuses[k_clientPawn];
            ImGui::Text("Ammo %d / %d%s  %s  bleed %u s", own.magazine, own.reserve, own.isReloading ? " reloading" : "",
                own.isDowned ? "DOWN" : "up", own.bleedSeconds);
            ImGui::Text("NPCs mirrored: %zu", state.local.npcViews.npcs.size());
            ImGui::TextDisabled("Merc spawning and AI tuning are host-side");
            return;
        }

        mercControls(state, engine);
        testActions(state, engine);
        npcList(state);
        npcTuning(state.settings.npcs);
        playerTuning(state);
    }
} //namespace game

#endif
