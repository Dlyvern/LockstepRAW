#include "IndirectLightingPanel.hpp"

#include <imgui.h>

namespace lockstep::debug
{
    namespace
    {
        constexpr float k_maxOcclusionRadius{3.0f};
        constexpr float k_maxOcclusionIntensity{2.0f};
        constexpr float k_maxOcclusionPower{4.0f};
        constexpr int k_maxSlices{4};
        constexpr int k_minOcclusionSteps{2};
        constexpr int k_maxOcclusionSteps{12};
        constexpr float k_maxContactLength{1.5f};
        constexpr float k_maxContactThickness{0.5f};
        constexpr int k_minContactSteps{4};
        constexpr int k_maxContactSteps{24};
        constexpr float k_maxReflectionIntensity{2.0f};
        constexpr float k_maxReflectionDistance{80.0f};
        constexpr float k_maxReflectionThickness{2.0f};
        constexpr int k_minReflectionSteps{8};
        constexpr int k_maxReflectionSteps{64};

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

        void drawAmbientOcclusion(render::AmbientOcclusionSettings& occlusion)
        {
            if (!ImGui::CollapsingHeader("Ambient occlusion (GTAO)"))
            {
                return;
            }

            ImGui::Checkbox("Ambient occlusion", &occlusion.enabled);
            ImGui::SliderFloat("AO radius (m)", &occlusion.radius, 0.05f, k_maxOcclusionRadius, "%.2f");
            ImGui::SliderFloat("AO intensity", &occlusion.intensity, 0.0f, k_maxOcclusionIntensity, "%.2f");
            ImGui::SliderFloat("AO power", &occlusion.power, 0.5f, k_maxOcclusionPower, "%.2f");
            sliderCount("AO slices", occlusion.slices, 1, k_maxSlices);
            sliderCount("AO steps per side", occlusion.steps, k_minOcclusionSteps, k_maxOcclusionSteps);
            ImGui::Checkbox("Multi-bounce", &occlusion.multiBounce);
            ImGui::SliderFloat("Multi-bounce albedo", &occlusion.multiBounceAlbedo, 0.0f, 1.0f, "%.2f");
            ImGui::Checkbox("Specular occlusion", &occlusion.specularOcclusion);
            ImGui::Checkbox("Show AO only", &occlusion.showOnly);
        }

        void drawContactShadows(render::ContactShadowSettings& contact)
        {
            if (!ImGui::CollapsingHeader("Contact shadows"))
            {
                return;
            }

            ImGui::Checkbox("Contact shadows (sun)", &contact.enabled);
            ImGui::SliderFloat("Contact length (m)", &contact.length, 0.02f, k_maxContactLength, "%.2f");
            ImGui::SliderFloat("Contact thickness (m)", &contact.thickness, 0.005f, k_maxContactThickness, "%.3f");
            ImGui::SliderFloat("Contact intensity", &contact.intensity, 0.0f, 1.0f, "%.2f");
            sliderCount("Contact steps", contact.steps, k_minContactSteps, k_maxContactSteps);
        }

        void drawScreenSpaceReflections(render::ScreenSpaceReflectionSettings& ssr)
        {
            if (!ImGui::CollapsingHeader("Screen-space reflections"))
            {
                return;
            }

            ImGui::Checkbox("SSR", &ssr.enabled);
            ImGui::SliderFloat("SSR intensity", &ssr.intensity, 0.0f, k_maxReflectionIntensity, "%.2f");
            ImGui::SliderFloat("SSR max roughness", &ssr.maxRoughness, 0.05f, 1.0f, "%.2f");
            ImGui::SliderFloat("SSR max distance (m)", &ssr.maxDistance, 1.0f, k_maxReflectionDistance, "%.1f");
            sliderCount("SSR steps", ssr.steps, k_minReflectionSteps, k_maxReflectionSteps);
            ImGui::SliderFloat("SSR thickness (m)", &ssr.thickness, 0.01f, k_maxReflectionThickness, "%.2f",
                ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("SSR edge fade", &ssr.edgeFade, 0.001f, 0.5f, "%.3f");
            ImGui::Checkbox("SSR temporal", &ssr.temporal);
            ImGui::SliderFloat("SSR history weight", &ssr.historyWeight, 0.0f, 0.97f, "%.2f");
            ImGui::Checkbox("Show SSR only", &ssr.debugView);
        }
    } //namespace

    void drawIndirectLightingSections(render::RenderSettings& settings)
    {
        drawAmbientOcclusion(settings.ambientOcclusion);
        drawContactShadows(settings.contactShadows);
        drawScreenSpaceReflections(settings.screenSpaceReflections);
    }
} //namespace lockstep::debug
