#include "Gore/GoreDevPanel.hpp"

#include "Gore/HeadBurstDevTools.hpp"

#if LOCKSTEP_DEV_TOOLS

#include "GameState.hpp"
#include "Gore/GoreRules.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>

#include <imgui.h>

#include <limits>
#include <optional>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace net = lockstep::net;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr int k_maxSeverDamage{200};
        constexpr int k_maxPieces{64};
        constexpr int k_maxDecals{600};
        constexpr int k_maxDroplets{2000};

        const NpcLook* nearestLook(const GameState& state)
        {
            const NpcLook* nearest = nullptr;
            float nearestDistance = std::numeric_limits<float>::max();

            for (const NpcLook& look : state.npcLooks.looks)
            {
                if (look.model == nullptr || !anim::hasLimbs(*look.model))
                {
                    continue;
                }

                const float distance = math::lengthSquared(drawnPlacement(look).position - state.gore.eye);

                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    nearest = &look;
                }
            }

            return nearest;
        }

        std::optional<world::EntityId> hostEntityOf(const EngineContext& engine, world::EntityId local)
        {
            if (engine.session.role() != net::Role::Client)
            {
                return local;
            }

            for (const net::MirroredEntity& mirrored : engine.replication.mirrored())
            {
                if (mirrored.entity == local)
                {
                    return mirrored.hostEntity;
                }
            }

            return std::nullopt;
        }

        void severNearest(GameState& state, const EngineContext& engine)
        {
            const NpcLook* look = nearestLook(state);

            if (look == nullptr)
            {
                return;
            }

            const SeverSource source{.model = *look->model, .instance = look->instance,
                .transform = drawnPlacement(*look), .gearParts = look->gearParts, .severed = look->severedLimbs};
            std::vector<anim::Limb> candidates;

            for (const anim::LimbInfo& info : look->model->limbs)
            {
                if (canSever(source, info.limb))
                {
                    candidates.push_back(info.limb);
                }
            }

            const std::optional<world::EntityId> target = hostEntityOf(engine, look->entity);

            if (candidates.empty() || !target)
            {
                return;
            }

            const auto pick = static_cast<std::size_t>(state.gore.random.next() * static_cast<float>(candidates.size()));
            const anim::Limb limb = candidates[std::min(pick, candidates.size() - 1)];
            const math::Vec3 point = drawnPlacement(*look).position + math::Vec3{0.0f, 1.2f, 0.0f};

            if (engine.session.role() == net::Role::Client)
            {
                state.gore.pending.push_back(GoreEvent{.kind = GoreEventKind::Sever, .target = *target, .limb = limb,
                    .point = point, .direction = math::Vec3{0.0f, 0.3f, -1.0f}, .seed = state.gore.random.state});
                return;
            }

            recordSever(state, *target, limb, point);
        }

        void tuningSliders(GoreTuning& tuning)
        {
            int maxPieces = static_cast<int>(tuning.maxPieces);
            int maxDecals = static_cast<int>(tuning.maxDecals);
            int maxDroplets = static_cast<int>(tuning.maxDroplets);
            int hitDroplets = static_cast<int>(tuning.hitDroplets);
            ImGui::SliderInt("Max pieces", &maxPieces, 0, k_maxPieces);
            ImGui::SliderFloat("Piece lifetime (s)", &tuning.pieceSeconds, 2.0f, 300.0f);
            ImGui::SliderFloat("Piece speed (m/s)", &tuning.pieceSpeed, 0.0f, 15.0f);
            ImGui::SliderInt("Max decals", &maxDecals, 0, k_maxDecals);
            ImGui::SliderFloat("Splatter size (m)", &tuning.splatterSize, 0.1f, 3.0f);
            ImGui::SliderFloat("Splatter reach (m)", &tuning.splatterReach, 0.5f, 8.0f);
            ImGui::SliderFloat("Pool size (m)", &tuning.poolSize, 0.2f, 4.0f);
            ImGui::SliderFloat("Pool spread (s)", &tuning.poolSeconds, 0.5f, 30.0f);
            ImGui::SliderInt("Max droplets", &maxDroplets, 0, k_maxDroplets);
            ImGui::SliderInt("Droplets per hit", &hitDroplets, 0, 100);
            ImGui::SliderFloat("Bleed time (s)", &tuning.bleedSeconds, 0.0f, 60.0f);
            ImGui::SliderFloat("Drip interval (s)", &tuning.dripInterval, 0.02f, 1.0f);
            tuning.maxPieces = static_cast<uint32_t>(maxPieces);
            tuning.maxDecals = static_cast<uint32_t>(maxDecals);
            tuning.maxDroplets = static_cast<uint32_t>(maxDroplets);
            tuning.hitDroplets = static_cast<uint32_t>(hitDroplets);
        }
    } //namespace

    void goreDevSection(GameState& state, EngineContext& engine)
    {
        if (!ImGui::CollapsingHeader("Gore"))
        {
            return;
        }

        if (engine.session.role() != net::Role::Client)
        {
            GoreRules& rules = state.settings.gore;
            ImGui::SliderInt("Sever damage", &rules.severDamage, 1, k_maxSeverDamage);
            ImGui::SliderFloat("Sever chance", &rules.severChance, 0.0f, 1.0f);
            ImGui::Checkbox("Headshot kills sever the head", &rules.headshotSevers);
            ImGui::Checkbox("NPC shots sever", &rules.npcSevers);
            ImGui::SliderFloat("Head burst chance", &rules.headBurstChance, 0.0f, 1.0f);
            ImGui::SliderFloat("Head burst close range (m)", &rules.headBurstRange, 0.0f, 20.0f);
            ImGui::SliderFloat("Head burst close bonus", &rules.headBurstBonus, 0.0f, 1.0f);
            ImGui::Checkbox("Head shots always burst", &rules.headBurstAlways);
        }

        tuningSliders(state.gore.tuning);
        bloodBurstDevSection(state.gore.bursts);
        headBurstDevSection(state.gore.headBursts.tuning);
        hitReactionDevSection(state.npcLooks.reactionTuning);
        const GoreState& gore = state.gore;
        ImGui::Text("Pieces %zu  stumps %zu  decals %zu  droplets %zu%s", gore.pieces.size(), gore.stumps.size(),
            gore.decals.decals.size(), gore.spray.droplets.size(), gore.assets ? "" : "  (blood unavailable)");

        if (ImGui::Button("Sever random limb (nearest NPC)"))
        {
            severNearest(state, engine);
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear blood"))
        {
            clearGore(state.gore, engine.physics);
        }
    }
} //namespace game

#endif
