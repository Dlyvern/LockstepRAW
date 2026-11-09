#include "Characters/NpcPlaceholders.hpp"

#include <Lockstep/Render/Renderer.hpp>

#include <array>
#include <cctype>
#include <cmath>
#include <iostream>
#include <utility>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace assets = lockstep::assets;

        constexpr std::string_view k_npcPrefix{"Npc_"};
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
        constexpr math::Vec3 k_upAxis{0.0f, 1.0f, 0.0f};
        constexpr float k_barkReach{2.5f};
        constexpr float k_barkHeight{1.5f};
        constexpr float k_barkCooldownSeconds{20.0f};
        constexpr float k_idleFadeSeconds{0.0f};
        constexpr float k_idleSpeedSpread{0.15f};

        std::string roleOf(std::string_view name)
        {
            for (const auto& [prefix, role] : k_roles)
            {
                if (name.starts_with(prefix))
                {
                    return std::string(role);
                }
            }

            std::string role(name.substr(k_npcPrefix.size()));
            role = role.substr(0, role.find('_'));

            for (char& character : role)
            {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }

            return role;
        }

        std::string barkPrefix(std::string_view level, std::string_view role)
        {
            if (role == k_chiefRole)
            {
                return {};
            }

            const std::string base = std::string(level) + "." + std::string(role);
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
            if (!std::string_view(node.name).starts_with(k_npcPrefix))
            {
                continue;
            }

            const std::string role = roleOf(node.name);
            placeholders.npcs.push_back(NpcPlaceholder{.name = node.name,
                .barkPrefix = barkPrefix(level, role),
                .placement = math::Transform{.position = node.transform.position,
                    .rotation = node.transform.rotation * math::angleAxis(math::k_pi, k_upAxis)}});
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

        const anim::SkinnedModel& model = characters->unarmed.skinned();

        for (std::size_t index = 0; index < placeholders.npcs.size(); ++index)
        {
            NpcPlaceholder& npc = placeholders.npcs[index];
            npc.instance.animator().play(characters->locomotion.idle, k_idleFadeSeconds,
                anim::PlaybackOptions{.loop = true, .speed = idleSpeed(index)});
            npc.instance.update(model, frameSeconds);
            npc.instance.submit(renderer, model, npc.placement);
        }
    }
} //namespace game
