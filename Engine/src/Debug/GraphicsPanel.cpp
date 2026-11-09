#include "GraphicsPanel.hpp"

#include <imgui.h>

#include <vector>

namespace lockstep::debug
{
    namespace
    {
        constexpr float k_gpuSmoothing{0.05f};
        constexpr float k_panelWidth{340.0f};
        constexpr ImVec2 k_panelPosition{360.0f, 10.0f};
        constexpr float k_maxAmbient{1.0f};
        constexpr float k_maxShadowBias{5.0f};
        constexpr float k_minShadowDistance{10.0f};
        constexpr float k_maxShadowDistance{300.0f};
        constexpr float k_maxFogDensity{0.1f};
        constexpr float k_maxFogStart{100.0f};
        constexpr float k_minExposure{0.05f};
        constexpr float k_maxExposure{8.0f};
        constexpr float k_maxSunIntensity{10.0f};
        constexpr float k_maxSkyIntensity{4.0f};
        constexpr float k_maxIblIntensity{3.0f};
        constexpr float k_minTurbidity{1.7f};
        constexpr float k_maxTurbidity{10.0f};
        constexpr float k_maxOcclusionRadius{3.0f};
        constexpr float k_maxOcclusionIntensity{3.0f};
        constexpr float k_maxOcclusionPower{4.0f};
        constexpr float k_maxBloomStrength{0.3f};
        constexpr int k_maxCascades{static_cast<int>(render::k_maxShadowCascades)};

        bool header(const char* label)
        {
            return ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
        }

        void smooth(float& smoothed, float sample)
        {
            smoothed += (sample - smoothed) * k_gpuSmoothing;
        }

