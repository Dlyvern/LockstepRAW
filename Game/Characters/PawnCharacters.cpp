#include "Characters/PawnCharacters.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Render/Renderer.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace assets = lockstep::assets;
        namespace render = lockstep::render;
        namespace world = lockstep::world;

        constexpr math::Vec3 k_upAxis{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_pawnForward{0.0f, 0.0f, -1.0f};

        void logStrides(std::string_view label, const anim::SkinnedModel& model, const LocomotionSet& set)
        {
            std::clog << std::fixed << std::setprecision(2) << "Game: pawn character '" << model.name << "' " << label
                << " stride speeds (m/s): walk " << set.walk.strideSpeed << ", run " << set.run.strideSpeed
                << ", back " << set.walkBack.strideSpeed << ", strafe " << set.strafeLeft.strideSpeed << " / "
                << set.strafeRight.strideSpeed << '\n';
        }

        float yawOf(const math::Quat& rotation)
        {
            const math::Vec3 forward = rotation * k_pawnForward;

            return std::atan2(-forward.x, -forward.z);
        }

        std::size_t pawnIndex(world::EntityOwner owner)
        {
            return owner == world::EntityOwner::Player2 ? 1 : 0;
        }

        std::optional<ArmedCharacter> loadArmed(render::Renderer& renderer, const assets::FileSystem& files)
        {
            std::optional<CharacterModel> body = loadCharacterModel(renderer, files, k_pawnCharacter.armedScene,
                k_pawnCharacter.model);

            if (!body)
            {
                std::clog << "Game: no armed pawn character; armed pawns use the unarmed body\n";
                return std::nullopt;
            }

            auto rifle = findLocomotionSet(body->skinned(), k_pawnCharacter.rifle);
            auto aiming = findLocomotionSet(body->skinned(), k_pawnCharacter.aiming);

            if (!rifle || !aiming)
            {
                std::clog << "Game: armed pawn character not usable: "
                          << (rifle ? aiming.error().message : rifle.error().message) << '\n';
                return std::nullopt;
            }

            logStrides("rifle", body->skinned(), rifle.value());
            logStrides("aiming", body->skinned(), aiming.value());

            return ArmedCharacter{.body = std::move(*body), .rifle = rifle.value(), .aiming = aiming.value()};
        }

        std::optional<FirstPersonModel> loadFirstPerson(render::Renderer& renderer, const assets::FileSystem& files)
        {
            std::optional<CharacterModel> arms = loadCharacterModel(renderer, files, k_pawnCharacter.firstPersonScene,
                k_pawnCharacter.model);

            if (!arms)
            {
                std::clog << "Game: no first-person viewmodel; armed players see no weapon\n";
                return std::nullopt;
            }

            auto model = createFirstPersonModel(std::move(*arms), k_pawnCharacter.firstPersonAim,
                k_pawnCharacter.firstPersonLow);

            if (!model)
            {
                std::clog << "Game: viewmodel not usable: " << model.error().message << '\n';
                return std::nullopt;
            }

            return std::move(model).value();
        }

        void drawBox(render::Renderer& renderer, const Visuals& visuals, world::EntityOwner owner,
            const math::Transform& transform, render::RenderLayers layers)
        {
            renderer.submit(visuals.cube, owner == world::EntityOwner::Player2 ? visuals.player2 : visuals.player1,
                transform, layers);
        }
    } //namespace

    std::optional<PawnCharacters> loadPawnCharacters(render::Renderer& renderer, const assets::FileSystem& files)
    {
        std::optional<CharacterModel> unarmed = loadCharacterModel(renderer, files, k_pawnCharacter.unarmedScene,
            k_pawnCharacter.model);

        if (!unarmed)
        {
            std::clog << "Game: no pawn character; pawns are boxes\n";
            return std::nullopt;
        }

        auto locomotion = findLocomotionSet(unarmed->skinned(), k_pawnCharacter.unarmed);

        if (!locomotion)
        {
            std::clog << "Game: pawn character not usable: " << locomotion.error().message << "; pawns are boxes\n";
            return std::nullopt;
        }

        logStrides("unarmed", unarmed->skinned(), locomotion.value());
        PawnCharacters characters{.unarmed = std::move(*unarmed), .locomotion = locomotion.value()};
        characters.armed = loadArmed(renderer, files);
        characters.firstPerson = loadFirstPerson(renderer, files);

        return characters;
    }

    void beginPawnCharacters(PawnCharacters& characters)
    {
        for (PawnCharacter& pawn : characters.pawns)
        {
            pawn.wasDrawn = pawn.isDrawn;
            pawn.isDrawn = false;
        }
    }

    void drawPawn(render::Renderer& renderer, const Visuals& visuals, PawnCharacters* characters,
        world::EntityOwner owner, const math::Transform& transform, const PawnLook& look, float deltaSeconds,
        render::RenderLayers layers)
    {
        if (characters == nullptr)
        {
            drawBox(renderer, visuals, owner, transform, layers);
            return;
        }

        PawnCharacter& pawn = characters->pawns[pawnIndex(owner)];
        const ArmedCharacter* armed = look.isArmed && characters->armed ? &*characters->armed : nullptr;
        const anim::SkinnedModel& model = armed != nullptr ? armed->body.skinned() : characters->unarmed.skinned();
        const LocomotionSet& set = armed == nullptr ? characters->locomotion
            : (look.isLowered ? armed->rifle : armed->aiming);
        const math::Vec3 foot = pawnFootPosition(transform);
        const float yaw = yawOf(transform.rotation);

        if (!pawn.wasDrawn || pawn.isArmedModel != (armed != nullptr))
        {
            pawn.locomotion.hasLast = false;
        }

        pawn.isDrawn = true;
        pawn.isArmedModel = armed != nullptr;
        updateLocomotion(pawn.locomotion, set, pawn.instance.animator(), foot, yaw, deltaSeconds);
        pawn.instance.update(model, deltaSeconds);
        pawn.instance.submit(renderer, model,
            math::Transform{.position = foot, .rotation = math::angleAxis(yaw + math::k_pi, k_upAxis)}, layers);
    }

    float pawnCharacterSpeed(const PawnCharacters& characters, world::EntityOwner owner)
    {
        const PawnCharacter& pawn = characters.pawns[pawnIndex(owner)];

        return pawn.isDrawn ? math::length(pawn.locomotion.velocity) : 0.0f;
    }

