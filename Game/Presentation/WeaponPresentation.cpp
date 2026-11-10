#include "Presentation/WeaponPresentation.hpp"

#include "Characters/ViewModelSights.hpp"
#include "GameState.hpp"
#include "Player/LocalPlayer.hpp"
#include "Presentation/Mirrors.hpp"
#include "Simulation/HostSimulation.hpp"
#include "Weapons/WeaponSockets.hpp"

#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <array>

namespace game
{
    namespace
    {
        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        std::size_t partnerOf(std::size_t index)
        {
            return index == k_hostPawn ? k_clientPawn : k_hostPawn;
        }

        float worldFov(const GameState& state, const HeadPose& head)
        {
            const float zoom = state.pawnCharacters ? viewModelZoom(state.pawnCharacters->viewModel) : 0.0f;

            return firstPersonCamera(head.eye, lookDirection(0.0f, 0.0f), zoom, math::Vec3{0.0f, 1.0f, 0.0f},
                head.fovOffset).verticalFov;
        }

        void takeAimOffset(GameState& state, float fov)
        {
            state.local.aimOffset = math::Vec2{0.0f, 0.0f};

            if (!state.pawnCharacters)
            {
                return;
            }

            const ViewModelState& viewModel = state.pawnCharacters->viewModel;

            if (const std::optional<math::Vec3> direction = viewModelShotDirection(viewModel, fov))
            {
                state.local.aimOffset = clampAimOffset(aimOffsetTowards(viewModel.lastYaw, viewModel.lastPitch,
                    *direction), weaponStats(state.settings.weapons, state.local.command.weapon).spread);
            }
        }

        std::optional<FlashlightMount> localMount(const GameState& state, float fov)
        {
            if (!state.pawnCharacters)
            {
                return std::nullopt;
            }

            const std::optional<ViewModelSockets> sockets = viewModelSockets(state.pawnCharacters->viewModel, fov);

            if (!sockets)
            {
                return std::nullopt;
            }

            return FlashlightMount{.position = sockets->rail, .direction = sockets->forward};
        }

        std::optional<FlashlightMount> partnerMount(const GameState& state, std::size_t index)
        {
            if (!state.pawnCharacters)
            {
                return std::nullopt;
            }

            const std::optional<WeaponSocket> socket = pawnWeaponSocket(*state.pawnCharacters, index);

            if (!socket)
            {
                return std::nullopt;
            }

            const float yaw = state.pawnCharacters->pawns[index].yaw;
            const math::Vec3 direction = lookDirection(yaw, state.local.statuses[index].lookPitch);

            return FlashlightMount{.position = socket->muzzle - socket->forward * state.flashlights.tuning.partnerBack,
                .direction = direction};
        }

        void drawPawnOptics(const GameState& state, lockstep::render::Renderer& renderer, std::size_t local)
        {
            if (!state.pawnCharacters)
            {
                return;
            }

            for (std::size_t index = 0; index < k_playerCount; ++index)
            {
                const OpticModel* optic = opticModel(state.optics, state.local.statuses[index].optic);
                const std::optional<WeaponSocket> socket = optic ? pawnWeaponSocket(*state.pawnCharacters, index)
                                                                 : std::nullopt;

                if (socket)
                {
                    drawOptic(renderer, *optic, socket->optic,
                        index == local ? k_mirrorOnlyLayer : lockstep::render::k_defaultRenderLayer);
                }
            }
        }
    } //namespace

    void presentWeaponHandling(GameState& state, EngineContext& engine, const std::optional<math::Transform>& localPawn,
        const HeadPose& head, float playerSeconds)
    {
        const float fov = worldFov(state, head);
        takeAimOffset(state, fov);

        const std::size_t local = localPawnIndex(engine);
        const std::size_t partner = partnerOf(local);
        const PlayerStatus& own = state.local.statuses[local];
        FlashlightFrame lights{.localIndex = local, .seconds = playerSeconds};
        lights.isOn[local] = localPawn && own.isArmed && state.local.command.isFlashlightOn;
        lights.isOn[partner] = state.local.statuses[partner].isFlashlightOn;
        lights.mounts[local] = localPawn ? localMount(state, fov) : std::nullopt;

        if (localPawn && !lights.mounts[local])
        {
            lights.mounts[local] = FlashlightMount{.position = head.eye,
                .direction = lookDirection(state.local.command.yaw, state.local.command.pitch)};
        }

        lights.mounts[partner] = partnerMount(state, partner);
        drawPawnOptics(state, engine.renderer, local);
        presentFlashlights(state.flashlights, engine.renderer, lights);

        WeaponFeedbackFrame feedback{.flashlightOn = lights.isOn,
            .localIndex = local,
            .mode = state.local.controls.trigger.mode};

        for (std::size_t index = 0; index < k_playerCount; ++index)
        {
            if (lights.mounts[index])
            {
                feedback.positions[index] = lights.mounts[index]->position;
            }
        }

        if (state.pawnCharacters)
        {
            feedback.holds = state.pawnCharacters->viewModel.breath.holds;
            feedback.releases = state.pawnCharacters->viewModel.breath.releases;
        }

        presentWeaponFeedback(state.weaponFeedback, engine.audio, feedback);
    }

    math::Vec3 localShotDirection(const GameState& state, const EngineContext& engine, uint8_t shotNumber)
    {
        return shotDirection(state.local.command, localPawnIndex(engine), shotNumber,
            weaponStats(state.settings.weapons, state.local.command.weapon).spread);
    }

    float heldBreathFocus(const GameState& state)
    {
        if (!state.pawnCharacters || !state.pawnCharacters->viewModel.isDrawn)
        {
            return 0.0f;
        }

        const ViewModelState& viewModel = state.pawnCharacters->viewModel;
        const float breath = viewModel.breath.steadiness * state.weaponFeedback.tuning.holdFocus;
        const float scoped = viewModel.opticAim && viewModel.opticAim->isMagnified
            ? viewModelZoom(viewModel) * state.optics.scopeTuning.outerBlur : 0.0f;

        return std::max(breath, scoped);
    }

#if LOCKSTEP_DEV_TOOLS
    void weaponsDevSection(GameState& state)
    {
        if (!ImGui::CollapsingHeader("Weapons"))
        {
            return;
        }

        GameState::Settings& settings = state.settings;
        const Trigger& trigger = state.local.controls.trigger;
        const RecoilState& recoil = state.local.controls.recoil;
        ImGui::Text("Mode %s  predicted rounds %d  cooldown %.3f s  flashlight %s", fireModeName(trigger.mode),
            trigger.rounds, trigger.cooldown, state.local.controls.isFlashlightOn ? "on" : "off");
        ImGui::Text("Camera recoil %.4f %.4f rad (pending %.4f)  string %u  sight offset %.4f %.4f rad",
            recoil.offset.x, recoil.offset.y, recoil.pending.y, recoil.streak, state.local.aimOffset.x,
            state.local.aimOffset.y);
        ImGui::Text("Last host shot: %s at %.1f m", toString(state.simulation.lastShot.outcome),
            state.simulation.lastShot.distance);

        for (std::size_t index = 0; index < k_weaponCount; ++index)
        {
            if (ImGui::TreeNode(weaponName(static_cast<WeaponKind>(index))))
            {
                weaponStatsDevSection(settings.weapons[index]);
                ImGui::TreePop();
            }
        }

        flashlightDevSection(state.flashlights);
        weaponFeedbackDevSection(state.weaponFeedback);
    }
#endif
} //namespace game
