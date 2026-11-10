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
#include <vector>

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
        constexpr math::Vec3 k_pawnBack{0.0f, 0.0f, 1.0f};
        constexpr math::Vec3 k_modelRight{1.0f, 0.0f, 0.0f};
        constexpr float k_minSquash{0.3f};

        math::Transform posedTransform(PawnCharacter& pawn, const PawnPoseTuning& tuning, const math::Vec3& foot,
            float yaw, float leanRoll, const PawnLook& look, bool hasCrouchClips, float deltaSeconds)
        {
            const float slideTarget = look.isSliding ? 1.0f : 0.0f;
            pawn.slideBlend += (slideTarget - pawn.slideBlend) * (1.0f - std::exp(-tuning.slideBlendRate * deltaSeconds));
            const float vaultTarget = look.isVaulting ? 1.0f : 0.0f;
            pawn.vaultBlend += (vaultTarget - pawn.vaultBlend) * (1.0f - std::exp(-tuning.vaultBlendRate * deltaSeconds));
            const float crouch = hasCrouchClips ? 0.0f : math::clamp(look.crouch, 0.0f, 1.0f);
            const float squash = std::max(1.0f - tuning.crouchSquash * crouch - tuning.slideSquash * pawn.slideBlend
                    - tuning.vaultSquash * pawn.vaultBlend, k_minSquash);

            const math::Quat facing = math::angleAxis(yaw, k_upAxis);
            const math::Quat vault = math::angleAxis(tuning.vaultRoll * pawn.vaultBlend, facing * k_pawnBack)
                * math::angleAxis(-tuning.vaultLean * pawn.vaultBlend, facing * k_modelRight);
            const math::Vec3 pivot = k_upAxis * (tuning.vaultPivot * squash);

            return math::Transform{.position = foot + pivot - vault * pivot,
                .rotation = vault * math::angleAxis(-leanRoll, facing * k_pawnBack)
                    * math::angleAxis(yaw + math::k_pi, k_upAxis)
                    * math::angleAxis(-tuning.slideLean * pawn.slideBlend, k_modelRight),
                .scale = math::Vec3{1.0f, squash, 1.0f}};
        }

        void updateKickTimer(PawnCharacter& pawn, const KickPoseTuning& tuning, const PawnLook& look,
            float deltaSeconds)
        {
            if (look.isKicking && !pawn.wasKicking)
            {
                pawn.kickSeconds = 0.0f;
            }
            else if (pawn.kickSeconds >= 0.0f)
            {
                pawn.kickSeconds += deltaSeconds;
            }

            if (pawn.kickSeconds > tuning.seconds)
            {
                pawn.kickSeconds = -1.0f;
            }

            pawn.wasKicking = look.isKicking;
        }

        void logStrides(std::string_view label, const anim::SkinnedModel& model, const LocomotionSet& set)
        {
            std::clog << std::fixed << std::setprecision(2) << "Game: pawn character '" << model.name << "' " << label
                << " stride speeds (m/s): walk " << set.walk.strideSpeed << ", run " << set.run.strideSpeed
                << ", back " << set.walkBack.strideSpeed << ", strafe " << set.strafeLeft.strideSpeed << " / "
                << set.strafeRight.strideSpeed << '\n';
        }

        void logFirstPersonBody(std::string_view label, bool hasLimbs)
        {
            std::clog << "Game: " << label << " pawn character "
                      << (hasLimbs ? "shows its body in first person"
                                   : "has no limbs (cook with --limbs); no first-person body") << '\n';
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
            std::optional<FirstPersonBodyMasks> firstPersonBody = findFirstPersonBodyMasks(body->skinned(),
                body->gearMasks);
            logFirstPersonBody("armed", firstPersonBody.has_value());

            return ArmedCharacter{.body = std::move(*body), .rifle = rifle.value(), .aiming = aiming.value(),
                .firstPersonBody = firstPersonBody};
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

            auto model = createFirstPersonModel(std::move(*arms), k_pawnCharacter.firstPersonAim);

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
        auto crouch = findLocomotionSet(unarmed->skinned(), k_pawnCharacter.unarmedCrouch);

        if (crouch)
        {
            logStrides("unarmed crouch", unarmed->skinned(), crouch.value());
        }
        else
        {
            std::clog << "Game: no unarmed crouch clips (" << crouch.error().message << "); crouching squashes the body\n";
        }

        PawnCharacters characters{.unarmed = std::move(*unarmed), .locomotion = locomotion.value()};
        characters.unarmedFirstPersonBody = findFirstPersonBodyMasks(characters.unarmed.skinned(),
            characters.unarmed.gearMasks);
        logFirstPersonBody("unarmed", characters.unarmedFirstPersonBody.has_value());

        if (crouch)
        {
            characters.crouch = crouch.value();
        }

        characters.armed = loadArmed(renderer, files);
        characters.firstPerson = loadFirstPerson(renderer, files);
        characters.npcBodies = loadNpcBodies(renderer, files, k_pawnCharacter.rifle, k_pawnCharacter.aiming);

        return characters;
    }

    StrideLengths pawnStrideLengths(const PawnCharacters& characters)
    {
        const std::vector<anim::AnimationClip>& clips = characters.unarmed.skinned().clips;
        const auto cycle = [&clips](const StrideClip& stride)
        {
            return stride.clip < clips.size() ? stride.strideSpeed * clips[stride.clip].duration : 0.0f;
        };

        return StrideLengths{.walk = cycle(characters.locomotion.walk),
            .run = cycle(characters.locomotion.run),
            .crouch = characters.crouch ? cycle(characters.crouch->walk) : 0.0f,
            .walkSpeed = characters.locomotion.walk.strideSpeed,
            .runSpeed = characters.locomotion.run.strideSpeed};
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
        render::RenderLayers layers, const FirstPersonBodyView* firstPerson)
    {
        if (characters == nullptr)
        {
            drawBox(renderer, visuals, owner, transform, layers);
            return;
        }

        PawnCharacter& pawn = characters->pawns[pawnIndex(owner)];
        const ArmedCharacter* armed = look.isArmed && characters->armed ? &*characters->armed : nullptr;
        const anim::SkinnedModel& model = armed != nullptr ? armed->body.skinned() : characters->unarmed.skinned();
        const bool isCrouchClip = armed == nullptr && characters->crouch && !look.isSliding
            && look.crouch >= characters->pose.crouchClipThreshold;
        const LocomotionSet& set = armed != nullptr ? (look.isLowered ? armed->rifle : armed->aiming)
            : (isCrouchClip ? *characters->crouch : characters->locomotion);
        const math::Vec3 foot = pawnFootPosition(transform);
        const float yaw = yawOf(transform.rotation);

        if (!pawn.wasDrawn || pawn.isArmedModel != (armed != nullptr))
        {
            pawn.locomotion.hasLast = false;
        }

        pawn.isDrawn = true;
        pawn.isArmedModel = armed != nullptr;
        updateLocomotion(pawn.locomotion, set, pawn.instance.animator(), foot, yaw, deltaSeconds);
        resolveReactionRig(pawn.reactionRig, model);
        updateHitReaction(pawn.reaction, pawn.reactionRig, characters->reactionTuning, deltaSeconds);

        if (firstPerson != nullptr)
        {
            resetHitReaction(pawn.reaction);
        }

        updateKickTimer(pawn, characters->kickPose, look, deltaSeconds);
        const float kickExtension = appendKickOffsets(pawn.reaction.offsets, pawn.reactionRig, characters->kickPose,
            pawn.kickSeconds);

        pawn.instance.update(model, deltaSeconds, pawn.reaction.offsets);
        const math::Transform placement = posedTransform(pawn, characters->pose, foot, yaw, pawnLeanRoll(transform),
            look, armed == nullptr && characters->crouch.has_value(), deltaSeconds);
        pawn.placement = placement;
        pawn.yaw = yaw;
        pawn.instance.submit(renderer, model, placement, layers,
            gearMask(armed != nullptr ? armed->body.gearMasks : characters->unarmed.gearMasks, playerGearLook(owner)));

        const std::optional<FirstPersonBodyMasks>& bodyMasks = armed != nullptr ? armed->firstPersonBody
                                                                                : characters->unarmedFirstPersonBody;

        if (firstPerson == nullptr || !bodyMasks)
        {
            return;
        }

        const anim::PartMask parts = firstPersonBodyParts(*bodyMasks, characters->firstPersonBody, *firstPerson);

        if (parts != 0)
        {
            const float crouch = look.isSliding ? 1.0f : look.crouch;
            pawn.instance.submit(renderer, model,
                firstPersonBodyPlacement(placement, characters->firstPersonBody, *firstPerson, crouch, kickExtension),
                k_firstPersonBodyLayer, parts);
        }
    }

    void reactToPawnHit(PawnCharacters& characters, world::EntityOwner owner, const ReactionHit& hit)
    {
        PawnCharacter& pawn = characters.pawns[pawnIndex(owner)];
        const bool isPosed = pawn.reactionRig.model != nullptr
            && pawn.instance.jointMatrices().size() == pawn.reactionRig.model->skeleton.joints.size();

        if (!pawn.isDrawn || !isPosed || !characters.reactionTuning.isEnabled)
        {
            return;
        }

        static_cast<void>(pushHitReaction(pawn.reaction, pawn.reactionRig, pawn.instance.jointMatrices(),
            pawn.placement, hit, characters.reactionTuning));
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

        ImGui::PushID("pawnReactions");
        hitReactionDevSection(characters->reactionTuning);
        ImGui::PopID();

        if (ImGui::TreeNode("Crouch / slide stand-in pose"))
        {
            PawnPoseTuning& pose = characters->pose;
            ImGui::SliderFloat("Crouch squash", &pose.crouchSquash, 0.0f, 0.6f);
            ImGui::SliderFloat("Crouch clip threshold", &pose.crouchClipThreshold, 0.05f, 1.0f);
            ImGui::SliderFloat("Slide squash", &pose.slideSquash, 0.0f, 0.6f);
            ImGui::SliderFloat("Slide lean (rad)", &pose.slideLean, -1.0f, 1.0f);
            ImGui::SliderFloat("Slide blend rate", &pose.slideBlendRate, 1.0f, 30.0f);
            ImGui::SliderFloat("Vault roll (rad)", &pose.vaultRoll, -1.2f, 1.2f);
            ImGui::SliderFloat("Vault lean (rad)", &pose.vaultLean, -1.0f, 1.0f);
            ImGui::SliderFloat("Vault squash", &pose.vaultSquash, 0.0f, 0.6f);
            ImGui::SliderFloat("Vault pivot (m)", &pose.vaultPivot, 0.0f, 1.6f);
            ImGui::SliderFloat("Vault blend rate", &pose.vaultBlendRate, 1.0f, 40.0f);
            ImGui::TreePop();
        }

        firstPersonBodyDevSection(characters->firstPersonBody);
        kickPoseDevSection(characters->kickPose);

        if (characters->firstPerson)
        {
            viewModelDevSection(characters->viewModel);
        }
    }
#endif
} //namespace game
