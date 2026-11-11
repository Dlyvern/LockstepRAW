#include "LightingFeaturesPanel.hpp"

#include "Render/RendererState.hpp"

#include <imgui.h>

namespace lockstep::debug
{
    namespace
    {
        bool sliderCount(const char* label, uint32_t& value, int minimum, int maximum)
        {
            int count = static_cast<int>(value);

            if (!ImGui::SliderInt(label, &count, minimum, maximum))
            {
                return false;
            }

            value = static_cast<uint32_t>(count);
            return true;
        }

        void drawLightShapes(render::LightShapeSettings& shapes)
        {
            if (!ImGui::CollapsingHeader("Tube lights and light profiles"))
            {
                return;
            }

            ImGui::Checkbox("Tube lights (off: points)", &shapes.tubeLights);
            ImGui::Checkbox("Light profiles", &shapes.profiles);
        }

        void drawSoftShadows(render::SoftShadowSettings& soft)
        {
            if (!ImGui::CollapsingHeader("Soft shadows (contact hardening)"))
            {
                return;
            }

            ImGui::Checkbox("Soft shadows", &soft.enabled);
            ImGui::SliderFloat("Sun size (penumbra m per m)", &soft.sunSize, 0.0f, 0.1f, "%.4f");
            ImGui::SliderFloat("Local light radius (m)", &soft.localRadius, 0.0f, 0.5f, "%.3f");
            sliderCount("Blocker taps", soft.blockerTaps, 4, 32);
            sliderCount("Filter taps", soft.filterTaps, 4, 32);
            ImGui::SliderFloat("Max penumbra (texels)", &soft.maxPenumbraTexels, 1.0f, 32.0f, "%.1f");
        }

        void drawBounceLight(render::IrradianceVolumeSettings& volume, const render::IrradianceVolumeState& state)
        {
            if (!ImGui::CollapsingHeader("Bounce light (irradiance volume)"))
            {
                return;
            }

            if (state.volume)
            {
                ImGui::Text("Volume: %u x %u x %u probes, %.2f m", state.volume->countX, state.volume->countY,
                    state.volume->countZ, static_cast<double>(state.volume->spacing.x));
            }
            else
            {
                ImGui::TextUnformatted("Volume: none (cook the level with --gi)");
            }

            ImGui::Checkbox("Bounce light", &volume.enabled);
            ImGui::SliderFloat("Bounce intensity", &volume.intensity, 0.0f, 4.0f, "%.2f");
            ImGui::SliderFloat("Sky visibility", &volume.skyVisibility, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Zone fill kept", &volume.fillKept, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Normal offset (m)", &volume.normalOffset, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Edge fade (m)", &volume.fadeDistance, 0.01f, 4.0f, "%.2f");
            ImGui::Checkbox("Show bounce light only", &volume.debugView);
        }

        void drawShadowDetail(render::ShadowDetailSettings& detail, const render::ShadowCache& cache)
        {
            if (!ImGui::CollapsingHeader("Shadow casters"))
            {
                return;
            }

            ImGui::SliderFloat("Min caster size (texels)", &detail.minCasterTexels, 0.0f, 16.0f, "%.1f");
            ImGui::Text("Shadow draw calls last frame: %u", cache.drawCalls);
        }
    } //namespace

    void drawLightingFeatureSections(render::RendererState& renderer)
    {
        render::RenderSettings& settings = renderer.requestedSettings;
        drawLightShapes(settings.lightShapes);
        drawSoftShadows(settings.softShadows);
        drawBounceLight(settings.irradianceVolume, renderer.irradiance);
        drawShadowDetail(settings.shadowDetail, renderer.shadowCache);
    }
} //namespace lockstep::debug
