#include "Melee/KickPresentation.hpp"

#include "GameState.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <limits>

namespace game
{
    namespace
    {
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_behindTolerance{0.3f};

        bool isInReach(const KickEvent& event, const math::Vec3& point, float reach)
        {
            const math::Vec3 offset = point - event.origin;

            return math::length(offset) <= reach && math::dot(offset, event.direction) > -k_behindTolerance;
        }

        bool wasPushed(const ShownKick& kick, world::EntityId entity)
        {
            return std::find(kick.pushed.begin(), kick.pushed.end(), entity) != kick.pushed.end();
        }

        void pushLooks(GameState& state, EngineContext& engine, ShownKick& kick)
        {
            const KickPresentationTuning& tuning = state.kicks.tuning;
            const KickEvent& event = kick.event;

            for (NpcLook& look : state.npcLooks.looks)
            {
                if (wasPushed(kick, look.entity))
                {
                    continue;
                }

                if (!look.ragdoll)
                {
                    const math::Vec3& placed = look.modelTransform.position;
                    const math::Vec3 chest{placed.x, event.origin.y, placed.z};

                    if (isInReach(event, chest, tuning.reach))
                    {
                        reactToNpcHit(state.npcLooks, look, ReactionHit{.point = chest, .direction = event.direction,
                            .damage = static_cast<int32_t>(tuning.reactionDamage)});
                        kick.pushed.push_back(look.entity);
                    }

                    continue;
                }

                if (look.ragdoll->poses.empty())
                {
                    continue;
                }

                uint32_t nearest = 0;
                float nearestDistance = std::numeric_limits<float>::max();

                for (uint32_t body = 0; body < look.ragdoll->poses.size(); ++body)
                {
                    const float distance = math::length(ragdollBodyCentre(*look.ragdoll, body) - event.origin);

                    if (distance < nearestDistance)
                    {
                        nearest = body;
                        nearestDistance = distance;
                    }
                }

                const math::Vec3 centre = ragdollBodyCentre(*look.ragdoll, nearest);

                if (!isInReach(event, centre, tuning.reach))
                {
                    continue;
                }

                pushNpcBody(engine.physics, state.npcLooks, look, nearest,
                    event.direction * tuning.ragdollImpulse + k_up * tuning.ragdollLift, centre);
                kick.pushed.push_back(look.entity);
            }
        }

        void pushPieces(GameState& state, EngineContext& engine, const KickEvent& event)
        {
            const KickPresentationTuning& tuning = state.kicks.tuning;
            const math::Vec3 push = event.direction * tuning.pieceSpeed + k_up * tuning.pieceLift;

            for (std::vector<SeveredPiece>* pieces : {&state.gore.pieces, &state.npcLooks.weapons.weapons})
            {
                for (const SeveredPiece& piece : *pieces)
                {
                    const std::optional<math::Transform> body = engine.physics.transform(piece.body);

                    if (body && isInReach(event, body->position, tuning.reach))
                    {
                        pushPiece(engine.physics, piece, body->position, push);
                    }
                }
            }
        }
    } //namespace

    void presentKicks(GameState& state, EngineContext& engine, float seconds)
    {
        KickEffects& effects = state.kicks;

        for (const KickEvent& event : effects.pending)
        {
            effects.active.push_back(ShownKick{.event = event, .seconds = 0.0f, .pushed = {}, .isShown = false});
        }

        effects.pending.clear();

        for (ShownKick& kick : effects.active)
        {
            if (!kick.isShown)
            {
                kick.isShown = true;
                pushPieces(state, engine, kick.event);

                if (kick.event.isHit)
                {
                    playKickHit(state.footsteps, engine.audio, kick.event.origin + kick.event.direction * 0.5f,
                        engine.timeDomains.scale(world::TimeDomain::World));
                }
            }

            pushLooks(state, engine, kick);
            kick.seconds += seconds;
        }

        std::erase_if(effects.active, [&effects](const ShownKick& kick)
        {
            return kick.seconds > effects.tuning.windowSeconds;
        });
    }

    bool receiveKickMessage(KickEffects& effects, std::span<const std::byte> message)
    {
        const std::optional<KickEvent> event = decodeKickEvent(message);

        if (!event)
        {
            return false;
        }

        effects.pending.push_back(*event);
        return true;
    }

#if LOCKSTEP_DEV_TOOLS
    void kickEffectsDevSection(KickEffects& effects)
    {
        if (!ImGui::TreeNode("Kick effects"))
        {
            return;
        }

        KickPresentationTuning& tuning = effects.tuning;
        ImGui::Text("Kicks shown %zu", effects.active.size());
        ImGui::SliderFloat("Ragdoll impulse (kg m/s)", &tuning.ragdollImpulse, 0.0f, 800.0f);
        ImGui::SliderFloat("Ragdoll lift (kg m/s)", &tuning.ragdollLift, 0.0f, 300.0f);
        ImGui::SliderFloat("Piece speed (m/s)", &tuning.pieceSpeed, 0.0f, 20.0f);
        ImGui::SliderFloat("Piece lift (m/s)", &tuning.pieceLift, 0.0f, 10.0f);
        ImGui::SliderFloat("Reach (m)", &tuning.reach, 0.3f, 3.0f);
        ImGui::SliderFloat("NPC reaction strength", &tuning.reactionDamage, 0.0f, 200.0f);
        ImGui::SliderFloat("Window (s)", &tuning.windowSeconds, 0.0f, 1.0f);
        ImGui::TreePop();
    }
#endif
} //namespace game
