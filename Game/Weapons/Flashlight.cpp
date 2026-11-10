#include "Weapons/Flashlight.hpp"

#include <Lockstep/Render/Lighting.hpp>
#include <Lockstep/Render/Renderer.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;

        struct FlickerStep
        {
            float until{0.0f};
            float level{1.0f};
        };

        constexpr FlickerStep k_switchOn[] = {
            {0.10f, 0.85f},
            {0.22f, 0.08f},
            {0.34f, 0.65f},
            {0.46f, 0.2f},
            {0.62f, 0.95f},
            {0.72f, 0.55f},
            {1.0f, 1.0f},
        };

        float switchOnLevel(float seconds, float flickerSeconds)
        {
            if (flickerSeconds <= 0.0f || seconds >= flickerSeconds)
            {
                return 1.0f;
            }

            const float progress = seconds / flickerSeconds;

            for (const FlickerStep& step : k_switchOn)
            {
                if (progress < step.until)
                {
                    return step.level;
                }
            }

            return 1.0f;
        }
    } //namespace

    void presentFlashlights(FlashlightState& state, render::Renderer& renderer, const FlashlightFrame& frame)
    {
        const FlashlightTuning& tuning = state.tuning;

        for (std::size_t index = 0; index < k_playerCount; ++index)
        {
            const bool isOn = frame.isOn[index];
            state.onSeconds[index] = isOn && state.wasOn[index] ? state.onSeconds[index] + frame.seconds : 0.0f;
            state.wasOn[index] = isOn;

            if (!isOn || !frame.mounts[index])
            {
                continue;
            }

            const FlashlightMount& mount = *frame.mounts[index];
            const float level = switchOnLevel(state.onSeconds[index], tuning.flickerSeconds);
            const bool isLocal = index == frame.localIndex;
            renderer.submitLight(render::SpotLight{.position = mount.position + mount.direction * tuning.forwardOffset,
                .direction = mount.direction,
                .color = tuning.color,
                .intensity = tuning.intensity * level,
                .range = tuning.range,
                .innerAngle = std::min(tuning.innerAngle, tuning.outerAngle * 0.95f),
                .outerAngle = tuning.outerAngle,
                .castsShadows = tuning.castsShadows,
                .volumetric = (isLocal ? tuning.volumetric : tuning.partnerVolumetric) * level});
        }
    }

#if LOCKSTEP_DEV_TOOLS
    void flashlightDevSection(FlashlightState& state)
    {
        if (!ImGui::TreeNode("Flashlight"))
        {
            return;
        }

        FlashlightTuning& tuning = state.tuning;
        ImGui::Text("On: Player1 %s  Player2 %s", state.wasOn[0] ? "yes" : "no", state.wasOn[1] ? "yes" : "no");
        ImGui::ColorEdit3("Colour", &tuning.color.x);
        ImGui::SliderFloat("Intensity", &tuning.intensity, 0.0f, 400.0f);
        ImGui::SliderFloat("Range (m)", &tuning.range, 1.0f, 80.0f);
        ImGui::SliderFloat("Hot spot (rad)", &tuning.innerAngle, 0.01f, 0.8f);
        ImGui::SliderFloat("Spill (rad)", &tuning.outerAngle, 0.02f, 1.2f);
        ImGui::SliderFloat("Fog, own beam", &tuning.volumetric, 0.0f, 2.0f);
        ImGui::SliderFloat("Fog, partner beam", &tuning.partnerVolumetric, 0.0f, 2.0f);
        ImGui::Checkbox("Shadows", &tuning.castsShadows);
        ImGui::SliderFloat("Ahead of mount (m)", &tuning.forwardOffset, 0.0f, 0.4f);
        ImGui::SliderFloat("Switch-on stutter (s)", &tuning.flickerSeconds, 0.0f, 1.0f);
        ImGui::SliderFloat("Partner light behind muzzle (m)", &tuning.partnerBack, 0.0f, 0.4f);
        ImGui::TreePop();
    }
#endif
} //namespace game
