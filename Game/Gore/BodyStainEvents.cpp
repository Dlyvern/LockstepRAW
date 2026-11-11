#include "Gore/BodyStainEvents.hpp"

#include "GameState.hpp"
#include "Player/LocalPlayer.hpp"

#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/World/Entities.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;
        namespace world = lockstep::world;
        namespace anim = lockstep::anim;
        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr float k_headBurstScale{1.5f};
        constexpr float k_stumpDirt{0.2f};
        constexpr float k_viewModelGrime{0.5f};
        constexpr float k_seedScale{7.31f};
        constexpr std::array<std::pair<anim::Limb, float>, 4> k_drillStains{{{anim::Limb::Torso, 0.2f},
            {anim::Limb::ForearmLeft, 0.12f}, {anim::Limb::ThighRight, 0.16f}, {anim::Limb::Head, 0.1f}}};

        bool isInside(std::span<const InteriorBox> boxes, const math::Vec3& point)
        {
            return std::any_of(boxes.begin(), boxes.end(), [&point](const InteriorBox& box)
            {
                return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y && point.y <= box.max.y
                    && point.z >= box.min.z && point.z <= box.max.z;
            });
        }

        float rainAt(const GameState& state, const math::Vec3& position)
        {
            return isInside(state.combatAudio.interiors, position) ? 0.0f : state.weather.wetness;
        }

        std::optional<math::Vec3> limbRootWorld(const anim::SkinnedModel* model, const anim::SkinnedInstance& instance,
            const math::Transform& placement, anim::Limb limb)
        {
            const anim::LimbInfo* info = model != nullptr ? anim::findLimb(*model, limb) : nullptr;
            const std::span<const math::Mat4> joints = instance.jointMatrices();

            if (info == nullptr || info->rootJoint >= joints.size())
            {
                return std::nullopt;
            }

            const math::Vec3 local{joints[info->rootJoint][3][0], joints[info->rootJoint][3][1],
                joints[info->rootJoint][3][2]};
            const math::Vec3 scaled{local.x * placement.scale.x, local.y * placement.scale.y, local.z * placement.scale.z};
            return placement.position + placement.rotation * scaled;
        }

        void stainNpc(const CharacterStainTuning& tuning, NpcLook& look, const GoreEvent& event)
        {
            const math::Transform& placement = drawnPlacement(look);

            if (event.kind == GoreEventKind::Hit)
            {
                addBodyStain(look.stains, look.instance, placement, event.point, tuning.hitRadius, tuning.hitDirt);
                return;
            }

            const anim::Limb limb = event.kind == GoreEventKind::HeadBurst ? anim::Limb::Head : event.limb;
            const std::optional<math::Vec3> stump = limbRootWorld(look.model, look.instance, placement, limb);
            const float scale = event.kind == GoreEventKind::HeadBurst ? k_headBurstScale : 1.0f;
            addBodyStain(look.stains, look.instance, placement, stump.value_or(event.point), tuning.severRadius * scale,
                k_stumpDirt);
        }

        void stainForDrill(const CharacterStainTuning& tuning, BodyStains& stains,
            const anim::SkinnedInstance& instance, const math::Transform& placement, const anim::SkinnedModel* model)
        {
            const std::span<const math::Mat4> joints = instance.jointMatrices();

            if (!tuning.drill || stains.count != 0 || joints.empty())
            {
                return;
            }

            for (std::size_t index = 0; index < k_drillStains.size(); ++index)
            {
                const auto& [limb, radius] = k_drillStains[index];
                const std::size_t fallback = joints.size() * index / k_drillStains.size();
                const math::Vec3 local{joints[fallback][3][0], joints[fallback][3][1], joints[fallback][3][2]};
                const math::Vec3 scaled{local.x * placement.scale.x, local.y * placement.scale.y,
                    local.z * placement.scale.z};
                const math::Vec3 point = limbRootWorld(model, instance, placement, limb)
                    .value_or(placement.position + placement.rotation * scaled);
                addBodyStain(stains, instance, placement, point, radius, tuning.hitDirt);
            }
        }
    } //namespace

    void stainStruckBody(GameState& state, const EngineContext& engine, std::optional<world::EntityId> entity,
        const GoreEvent& event)
    {
        if (!entity)
        {
            return;
        }

        const CharacterStainTuning& tuning = state.gore.stains;
        const auto look = std::find_if(state.npcLooks.looks.begin(), state.npcLooks.looks.end(),
            [&entity](const NpcLook& candidate) { return candidate.entity == *entity; });

        if (look != state.npcLooks.looks.end())
        {
            stainNpc(tuning, *look, event);
            return;
        }

        const world::Entity* found = engine.entities.find(*entity);

        if (!state.pawnCharacters || event.kind != GoreEventKind::Hit || found == nullptr
            || found->owner() == world::EntityOwner::Host)
        {
            return;
        }

        PawnCharacter& pawn = state.pawnCharacters->pawns[found->owner() == world::EntityOwner::Player2 ? 1 : 0];
        addBodyStain(pawn.stains, pawn.instance, pawn.placement, event.point, tuning.hitRadius, tuning.hitDirt);
    }

    void prepareCharacterStains(GameState& state, EngineContext& engine, float seconds)
    {
        const CharacterStainTuning& tuning = state.gore.stains;
        render::Renderer& renderer = engine.renderer;

        for (NpcLook& look : state.npcLooks.looks)
        {
            look.stains.seed = static_cast<float>(look.entity.index) * k_seedScale;
            stainForDrill(tuning, look.stains, look.instance, drawnPlacement(look), look.model);
            prepareBodyStains(renderer, look.stains, tuning, rainAt(state, drawnPlacement(look).position), seconds);
        }

        for (std::size_t index = 0; index < state.npcs.npcs.size(); ++index)
        {
            NpcPlaceholder& npc = state.npcs.npcs[index];
            npc.stains.seed = static_cast<float>(index + 3) * k_seedScale;
            stainForDrill(tuning, npc.stains, npc.instance, npc.placement, nullptr);
            prepareBodyStains(renderer, npc.stains, tuning, rainAt(state, npc.placement.position), seconds);
        }

        if (!state.pawnCharacters)
        {
            return;
        }

        PawnCharacters& characters = *state.pawnCharacters;

        for (std::size_t index = 0; index < characters.pawns.size(); ++index)
        {
            PawnCharacter& pawn = characters.pawns[index];
            pawn.stains.seed = static_cast<float>(index + 1) * k_seedScale;
            prepareBodyStains(renderer, pawn.stains, tuning, rainAt(state, pawn.placement.position), seconds);
        }

        const BodyStains& own = characters.pawns[std::min(localPawnIndex(engine), characters.pawns.size() - 1)].stains;
        characters.viewModel.surface = tuning.enabled
            ? renderer.submitCharacterSurface(render::CharacterSurface{.soak = own.soak,
                .grime = tuning.grime * k_viewModelGrime, .noiseSeed = own.seed})
            : render::k_noCharacterSurface;
    }
} //namespace game
