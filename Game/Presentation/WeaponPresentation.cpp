#include "Presentation/WeaponPresentation.hpp"

#include "Characters/ViewModelSights.hpp"
#include "GameState.hpp"
#include "Player/LocalPlayer.hpp"
#include "Simulation/HostSimulation.hpp"
#include "Weapons/WeaponSockets.hpp"

#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

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
                    *direction), state.settings.spread);
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
        return shotDirection(state.local.command, localPawnIndex(engine), shotNumber, state.settings.spread);
    }

    float heldBreathFocus(const GameState& state)
    {
        if (!state.pawnCharacters || !state.pawnCharacters->viewModel.isDrawn)
        {
            return 0.0f;
        }

        return state.pawnCharacters->viewModel.breath.steadiness * state.weaponFeedback.tuning.holdFocus;
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

        if (ImGui::TreeNode("Fire rate"))
        {
            ImGui::SliderFloat("Rounds per minute", &settings.fire.roundsPerMinute, 60.0f, 1500.0f);
            int burst = settings.fire.burstLength;

            if (ImGui::SliderInt("Burst length", &burst, 2, 5))
            {
                settings.fire.burstLength = static_cast<uint8_t>(burst);
            }

            ImGui::SliderFloat("Burst pause (s)", &settings.fire.burstPause, 0.0f, 0.5f);
            ImGui::SliderFloat("Host rate tolerance", &settings.fire.rateTolerance, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Camera recoil"))
        {
            RecoilSettings& camera = settings.recoil;
            ImGui::SliderFloat("Climb per shot (rad)", &camera.climb, 0.0f, 0.04f, "%.4f");
            ImGui::SliderFloat("First shot x", &camera.firstShotScale, 0.5f, 3.0f);
            ImGui::SliderFloat("Build per shot", &camera.build, 0.0f, 0.2f);
            ImGui::SliderFloat("Max build x", &camera.maxBuild, 1.0f, 3.0f);
            ImGui::SliderFloat("Wander per shot (rad)", &camera.wander, 0.0f, 0.02f, "%.4f");
            ImGui::SliderFloat("Wander band (rad)", &camera.wanderLimit, 0.0f, 0.1f, "%.4f");
            ImGui::SliderFloat("Wander pull", &camera.wanderPull, 0.0f, 1.0f);
            ImGui::SliderFloat("Kick time (s)", &camera.kickSeconds, 0.0f, 0.2f);
            ImGui::SliderFloat("Recovery delay (s)", &camera.recoverDelay, 0.0f, 0.6f);
            ImGui::SliderFloat("Recovery rate", &camera.recoverRate, 0.0f, 20.0f);
            ImGui::SliderFloat("Recovered fraction", &camera.recoverAmount, 0.0f, 1.0f);
            ImGui::SliderFloat("String ends after (s)", &camera.streakSeconds, 0.05f, 1.0f);
            ImGui::SliderFloat("Aiming x", &camera.aimScale, 0.0f, 1.0f);
            ImGui::SliderFloat("Crouched x", &camera.crouchScale, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Spread"))
        {
            SpreadSettings& spread = settings.spread;
            ImGui::SliderFloat("Hip (rad)", &spread.hip, 0.0f, 0.1f, "%.4f");
            ImGui::SliderFloat("Aiming (rad)", &spread.aim, 0.0f, 0.02f, "%.4f");
            ImGui::SliderFloat("Moving (rad)", &spread.moving, 0.0f, 0.1f, "%.4f");
            ImGui::SliderFloat("Running (rad)", &spread.sprinting, 0.0f, 0.15f, "%.4f");
            ImGui::SliderFloat("Crouched x", &spread.crouchScale, 0.0f, 1.0f);
            ImGui::SliderFloat("Max sight offset (rad)", &spread.maxAimOffset, 0.0f, 0.4f);
            ImGui::TreePop();
        }

        penetrationDevSection(settings.penetration);
        flashlightDevSection(state.flashlights);
        weaponFeedbackDevSection(state.weaponFeedback);
    }
#endif
} //namespace game
