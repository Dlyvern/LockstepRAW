#include "GraphicsLookPanel.hpp"

#include <imgui.h>

namespace lockstep::debug
{
    namespace
    {
        constexpr float k_maxZoneIbl{1.0f};
        constexpr float k_maxFillIntensity{0.5f};
        constexpr float k_maxBlendDistance{3.0f};
        constexpr float k_maxNormalOffset{1.0f};
        constexpr float k_minTargetLuminance{0.01f};
        constexpr float k_maxTargetLuminance{0.5f};
        constexpr float k_evRange{6.0f};
        constexpr float k_maxAdaptationSpeed{10.0f};
        constexpr float k_liftRange{0.2f};
        constexpr float k_maxGrain{0.2f};
        constexpr float k_maxFogDensity{0.3f};
        constexpr float k_maxHeightFalloff{2.0f};
        constexpr float k_heightRange{50.0f};
        constexpr float k_maxFogLight{8.0f};
        constexpr float k_maxFogAmbient{0.5f};
        constexpr float k_maxInteriorScale{8.0f};
        constexpr float k_maxFogDistance{150.0f};
        constexpr int k_minFogSteps{8};
        constexpr int k_maxFogSteps{64};
        constexpr float k_maxPuddleSize{10.0f};

        bool header(const char* label)
        {
            return ImGui::CollapsingHeader(label);
        }