        void drawPassTimings(GraphicsPanelState& panel, const std::vector<vela::graphics::PassTiming>& timings)
        {
            if (!ImGui::CollapsingHeader("GPU time per pass"))
            {
                return;
            }

            if (timings.empty())
            {
                ImGui::TextDisabled("No pass timings (no GPU timestamps yet)");
                return;
            }

            if (!ImGui::BeginTable("passTimings", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
            {
                return;
            }

            ImGui::TableSetupColumn("Pass");
            ImGui::TableSetupColumn("ms (smoothed)");
            ImGui::TableHeadersRow();

            for (const vela::graphics::PassTiming& timing : timings)
            {
                const float smoothed = panel.smoothedPassMs[timing.name];

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(timing.name.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", static_cast<double>(smoothed));
            }

            ImGui::EndTable();
        }

        void drawLighting(render::LightingSettings& lighting)
        {
            if (!header("Lighting"))
            {
                return;
            }

            ImGui::Checkbox("Sun / directional light", &lighting.directional);
            ImGui::Checkbox("Point / spot lights", &lighting.localLights);
            ImGui::Checkbox("Specular", &lighting.specular);
            ImGui::ColorEdit3("Flat ambient colour", &lighting.ambientColor.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Flat ambient", &lighting.ambientIntensity, 0.0f, k_maxAmbient, "%.3f",
                ImGuiSliderFlags_Logarithmic);
        }

        void drawSky(render::SkySettings& sky, render::ImageBasedLightingSettings& ibl)
        {
            if (!header("Sun and sky"))
            {
                return;
            }

            ImGui::Checkbox("Sky (and its sun)", &sky.enabled);
            ImGui::SliderAngle("Sun azimuth", &sky.sunAzimuth, -180.0f, 180.0f);
            ImGui::SliderAngle("Sun elevation", &sky.sunElevation, -10.0f, 90.0f);
            ImGui::ColorEdit3("Sun colour", &sky.sunColor.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Sun intensity", &sky.sunIntensity, 0.0f, k_maxSunIntensity, "%.2f");
            ImGui::Checkbox("Atmospheric sun tint", &sky.atmosphericSunTint);
            ImGui::Checkbox("Sun disk", &sky.sunDisk);
            ImGui::SliderFloat("Turbidity", &sky.turbidity, k_minTurbidity, k_maxTurbidity, "%.1f");
            ImGui::SliderFloat("Sky intensity", &sky.skyIntensity, 0.0f, k_maxSkyIntensity, "%.2f");
            ImGui::ColorEdit3("Ground albedo", &sky.groundAlbedo.x, ImGuiColorEditFlags_Float);
            ImGui::Separator();
            ImGui::Checkbox("Image-based lighting", &ibl.enabled);
            ImGui::SliderFloat("IBL diffuse", &ibl.diffuseIntensity, 0.0f, k_maxIblIntensity, "%.2f");
            ImGui::SliderFloat("IBL specular", &ibl.specularIntensity, 0.0f, k_maxIblIntensity, "%.2f");
        }

        void drawShadows(render::ShadowSettings& shadows)
        {
            if (!header("Shadows"))
            {
                return;
            }

            int cascades = static_cast<int>(shadows.cascadeCount);

            ImGui::Checkbox("Shadow maps", &shadows.enabled);

            if (ImGui::SliderInt("Cascades", &cascades, 1, k_maxCascades))
            {
                shadows.cascadeCount = static_cast<uint32_t>(cascades);
            }

            ImGui::SliderFloat("Distance (m)", &shadows.distance, k_minShadowDistance, k_maxShadowDistance, "%.0f");
            ImGui::SliderFloat("Split lambda", &shadows.splitLambda, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Slope depth bias", &shadows.depthBias, 0.0f, k_maxShadowBias, "%.2f");
            ImGui::SliderFloat("Normal bias (texels)", &shadows.normalBias, 0.0f, k_maxShadowBias, "%.2f");
            ImGui::Checkbox("Show cascades", &shadows.debugCascades);

            int maskedCascades = static_cast<int>(shadows.maskedCasterCascades);

            if (ImGui::SliderInt("Foliage cascades", &maskedCascades, 0, k_maxCascades))
            {
                shadows.maskedCasterCascades = static_cast<uint32_t>(maskedCascades);
            }
        }

        void drawFog(render::FogSettings& fog)
        {
            if (!ImGui::CollapsingHeader("Fog"))
            {
                return;
            }

            ImGui::Checkbox("Distance fog", &fog.enabled);
            ImGui::ColorEdit3("Fog colour", &fog.color.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Density", &fog.density, 0.0f, k_maxFogDensity, "%.4f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("Start (m)", &fog.start, 0.0f, k_maxFogStart, "%.1f");
            ImGui::SliderFloat("Max amount", &fog.maxAmount, 0.0f, 1.0f, "%.2f");
        }

        void drawAntiAliasing(render::AntiAliasingSettings& antiAliasing)
        {
            if (!header("Anti-aliasing"))
            {
                return;
            }

            int mode = static_cast<int>(antiAliasing.mode);

            if (ImGui::Combo("Mode", &mode, "Off\0" "FXAA\0" "MSAA 2x\0" "MSAA 4x\0" "TAA\0"))
            {
                antiAliasing.mode = static_cast<render::AntiAliasingMode>(mode);
            }

            if (antiAliasing.mode != render::AntiAliasingMode::Taa)
            {
                return;
            }

            ImGui::SliderFloat("History (still)", &antiAliasing.historyWeight, 0.5f, 0.98f, "%.3f");
            ImGui::SliderFloat("History (moving)", &antiAliasing.motionWeight, 0.0f, 0.98f, "%.3f");
            ImGui::SliderFloat("Motion for full falloff (px)", &antiAliasing.motionPixels, 1.0f, 64.0f, "%.1f");
            ImGui::SliderFloat("Sharpness", &antiAliasing.sharpness, 0.0f, 1.0f, "%.2f");
            ImGui::Checkbox("Jitter", &antiAliasing.jitter);
        }

        void drawReflections(render::PlanarReflectionSettings& reflections)
        {
            if (!header("Planar reflections"))
            {
                return;
            }

            ImGui::Checkbox("Mirrors", &reflections.enabled);
            ImGui::SliderFloat("Resolution scale", &reflections.resolutionScale, 0.25f, 1.0f, "%.2f");
            ImGui::SliderFloat("Max distance (m)", &reflections.maxDistance, 1.0f, 30.0f, "%.1f");
        }

        void drawPostProcessing(render::RenderSettings& settings)
        {
            if (!header("Post-processing"))
            {
                return;
            }

            render::AmbientOcclusionSettings& occlusion = settings.ambientOcclusion;
            ImGui::Checkbox("SSAO", &occlusion.enabled);
            ImGui::SliderFloat("SSAO radius (m)", &occlusion.radius, 0.05f, k_maxOcclusionRadius, "%.2f");
            ImGui::SliderFloat("SSAO intensity", &occlusion.intensity, 0.0f, k_maxOcclusionIntensity, "%.2f");
            ImGui::SliderFloat("SSAO power", &occlusion.power, 0.5f, k_maxOcclusionPower, "%.2f");
            ImGui::Checkbox("Show SSAO only", &occlusion.showOnly);
            ImGui::Separator();
            ImGui::Checkbox("Bloom", &settings.bloom.enabled);
            ImGui::SliderFloat("Bloom strength", &settings.bloom.strength, 0.0f, k_maxBloomStrength, "%.3f");
            ImGui::Checkbox("Frustum culling", &settings.culling.frustumCulling);
        }

        void drawTonemap(render::TonemapSettings& tonemap)
        {
            if (!header("Exposure and tonemapping"))
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

        const std::vector<vela::graphics::PassTiming>& passTimings = renderer.graph->getPassTimings();

        if (frameStats.gpuTimingValid)
        {
            smooth(panel.smoothedGpuMs, frameStats.gpuFrameMs);
        }

        for (const vela::graphics::PassTiming& timing : passTimings)
        {
            smooth(panel.smoothedPassMs[timing.name], timing.gpuMs);
        }

        ImGui::SetNextWindowPos(k_panelPosition, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(k_panelWidth, 0.0f), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Graphics"))
        {
            const render::FrameStats& frame = renderer.lastFrame;

            ImGui::Text("GPU frame (smoothed) %.3f ms", static_cast<double>(panel.smoothedGpuMs));
            ImGui::TextDisabled("Toggle a feature and compare to read its cost.");
            ImGui::Text("Draws: %u submitted, %u visible, %u shadow", frame.submitted, frame.visible,
                frame.shadowDraws);
            ImGui::Text("Lights: %u local (%u dropped), sun %s", frame.local, frame.dropped,
                frame.hasDirectional ? "yes" : "no");
            ImGui::Text("Shadows: %u cascades%s", frame.cascades, frame.spotShadow ? " + spot" : "");
            ImGui::Text("Mirror: %s, %u draws", renderer.reflection.active ? "rendered" : "none",
                static_cast<uint32_t>(renderer.reflection.opaque.size() + renderer.reflection.blended.size()));
            drawPassTimings(panel, passTimings);
            ImGui::Separator();

            render::RenderSettings& settings = renderer.settings;
            drawLighting(settings.lighting);
            drawSky(settings.sky, settings.ibl);
            drawShadows(settings.shadows);
            drawPostProcessing(settings);
            drawAntiAliasing(settings.antiAliasing);
            drawReflections(settings.reflections);
            drawTonemap(settings.tonemap);
            drawFog(settings.fog);
        }

        ImGui::End();
    }
} //namespace lockstep::debug
