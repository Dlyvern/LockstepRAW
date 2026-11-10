#include "Gore/HeadBurstDevTools.hpp"

#if LOCKSTEP_DEV_TOOLS

#include "GameState.hpp"
#include "Gore/CorpseHits.hpp"
#include "Gore/GoreRules.hpp"
#include "Npcs/NpcRoster.hpp"
#include "Simulation/HostSimulation.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace game
{
    namespace
    {
        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
    } //namespace

    void runHeadBurstDrill(GameState& state, EngineContext& engine)
    {
        constexpr std::string_view k_drillFlag{"--dev-head-burst"};
        constexpr std::string_view k_pieceFlag{"--dev-piece-drill"};
        constexpr uint64_t k_pieceShots{60};
        constexpr uint64_t k_pieceShotEvery{15};
        constexpr uint64_t k_firstTick{300};
        constexpr uint64_t k_interval{240};
        constexpr int32_t k_lethal{1000};
        constexpr float k_eyeHeight{1.6f};
        constexpr float k_headHeight{1.62f};
        constexpr float k_bodyHeight{0.3f};
        const auto hasFlag = [&](std::string_view flag)
        {
            return std::any_of(engine.arguments.begin(), engine.arguments.end(),
                [&](const char* argument) { return argument != nullptr && std::string_view(argument) == flag; });
        };
        const bool isPieceDrill = hasFlag(k_pieceFlag);

        if (!hasFlag(k_drillFlag) && !isPieceDrill)
        {
            return;
        }

        state.settings.gore.headBurstAlways = !isPieceDrill;
        state.settings.gore.headBurstChance = isPieceDrill ? 0.0f : state.settings.gore.headBurstChance;
        state.settings.gore.headBurstBonus = isPieceDrill ? 0.0f : state.settings.gore.headBurstBonus;
        const std::optional<math::Vec3> foot = engine.physics.footPosition(state.simulation.pawns[k_hostPawn].controller);
        NpcRoster& roster = state.simulation.npcs;

        if (!foot || state.lastTick < k_firstTick)
        {
            return;
        }

        const math::Vec3 eye = *foot + k_up * k_eyeHeight;
        std::optional<std::size_t> nearest;
        std::optional<std::size_t> nearestAlive;
        float best = 1.0e9f;
        float bestAlive = 1.0e9f;

        for (std::size_t index = 0; index < roster.npcs.size(); ++index)
        {
            const std::optional<math::Vec3> standing = npcFoot(roster.npcs[index], engine.entities);
            const float distance = standing ? math::length(*standing - *foot) : 1.0e9f;
            nearest = distance < best ? std::optional<std::size_t>(index) : nearest;
            best = std::min(best, distance);
            const bool isAlive = isNpcAlive(roster.npcs[index]);
            nearestAlive = isAlive && distance < bestAlive ? std::optional<std::size_t>(index) : nearestAlive;
            bestAlive = isAlive ? std::min(bestAlive, distance) : bestAlive;
        }

        const bool isShot = state.lastTick % k_interval == 0 && nearestAlive;
        const std::optional<std::size_t> target = isShot ? nearestAlive : nearest;
        const std::optional<math::Vec3> targetFoot = target ? npcFoot(roster.npcs[*target], engine.entities)
                                                            : std::nullopt;

        if (!targetFoot)
        {
            return;
        }

        const uint64_t sinceShot = state.lastTick % k_interval;

        if (isPieceDrill && !isShot && !state.gore.pieces.empty() && sinceShot >= k_pieceShots
            && sinceShot % k_pieceShotEvery == 0)
        {
            const math::Vec3 piece = state.gore.pieces.back().current.position
                + math::Vec3{0.0f, 0.1f, 0.0f};
            const math::Vec3 aim = math::normalize(piece - eye);
            state.local.controls.yaw = std::atan2(-aim.x, -aim.z);
            state.local.controls.pitch = std::asin(std::clamp(aim.y, -1.0f, 1.0f));
            fireAtBodies(state, engine, eye, aim);
            return;
        }

        const bool isTargetAlive = isNpcAlive(roster.npcs[*target]);
        const math::Vec3 head = *targetFoot + k_up * (isTargetAlive ? k_headHeight : k_bodyHeight);
        const math::Vec3 toward = math::normalize(head - eye);
        state.local.controls.yaw = std::atan2(-toward.x, -toward.z);
        state.local.controls.pitch = std::asin(std::clamp(toward.y, -1.0f, 1.0f));

        if (!isShot)
        {
            return;
        }

        const bool killed = damageNpc(roster, *target, k_lethal, eye, NpcTargetRef{}, engine.entities, engine.physics,
            state.settings.npcs);
        static_cast<void>(recordNpcGore(state, NpcGoreHit{.npc = *target, .point = head, .direction = toward,
            .damage = state.settings.npcs.playerDamage, .killed = killed, .shooter = k_hostPawn,
            .distance = math::length(head - eye)}));
    }

    void headBurstDevSection(HeadBurstTuning& tuning)
    {
        if (!ImGui::TreeNode("Head bursts"))
        {
            return;
        }

        int gibs = static_cast<int>(tuning.gibsPerBurst);
        int maxGibs = static_cast<int>(tuning.maxGibs);
        int rays = static_cast<int>(tuning.splatterRays);

        if (ImGui::SliderInt("Gibs per burst", &gibs, 0, 30))
        {
            tuning.gibsPerBurst = static_cast<uint32_t>(gibs);
        }

        if (ImGui::SliderInt("Max gibs", &maxGibs, 0, 200))
        {
            tuning.maxGibs = static_cast<uint32_t>(maxGibs);
        }

        if (ImGui::SliderInt("Splatter rays", &rays, 0, 48))
        {
            tuning.splatterRays = static_cast<uint32_t>(rays);
        }

        ImGui::SliderFloat("Gib lifetime (s)", &tuning.gibSeconds, 5.0f, 300.0f);
        ImGui::SliderFloat("Gib speed (m/s)", &tuning.gibSpeed, 0.0f, 12.0f);
        ImGui::SliderFloat("Gib size (m)", &tuning.gibSize, 0.01f, 0.12f);
        ImGui::SliderFloat("Neck spurt (s)", &tuning.fountainSeconds, 0.0f, 4.0f);
        ImGui::SliderFloat("Spurt pulse (s)", &tuning.fountainPulse, 0.05f, 0.5f);
        ImGui::SliderFloat("Splatter reach (m)", &tuning.splatterReach, 0.5f, 10.0f);
        ImGui::SliderFloat("Splatter size (m)", &tuning.splatterSize, 0.2f, 4.0f);
        ImGui::SliderFloat("Body push (kg m/s)", &tuning.push, 0.0f, 600.0f);
        ImGui::SliderFloat("Droplets", &tuning.dropletScale, 0.0f, 12.0f);
        ImGui::SliderFloat("Particles", &tuning.particleScale, 0.0f, 8.0f);
        ImGui::SliderFloat("Splatter volume", &tuning.volume, 0.0f, 2.0f);
        ImGui::TreePop();
    }
} //namespace game

#endif