        void drawInteriorAmbient(render::InteriorAmbientSettings& interior, std::size_t zoneCount)
        {
            if (!header("Interior ambient zones"))
            {
                return;
            }

            ImGui::Text("%zu zones in the level", zoneCount);
            ImGui::Checkbox("Interior zones", &interior.enabled);
            ImGui::SliderFloat("Sky diffuse inside", &interior.diffuseIntensity, 0.0f, k_maxZoneIbl, "%.3f");
            ImGui::SliderFloat("Sky reflections inside", &interior.specularIntensity, 0.0f, k_maxZoneIbl, "%.3f");
            ImGui::ColorEdit3("Fill colour", &interior.fillColor.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Fill intensity", &interior.fillIntensity, 0.0f, k_maxFillIntensity, "%.3f",
                ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("Blend distance (m)", &interior.blendDistance, 0.01f, k_maxBlendDistance, "%.2f");
            ImGui::SliderFloat("Normal offset (m)", &interior.normalOffset, 0.0f, k_maxNormalOffset, "%.2f");
            ImGui::Checkbox("Show zone weight", &interior.debugView);
        }

        void drawVolumetricFog(render::VolumetricFogSettings& fog)
        {
            if (!header("Volumetric fog"))
            {
                return;
            }

            int steps = static_cast<int>(fog.steps);
            ImGui::Checkbox("Volumetric fog", &fog.enabled);
            ImGui::SliderFloat("Haze density", &fog.density, 0.0f, k_maxFogDensity, "%.4f",
                ImGuiSliderFlags_Logarithmic);
            ImGui::ColorEdit3("Haze albedo", &fog.albedo.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Anisotropy (g)", &fog.anisotropy, -0.9f, 0.9f, "%.2f");
            ImGui::SliderFloat("Height falloff", &fog.heightFalloff, 0.0f, k_maxHeightFalloff, "%.3f");
            ImGui::SliderFloat("Base height (m)", &fog.baseHeight, -k_heightRange, k_heightRange, "%.1f");
            ImGui::SliderFloat("Sun scattering", &fog.sunIntensity, 0.0f, k_maxFogLight, "%.2f");
            ImGui::SliderFloat("Local light scattering", &fog.localLightIntensity, 0.0f, k_maxFogLight, "%.2f");
            ImGui::SliderFloat("Ambient glow", &fog.ambientIntensity, 0.0f, k_maxFogAmbient, "%.4f",
                ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("Interior density x", &fog.interiorDensityScale, 0.0f, k_maxInteriorScale, "%.2f");
            ImGui::SliderFloat("Max distance (m)", &fog.maxDistance, 1.0f, k_maxFogDistance, "%.0f");

            if (ImGui::SliderInt("Steps", &steps, k_minFogSteps, k_maxFogSteps))
            {
                fog.steps = static_cast<uint32_t>(steps);
            }

            ImGui::SliderFloat("Resolution scale", &fog.resolutionScale, 0.25f, 1.0f, "%.2f");
            ImGui::Checkbox("Temporal accumulation", &fog.temporal);
            ImGui::SliderFloat("History weight", &fog.historyWeight, 0.0f, 0.98f, "%.2f");
        }

        void drawWetness(render::WetnessSettings& wetness)
        {
            if (!header("Surface wetness"))
            {
                return;
            }

            ImGui::Checkbox("Wet surfaces", &wetness.enabled);
            ImGui::SliderFloat("Wetness", &wetness.amount, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Puddle coverage", &wetness.puddles, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Puddle size (m)", &wetness.puddleSize, 0.2f, k_maxPuddleSize, "%.2f");
            ImGui::SliderFloat("Porous darkening", &wetness.darkening, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Glossiness", &wetness.glossiness, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Ripples", &wetness.ripples, 0.0f, 1.0f, "%.2f");
            ImGui::Checkbox("Show wetness (blue) and puddles (cyan)", &wetness.debugView);
        }

        void drawAutoExposure(render::AutoExposureSettings& exposure)
        {
            if (!header("Auto exposure"))
            {
                return;
            }

            ImGui::Checkbox("Eye adaptation", &exposure.enabled);
            ImGui::SliderFloat("Target luminance", &exposure.targetLuminance, k_minTargetLuminance,
                k_maxTargetLuminance, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("Min EV", &exposure.minEv, -k_evRange, 0.0f, "%.2f");
            ImGui::SliderFloat("Max EV", &exposure.maxEv, 0.0f, k_evRange, "%.2f");
            ImGui::SliderFloat("Compensation EV", &exposure.compensationEv, -k_evRange * 0.5f, k_evRange * 0.5f,
                "%.2f");
            ImGui::SliderFloat("Brighten speed", &exposure.brightenSpeed, 0.0f, k_maxAdaptationSpeed, "%.2f");
            ImGui::SliderFloat("Darken speed", &exposure.darkenSpeed, 0.0f, k_maxAdaptationSpeed, "%.2f");
            ImGui::SliderFloat("Centre weight", &exposure.centreWeight, 0.0f, 1.0f, "%.2f");
        }

        void drawColorGrading(render::ColorGradingSettings& grading)
        {
            if (!header("Colour grading"))
            {
                return;
            }

            ImGui::Checkbox("Grade", &grading.enabled);
            ImGui::SliderFloat3("Lift", &grading.lift.x, -k_liftRange, k_liftRange, "%.3f");
            ImGui::SliderFloat3("Gamma", &grading.gamma.x, 0.5f, 1.5f, "%.3f");
            ImGui::SliderFloat3("Gain", &grading.gain.x, 0.5f, 1.5f, "%.3f");
            ImGui::SliderFloat("Contrast", &grading.contrast, 0.5f, 2.0f, "%.2f");
            ImGui::SliderFloat("Saturation", &grading.saturation, 0.0f, 2.0f, "%.2f");
            ImGui::ColorEdit3("Shadow tint", &grading.shadowTint.x, ImGuiColorEditFlags_Float);
            ImGui::ColorEdit3("Highlight tint", &grading.highlightTint.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Split-tone strength", &grading.splitToneStrength, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Split-tone balance", &grading.splitToneBalance, 0.0f, 1.0f, "%.2f");
        }

        void drawFilmEffects(render::VignetteSettings& vignette, render::FilmGrainSettings& grain)
        {
            if (!header("Vignette and film grain"))
            {
                return;
            }

            ImGui::Checkbox("Vignette", &vignette.enabled);
            ImGui::SliderFloat("Vignette intensity", &vignette.intensity, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Vignette radius", &vignette.radius, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Vignette smoothness", &vignette.smoothness, 0.01f, 1.0f, "%.2f");
            ImGui::Separator();
            ImGui::Checkbox("Film grain", &grain.enabled);
            ImGui::SliderFloat("Grain intensity", &grain.intensity, 0.0f, k_maxGrain, "%.3f");
            ImGui::SliderFloat("Grain highlight falloff", &grain.response, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Grain size (px)", &grain.size, 1.0f, 3.0f, "%.2f");
        }
    } //namespace

    void drawLookSections(render::RenderSettings& settings, std::size_t zoneCount)
    {
        drawInteriorAmbient(settings.interiorAmbient, zoneCount);
        drawVolumetricFog(settings.volumetricFog);
        drawWetness(settings.wetness);
        drawAutoExposure(settings.autoExposure);
        drawColorGrading(settings.colorGrading);
        drawFilmEffects(settings.vignette, settings.filmGrain);
    }
} //namespace lockstep::debug
