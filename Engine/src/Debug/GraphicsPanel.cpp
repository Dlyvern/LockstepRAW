#include "GraphicsPanel.hpp"

#include <imgui.h>

namespace lockstep::debug
{
    namespace
    {
        constexpr float k_gpuSmoothing{0.05f};
        constexpr float k_panelWidth{340.0f};
        constexpr ImVec2 k_panelPosition{360.0f, 10.0f};
        constexpr float k_maxAmbient{1.0f};
        constexpr float k_maxDepthBias{0.2f};
        constexpr float k_maxNormalBias{5.0f};
        constexpr float k_maxFogDensity{0.3f};
        constexpr float k_maxFogStart{30.0f};
        constexpr float k_minExposure{0.05f};
        constexpr float k_maxExposure{8.0f};

        const char* casterName(render::ShadowCaster caster)
        {
            switch (caster)
            {
                case render::ShadowCaster::Directional:
                {
                    return "directional";
                }
                case render::ShadowCaster::Spot:
                {
                    return "spot";
                }
                case render::ShadowCaster::None:
                {
                    break;
                }
            }

            return "none";
        }

        void drawLighting(render::LightingSettings& lighting)
        {
            if (!ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
            {
                return;
            }

            ImGui::Checkbox("Directional light", &lighting.directional);
            ImGui::Checkbox("Point / spot lights", &lighting.localLights);
            ImGui::Checkbox("Specular", &lighting.specular);
            ImGui::ColorEdit3("Ambient colour", &lighting.ambientColor.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Ambient", &lighting.ambientIntensity, 0.0f, k_maxAmbient, "%.3f",
                ImGuiSliderFlags_Logarithmic);
        }

        void drawShadows(render::ShadowSettings& shadows)
        {
            if (!ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen))
            {
                return;
            }

            ImGui::Checkbox("Shadow map", &shadows.enabled);
            ImGui::SliderFloat("Depth bias (m)", &shadows.depthBias, 0.0f, k_maxDepthBias, "%.3f");
            ImGui::SliderFloat("Normal bias (texels)", &shadows.normalBias, 0.0f, k_maxNormalBias, "%.2f");
        }

        void drawFog(render::FogSettings& fog)
        {
            if (!ImGui::CollapsingHeader("Fog", ImGuiTreeNodeFlags_DefaultOpen))
            {
                return;
            }

            ImGui::Checkbox("Distance fog", &fog.enabled);
            ImGui::ColorEdit3("Fog colour", &fog.color.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Density", &fog.density, 0.0f, k_maxFogDensity, "%.3f");
            ImGui::SliderFloat("Start (m)", &fog.start, 0.0f, k_maxFogStart, "%.1f");
            ImGui::SliderFloat("Max amount", &fog.maxAmount, 0.0f, 1.0f, "%.2f");
        }

        void drawTonemap(render::TonemapSettings& tonemap)
        {
            if (!ImGui::CollapsingHeader("Tonemapping", ImGuiTreeNodeFlags_DefaultOpen))
            {
                return;
            }

            ImGui::Checkbox("Tonemap", &tonemap.enabled);

            int tonemapper = static_cast<int>(tonemap.tonemapper);

            if (ImGui::Combo("Curve", &tonemapper, "Clamp\0Reinhard\0ACES\0"))
            {
                tonemap.tonemapper = static_cast<render::Tonemapper>(tonemapper);
            }

            ImGui::SliderFloat("Exposure", &tonemap.exposure, k_minExposure, k_maxExposure, "%.2f",
                ImGuiSliderFlags_Logarithmic);
            ImGui::Checkbox("Dither", &tonemap.dither);
        }
    } //namespace

    void drawGraphicsPanel(GraphicsPanelState& panel, render::RendererState& renderer)
    {
        const vela::graphics::FrameStats frameStats = renderer.graph->getFrameStats();

        if (frameStats.gpuTimingValid)
        {
            panel.smoothedGpuMs += (frameStats.gpuFrameMs - panel.smoothedGpuMs) * k_gpuSmoothing;
        }

        ImGui::SetNextWindowPos(k_panelPosition, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(k_panelWidth, 0.0f), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Graphics"))
        {
            const render::FrameLightStats& lights = renderer.lastFrameLights;

            ImGui::Text("GPU frame (smoothed) %.3f ms", static_cast<double>(panel.smoothedGpuMs));
            ImGui::TextDisabled("Toggle a feature and compare to read its cost.");
            ImGui::Text("Lights: %u local (%u dropped), directional %s", lights.local, lights.dropped,
                lights.hasDirectional ? "yes" : "no");
            ImGui::Text("Shadow caster: %s", casterName(lights.caster));
            ImGui::Separator();

            render::RenderSettings& settings = renderer.settings;
            drawLighting(settings.lighting);
            drawShadows(settings.shadows);
            drawFog(settings.fog);
            drawTonemap(settings.tonemap);
        }

        ImGui::End();
    }
} //namespace lockstep::debug
