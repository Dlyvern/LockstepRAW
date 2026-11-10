#include "Characters/NpcBodies.hpp"

#include "Characters/PawnCharacters.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <iostream>
#include <utility>

namespace game
{
    namespace
    {
        std::optional<NpcArmedBody> loadArmedBody(lockstep::render::Renderer& renderer,
            const lockstep::assets::FileSystem& files, std::string_view scene, const LocomotionClipNames& rifleNames,
            const LocomotionClipNames& aimingNames)
        {
            std::optional<CharacterModel> body = loadCharacterModel(renderer, files, scene, k_pawnCharacter.model);

            if (!body)
            {
                return std::nullopt;
            }

            auto rifle = findLocomotionSet(body->skinned(), rifleNames);
            auto aiming = findLocomotionSet(body->skinned(), aimingNames);

            if (!rifle || !aiming)
            {
                std::clog << "Game: NPC body '" << scene << "' not usable: "
                          << (rifle ? aiming.error().message : rifle.error().message) << '\n';
                return std::nullopt;
            }

            std::optional<ArmedCrouch> crouch = findArmedCrouch(body->skinned(), k_armedCrouchNames);

            return NpcArmedBody{.body = std::move(*body), .rifle = rifle.value(), .aiming = aiming.value(),
                .crouch = std::move(crouch)};
        }
    } //namespace

    NpcBodies loadNpcBodies(lockstep::render::Renderer& renderer, const lockstep::assets::FileSystem& files,
        const LocomotionClipNames& rifle, const LocomotionClipNames& aiming)
    {
        NpcBodies bodies{};

        for (std::size_t index = 0; index < k_npcBodyCount; ++index)
        {
            const NpcBodyScenes& scenes = k_npcBodyScenes[index];
            NpcBodyModels& models = bodies.bodies[index];

            if (!scenes.unarmed.empty())
            {
                models.unarmed = loadCharacterModel(renderer, files, scenes.unarmed, k_pawnCharacter.model);
            }

            if (!scenes.armed.empty())
            {
                models.armed = loadArmedBody(renderer, files, scenes.armed, rifle, aiming);
            }

            if ((!scenes.unarmed.empty() && !models.unarmed) || (!scenes.armed.empty() && !models.armed))
            {
                std::clog << "Game: NPC body " << index << " incomplete; those NPCs use the SWAT model\n";
            }
        }

        return bodies;
    }

    const CharacterModel& npcUnarmedModel(const PawnCharacters& characters, NpcBody body)
    {
        const std::optional<CharacterModel>& model = characters.npcBodies.bodies[static_cast<std::size_t>(body)].unarmed;
        return model ? *model : characters.unarmed;
    }

    std::optional<NpcArmedView> npcArmedModel(const PawnCharacters& characters, NpcBody body)
    {
        if (const auto& armed = characters.npcBodies.bodies[static_cast<std::size_t>(body)].armed; armed)
        {
            return NpcArmedView{.body = &armed->body, .rifle = &armed->rifle, .aiming = &armed->aiming,
                .crouch = armed->crouch ? &*armed->crouch : nullptr};
        }

        if (characters.armed)
        {
            return NpcArmedView{.body = &characters.armed->body, .rifle = &characters.armed->rifle,
                .aiming = &characters.armed->aiming,
                .crouch = characters.armed->crouch ? &*characters.armed->crouch : nullptr};
        }

        return std::nullopt;
    }
} //namespace game
