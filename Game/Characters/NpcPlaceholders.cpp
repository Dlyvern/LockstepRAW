#include "Characters/NpcPlaceholders.hpp"

#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iostream>
#include <span>
#include <utility>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace assets = lockstep::assets;

        constexpr std::string_view k_npcPrefix{"Npc_"};
        constexpr std::string_view k_armedSquadPrefix{"Npc_Squad_"};
        constexpr std::array<std::pair<std::string_view, std::string_view>, 7> k_roles{{
            {"Npc_Chief", "chief"},
            {"Npc_DeskSergeant", "sergeant"},
            {"Npc_Armorer", "armorer"},
            {"Npc_RangeOfficer", "range"},
            {"Npc_Guard", "guard"},
            {"Npc_Officer", "npc"},
            {"Npc_Prisoner", "prisoner"}}};
        constexpr std::string_view k_prisonerRole{"prisoner"};
        constexpr std::string_view k_chiefRole{"chief"};
        constexpr std::string_view k_idleSuffix{".idle."};
        constexpr std::array<std::pair<std::string_view, std::string_view>, 1> k_lineRoles{{
            {"cordonofficer", "cordon"}}};
        constexpr math::Vec3 k_upAxis{0.0f, 1.0f, 0.0f};
        constexpr float k_barkReach{2.5f};
        constexpr float k_barkHeight{1.5f};
        constexpr float k_barkCooldownSeconds{20.0f};
        constexpr float k_idleFadeSeconds{0.0f};
        constexpr float k_idleSpeedSpread{0.15f};
        constexpr std::array<std::string_view, 2> k_crouchedRoles{"civilian", "survivor"};
        constexpr std::array<std::string_view, 3> k_civilianRoles{"civilian", "survivor", "prisoner"};
        constexpr std::array<std::string_view, 3> k_armedRoles{"squadoutside", "cordonofficer", "guard"};
        constexpr std::array<std::pair<std::string_view, NpcPose>, 4> k_poseSuffixes{{
            {"idle", NpcPose::Idle},
            {"crouch", NpcPose::Crouch},
            {"armed", NpcPose::Armed},
            {"aim", NpcPose::Aim}}};
        constexpr std::array<std::string_view, 4> k_poseClips{"Idle", "CrouchIdle", "RifleIdle", "AimIdle"};

        std::string lowered(std::string_view text)
        {
            std::string result(text);

            for (char& character : result)
            {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }

            return result;
        }

        bool contains(std::span<const std::string_view> roles, std::string_view role)
        {
            return std::find(roles.begin(), roles.end(), role) != roles.end();
        }

        std::optional<NpcPose> poseSuffix(std::string_view name)
        {
            std::string_view rest = name.substr(k_npcPrefix.size());
            rest = rest.substr(0, rest.find('.'));

            while (!rest.empty())
            {
                const std::size_t separator = rest.find('_');
                const std::string part = lowered(rest.substr(0, separator));

                for (const auto& [suffix, pose] : k_poseSuffixes)
                {
                    if (part == suffix)
                    {
                        return pose;
                    }
                }

                rest = separator == std::string_view::npos ? std::string_view{} : rest.substr(separator + 1);
            }

            return std::nullopt;
        }

        NpcPose poseOf(std::string_view name, std::string_view role)
        {
            if (const std::optional<NpcPose> pose = poseSuffix(name))
            {
                return *pose;
            }

            if (contains(k_crouchedRoles, role))
            {
                return NpcPose::Crouch;
            }

            return contains(k_armedRoles, role) ? NpcPose::Armed : NpcPose::Idle;
        }

        float startPhase(std::string_view name)
        {
            uint32_t hash = 2166136261u;

            for (const char character : name)
            {
                hash = (hash ^ static_cast<unsigned char>(character)) * 16777619u;
            }

            return static_cast<float>(hash % 1000u) / 1000.0f;
        }

        bool isArmedPose(NpcPose pose)
        {
            return pose == NpcPose::Armed || pose == NpcPose::Aim;
        }

        const CharacterModel& modelFor(NpcPose pose, const PawnCharacters& characters)
        {
            return isArmedPose(pose) && characters.armed ? characters.armed->body : characters.unarmed;
        }

        uint32_t clipFor(const anim::SkinnedModel& model, NpcPose pose, const PawnCharacters& characters)
        {
            const std::optional<uint32_t> clip = anim::findClip(model.clips,
                k_poseClips[static_cast<std::size_t>(pose)]);
            return clip.value_or(characters.locomotion.idle);
        }

        std::string roleOf(std::string_view name)
        {
            for (const auto& [prefix, role] : k_roles)
            {
                if (name.starts_with(prefix))
                {
                    return std::string(role);
                }
            }

            const std::string_view rest = name.substr(k_npcPrefix.size());
            return lowered(rest.substr(0, std::min(rest.find('_'), rest.find('.'))));
        }

        std::string barkPrefix(std::string_view level, std::string_view role)
        {
            if (role == k_chiefRole)
            {
                return {};
            }

            std::string_view lineRole = role;

            for (const auto& [name, lines] : k_lineRoles)
            {
                lineRole = role == name ? lines : lineRole;
            }

            const std::string base = std::string(level.substr(0, level.find('_'))) + "." + std::string(lineRole);
            return role == k_prisonerRole ? base + "." : base + std::string(k_idleSuffix);
        }

        uint32_t nextRandom(uint32_t& state)
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;

            return state;
        }

        bool isWithinReach(const NpcPlaceholder& npc, const math::Vec3& foot)
        {
            const math::Vec3 offset = foot - npc.placement.position;

            return std::abs(offset.y) <= k_barkHeight
                && math::length(math::Vec3{offset.x, 0.0f, offset.z}) <= k_barkReach;
        }

        float idleSpeed(std::size_t index)
        {
            const float spread = static_cast<float>(index % 5) / 4.0f - 0.5f;
            return 1.0f + spread * 2.0f * k_idleSpeedSpread;
        }
    } //namespace

    NpcPlaceholders findNpcPlaceholders(const assets::Scene& scene, std::string_view level)
    {
        NpcPlaceholders placeholders{};

        for (const assets::SceneNode& node : scene.nodes)
        {
            const std::string_view name = node.name;
            const bool isArmedSquad = name.starts_with(k_armedSquadPrefix) && name.size() > k_armedSquadPrefix.size()
                && std::isdigit(static_cast<unsigned char>(name[k_armedSquadPrefix.size()])) != 0;

            if (!name.starts_with(k_npcPrefix) || isArmedSquad)
            {
                continue;
            }

            const std::string role = roleOf(node.name);
            placeholders.npcs.push_back(NpcPlaceholder{.name = node.name,
                .barkPrefix = barkPrefix(level, role),
                .placement = math::Transform{.position = node.transform.position,
                    .rotation = node.transform.rotation * math::angleAxis(math::k_pi, k_upAxis)},
                .pose = poseOf(name, role),
                .gearLook = contains(k_civilianRoles, role) ? gearLookSuffix(name).value_or(GearLook::Balaclava)
                                                            : squadGearLook(name),
                .phase = startPhase(name)});
        }

        if (!placeholders.npcs.empty())
        {
            std::clog << "Game: " << placeholders.npcs.size() << " NPC placeholders\n";
        }

        return placeholders;
    }

    void updateNpcBarks(NpcPlaceholders& placeholders, DialogueBook& dialogue, Subtitles& subtitles,
        const std::optional<math::Vec3>& localFoot, float playerDelta)
    {
        for (NpcPlaceholder& npc : placeholders.npcs)
        {
            npc.cooldown = std::max(npc.cooldown - playerDelta, 0.0f);
            const bool isNear = localFoot && isWithinReach(npc, *localFoot);
            const bool hasArrived = isNear && !npc.isNear;
            npc.isNear = isNear;

            if (!hasArrived || npc.cooldown > 0.0f || isSpeaking(subtitles) || npc.barkPrefix.empty())
            {
                continue;
            }

            const std::vector<const DialogueLine*> lines = linesWithPrefix(dialogue, npc.barkPrefix);

            if (lines.empty())
            {
                continue;
            }

            const DialogueLine* line = lines[nextRandom(placeholders.random) % lines.size()];
            queueLine(subtitles, dialogue, line->id);
            npc.cooldown = k_barkCooldownSeconds;
        }
    }

    void drawNpcPlaceholders(lockstep::render::Renderer& renderer, NpcPlaceholders& placeholders,
        const PawnCharacters* characters, float frameSeconds)
    {
        if (characters == nullptr)
        {
            return;
        }

        for (std::size_t index = 0; index < placeholders.npcs.size(); ++index)
        {
            NpcPlaceholder& npc = placeholders.npcs[index];
            const CharacterModel& character = modelFor(npc.pose, *characters);
            const anim::SkinnedModel& model = character.skinned();
            const uint32_t clip = clipFor(model, npc.pose, *characters);
            npc.instance.animator().play(clip, k_idleFadeSeconds,
                anim::PlaybackOptions{.loop = true, .speed = idleSpeed(index)});
            float seconds = frameSeconds;

            if (!npc.isStarted && clip < model.clips.size())
            {
                npc.isStarted = true;
                seconds = npc.phase * model.clips[clip].duration;
            }

            npc.instance.update(model, seconds);
            npc.instance.submit(renderer, model, npc.placement, lockstep::render::k_defaultRenderLayer,
                gearMask(character.gearMasks, npc.gearLook));
        }
    }
} //namespace game
