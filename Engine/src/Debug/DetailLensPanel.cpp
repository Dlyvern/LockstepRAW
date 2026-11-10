#include "DetailLensPanel.hpp"

#include <imgui.h>

namespace lockstep::debug
{
    namespace
    {
        constexpr float k_maxParallaxDistance{40.0f};
        constexpr float k_maxDetailDistance{30.0f};
        constexpr int k_minParallaxSteps{4};
        constexpr int k_maxParallaxSteps{64};
        constexpr float k_maxBlurPixels{64.0f};
        constexpr int k_minBlurSamples{4};
        constexpr int k_maxBlurSamples{16};
        constexpr float k_maxFocusDistance{50.0f};
        constexpr float k_maxBlurRadius{24.0f};
        constexpr int k_minDiscSamples{8};
        constexpr int k_maxDiscSamples{48};
        constexpr float k_maxAberration{6.0f};

        bool sliderCount(const char* label, uint32_t& value, int minimum, int maximum)
        {
            int edited = static_cast<int>(value);

            if (!ImGui::SliderInt(label, &edited, minimum, maximum))
            {
                return false;
            }

            value = static_cast<uint32_t>(edited);
            return true;
        }

        void drawSurfaceDetail(render::SurfaceDetailSettings& surface)
        {
            if (!ImGui::CollapsingHeader("Surface detail"))
            {
                return;
            }

            ImGui::Checkbox("Parallax occlusion", &surface.parallax);
            ImGui::SliderFloat("Parallax distance (m)", &surface.parallaxDistance, 1.0f, k_maxParallaxDistance, "%.1f");
            static_cast<void>(sliderCount("Parallax steps (head-on)", surface.parallaxMinSteps, k_minParallaxSteps,
                k_maxParallaxSteps));
            static_cast<void>(sliderCount("Parallax steps (grazing)", surface.parallaxMaxSteps, k_minParallaxSteps,
                k_maxParallaxSteps));
            ImGui::Checkbox("Relief self-shadowing", &surface.parallaxShadows);
            ImGui::Separator();
            ImGui::Checkbox("Micro-detail layer", &surface.detail);
            ImGui::SliderFloat("Detail distance (m)", &surface.detailDistance, 1.0f, k_maxDetailDistance, "%.1f");
            ImGui::SliderFloat("Detail strength", &surface.detailStrength, 0.0f, 2.0f, "%.2f");
        }

        void drawLens(render::RenderSettings& settings)
        {
            if (!ImGui::CollapsingHeader("Lens"))
            {
                return;
            }

            render::MotionBlurSettings& blur = settings.motionBlur;
            ImGui::Checkbox("Motion blur", &blur.enabled);
            ImGui::SliderFloat("Shutter", &blur.intensity, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Max smear (px)", &blur.maxPixels, 1.0f, k_maxBlurPixels, "%.0f");
            static_cast<void>(sliderCount("Blur samples", blur.samples, k_minBlurSamples, k_maxBlurSamples));
            ImGui::SliderFloat("Viewmodel blur", &blur.viewModelScale, 0.0f, 1.0f, "%.2f");
            ImGui::Separator();

            render::DepthOfFieldSettings& focus = settings.depthOfField;
            ImGui::Checkbox("Depth of field", &focus.enabled);
            ImGui::SameLine();
            ImGui::Checkbox("Preview (full strength)", &focus.preview);
            ImGui::Text("Strength (Game, bullet-time): %.2f", static_cast<double>(focus.strength));
            ImGui::SliderFloat("Focus distance (m, 0 auto)", &focus.focusDistance, 0.0f, k_maxFocusDistance, "%.1f");
            ImGui::SliderFloat("Sharp band", &focus.focusTolerance, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Blur growth", &focus.blurScale, 0.1f, 6.0f, "%.2f");
            ImGui::SliderFloat("Max radius (px)", &focus.maxRadius, 1.0f, k_maxBlurRadius, "%.1f");
            static_cast<void>(sliderCount("Disc samples", focus.samples, k_minDiscSamples, k_maxDiscSamples));
            ImGui::Separator();

            ImGui::Checkbox("Chromatic aberration", &settings.chromaticAberration.enabled);
            ImGui::SliderFloat("Aberration (px at corners)", &settings.chromaticAberration.intensity, 0.0f,
                k_maxAberration, "%.2f");
            ImGui::Checkbox("Lens dirt", &settings.lensDirt.enabled);
            ImGui::SliderFloat("Dirt intensity", &settings.lensDirt.intensity, 0.0f, 2.0f, "%.2f");
        }
    } //namespace

    void drawDetailLensSections(render::RenderSettings& settings)
    {
        drawSurfaceDetail(settings.surfaceDetail);
        drawLens(settings);
    }
} //namespace lockstep::debug