#if LOCKSTEP_DEV_TOOLS
    void pawnCharactersDevSection(PawnCharacters* characters)
    {
        if (!ImGui::CollapsingHeader("Pawn characters"))
        {
            return;
        }

        if (characters == nullptr)
        {
            ImGui::TextDisabled("Not loaded (mount characters.pak); pawns are boxes");
            return;
        }

        const LocomotionSet& set = characters->locomotion;
        ImGui::Text("'%s'  armed body %s  viewmodel %s", characters->unarmed.skinned().name.c_str(),
            characters->armed ? "yes" : "no", characters->firstPerson ? "yes" : "no");
        ImGui::Text("Strides (m/s): walk %.2f  run %.2f  back %.2f  strafe %.2f / %.2f", set.walk.strideSpeed,
            set.run.strideSpeed, set.walkBack.strideSpeed, set.strafeLeft.strideSpeed, set.strafeRight.strideSpeed);

        if (characters->armed)
        {
            const LocomotionSet& aiming = characters->armed->aiming;
            ImGui::Text("Aiming strides: walk %.2f  back %.2f  strafe %.2f / %.2f", aiming.walk.strideSpeed,
                aiming.walkBack.strideSpeed, aiming.strafeLeft.strideSpeed, aiming.strafeRight.strideSpeed);
        }

        for (std::size_t index = 0; index < characters->pawns.size(); ++index)
        {
            const PawnCharacter& pawn = characters->pawns[index];
            const LocomotionState& locomotion = pawn.locomotion;
            ImGui::Text("Player %zu: %s%s  %.2f m/s  turn %.2f rad/s  phase %.2f", index + 1, gaitName(locomotion.gait),
                pawn.isArmedModel ? " (armed)" : "", math::length(locomotion.velocity), locomotion.yawRate,
                pawn.instance.animator().phase());
        }

        if (characters->firstPerson)
        {
            viewModelDevSection(characters->viewModel);
        }
    }
#endif
} //namespace game
