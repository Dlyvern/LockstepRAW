#include "AtmospherePanel.hpp"

#include "Lockstep/Render/RenderSettings.hpp"

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

        void drawClouds(render::CloudSettings& clouds)
        {
            if (!ImGui::CollapsingHeader("Clouds"))
            {
                return;
            }

            ImGui::Checkbox("Clouds", &clouds.enabled);
            ImGui::SameLine();
            ImGui::Checkbox("Volumetric (off: flat layer)", &clouds.volumetric);
            ImGui::SliderFloat("Coverage##clouds", &clouds.coverage, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Density##clouds", &clouds.density, 0.0f, 4.0f, "%.2f");
            ImGui::SliderFloat("Base (m)##clouds", &clouds.baseHeight, 200.0f, 5000.0f, "%.0f");
            ImGui::SliderFloat("Thickness (m)##clouds", &clouds.thickness, 100.0f, 4000.0f, "%.0f");
            ImGui::SliderFloat("Feature size##clouds", &clouds.scale, 0.2f, 4.0f, "%.2f");
            ImGui::ColorEdit3("Ambient##clouds", &clouds.ambient.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Sunlight##clouds", &clouds.sunLight, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat("Silver lining##clouds", &clouds.silverLining, 0.0f, 0.9f, "%.2f");
            ImGui::SliderFloat("Influence on sky light##clouds", &clouds.iblInfluence, 0.0f, 1.0f, "%.2f");
            sliderCount("Steps##clouds", clouds.steps, 8, 64);
            ImGui::SliderFloat("Resolution##clouds", &clouds.resolutionScale, 0.25f, 1.0f, "%.2f");
            ImGui::Text("Drift %.0f, %.0f m (Game moves it with the wind)", static_cast<double>(clouds.offset.x),
                static_cast<double>(clouds.offset.y));
        }

        void drawWind(render::WindSettings& wind)
        {
            if (!ImGui::CollapsingHeader("Wind (foliage)"))
            {
                return;
            }

            ImGui::Checkbox("Foliage sway", &wind.enabled);
            ImGui::SliderFloat("Sway##wind", &wind.sway, 0.0f, 3.0f, "%.2f");
            ImGui::Text("Speed %.1f m/s towards (%.2f, %.2f) (set by Game)", static_cast<double>(wind.speed),
                static_cast<double>(wind.direction.x), static_cast<double>(wind.direction.y));
        }

        void drawLightning(render::LightningSettings& lightning)
        {
            if (!ImGui::CollapsingHeader("Lightning flash"))
            {
                return;
            }

            ImGui::ColorEdit3("Colour##lightning", &lightning.color.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Light intensity##lightning", &lightning.lightIntensity, 0.0f, 20.0f, "%.1f");
            ImGui::SliderFloat("Sky intensity##lightning", &lightning.skyIntensity, 0.0f, 10.0f, "%.1f");
            ImGui::Checkbox("Casts shadows##lightning", &lightning.castsShadows);
            ImGui::Text("Flash %.2f, bolt %.0f (set by Game)", static_cast<double>(lightning.flash),
                static_cast<double>(lightning.bolt));
        }

        void drawLensRain(render::LensRainSettings& lens)
        {
            if (!ImGui::CollapsingHeader("Rain on the lens"))
            {
                return;
            }

            ImGui::Checkbox("Draw drops", &lens.enabled);
            ImGui::SliderFloat("Refraction##lens", &lens.refraction, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Blur##lens", &lens.blur, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Opacity##lens", &lens.opacity, 0.0f, 1.0f, "%.2f");
        }
    } //namespace

    void drawAtmosphereSections(render::RenderSettings& settings)
    {
        drawClouds(settings.clouds);
        drawWind(settings.wind);
        drawLightning(settings.lightning);
        drawLensRain(settings.lensRain);
    }
} //namespace lockstep::debug
