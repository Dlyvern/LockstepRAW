#ifndef LOCKSTEP_GAME_CHARACTERS_NPC_PLACEHOLDERS_HPP
#define LOCKSTEP_GAME_CHARACTERS_NPC_PLACEHOLDERS_HPP

#include "Characters/GearLooks.hpp"
#include "Characters/PawnCharacters.hpp"
#include "Dialogue/Dialogue.hpp"

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // How a placeholder NPC stands, and so which looping clip it plays.
    // Idle:   unarmed body, "Idle".
    // Crouch: unarmed body, "CrouchIdle" (someone hiding).
    // Armed:  armed body, "RifleIdle" (weapon lowered).
    // Aim:    armed body, "AimIdle" (weapon up).
    enum class NpcPose : uint8_t
    {
        Idle = 0,
        Crouch,
        Armed,
        Aim
    };

    // A person standing in a level, marked by an "Npc_*" node (position and facing; the node's
    // -Z, Blender's +Y, is where they look). Drawn with the pawn character playing a looping clip
    // picked by its pose; no physics. Client-only presentation: each machine draws and voices its
    // own copies.
    // name:        the node's name.
    // barkPrefix:  id prefix of the lines they say when the local player comes close, e.g.
    //              "station.sergeant.idle."; empty for someone who never speaks.
    // placement:   where the character stands and which way it faces.
    // pose:        how they stand (from the role, or a pose suffix of the node name).
    // gearLook:    their head gear (from the role, or a look suffix of the node name).
    // phase:       0 to 1: where in its clip the NPC starts, so neighbours don't move in step.
    // instance:    pose and animator.
    // isStarted:   the clip was started (at `phase`).
    // cooldown:    Player seconds until they may speak again.
    // isNear:      the local player stood within reach on the last tick.
    struct NpcPlaceholder
    {
        std::string name;
        std::string barkPrefix;
        math::Transform placement{};
        NpcPose pose{NpcPose::Idle};
        GearLook gearLook{GearLook::Full};
        float phase{0.0f};
        lockstep::anim::SkinnedInstance instance{};
        bool isStarted{false};
        float cooldown{0.0f};
        bool isNear{false};
    };

    // A level's NPC placeholders and the random state that picks their lines.
    // npcs:   the placeholders.
    // random: xorshift state for line choice (presentation only, never replicated).
    struct NpcPlaceholders
    {
        std::vector<NpcPlaceholder> npcs;
        uint32_t random{0x9E3779B9u};
    };

    // Finds the "Npc_*" markers of a level. The role comes from the name: Npc_DeskSergeant
    // ("sergeant"), Npc_Armorer ("armorer"), Npc_RangeOfficer ("range"), Npc_Guard ("guard"),
    // Npc_Officer_* ("npc"), Npc_Prisoner_* (lines "<level>.prisoner.*"); they say
    // "<level>.<role>.idle.*", where <level> is the level name up to any '_' (hospital_a and
    // hospital_b speak hospital.* lines) and Npc_CordonOfficer's role is "cordon". Npc_Chief
    // stays silent (he speaks through the briefing).
    // Npc_Squad_<n> markers are skipped: they are the armed squad officers (Npcs/).
    // Poses by role: Npc_Civilian_* and Npc_Survivor_* crouch (hiding); Npc_SquadOutside_*,
    // Npc_CordonOfficer and Npc_Guard are armed; the rest idle. A
    // name part "_Idle", "_Crouch", "_Armed" or "_Aim" (any case, e.g. Npc_Civilian_2_Idle)
    // overrides the pose. Civilians, survivors and prisoners wear only the balaclava; everyone else
    // gets squadGearLook() (both honour "_Full", "_NoNVG", "_Balaclava").
    // scene: the loaded level.
    // level: the level's short name ("station"), the first part of line ids.
    // Returns: the placeholders (none without markers).
    [[nodiscard]] NpcPlaceholders findNpcPlaceholders(const lockstep::assets::Scene& scene, std::string_view level);

    // Lets an NPC speak a random line of its set when the local player comes within ~2.5 m of
    // it, unless a subtitle is already on screen or it spoke recently.
    // placeholders: the level's placeholders.
    // dialogue:     the level's dialogue.
    // subtitles:    the local subtitles.
    // localFoot:    the local pawn's foot position, or nothing while it has none.
    // playerDelta:  Player domain delta of this tick.
    void updateNpcBarks(NpcPlaceholders& placeholders, DialogueBook& dialogue, Subtitles& subtitles,
        const std::optional<math::Vec3>& localFoot, float playerDelta);

    // Animates and queues every placeholder for drawing: the unarmed or the armed body, by pose,
    // each looping its pose's clip from its own phase.
    // renderer:     the renderer.
    // placeholders: the level's placeholders.
    // characters:   the pawn characters; nothing is drawn without them. Armed poses use the
    //               unarmed body when the armed one is missing.
    // frameSeconds: presentation time since the last frame.
    void drawNpcPlaceholders(lockstep::render::Renderer& renderer, NpcPlaceholders& placeholders,
        const PawnCharacters* characters, float frameSeconds);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_NPC_PLACEHOLDERS_HPP
