#if LOCKSTEP_DEV_TOOLS
#include "Weather/Weather.hpp"

#include <imgui.h>

namespace game
{
    namespace
    {
        void windSection(Weather& weather)
        {
            if (!ImGui::TreeNode("Wind"))
            {
                return;
            }

            WindTuning& tuning = weather.tuning.wind;
            ImGui::Checkbox("Blow", &tuning.enabled);
            ImGui::SliderFloat("Clear speed (m/s)", &tuning.clearSpeed, 0.0f, 20.0f, "%.1f");
            ImGui::SliderFloat("Storm speed (m/s)", &tuning.stormSpeed, 0.0f, 30.0f, "%.1f");
            ImGui::SliderFloat("Gustiness", &tuning.gustiness, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Heading (deg)", &tuning.heading, 0.0f, 360.0f, "%.0f");
            ImGui::SliderFloat("Heading wander (deg)", &tuning.wander, 0.0f, 90.0f, "%.0f");
            ImGui::SliderFloat("Cloud speed (x wind)", &tuning.cloudSpeed, 0.0f, 20.0f, "%.1f");
            ImGui::Checkbox("Litter", &weather.tuning.litter);
            ImGui::SameLine();
            ImGui::SliderFloat("Litter density", &weather.tuning.litterDensity, 0.0f, 4.0f, "%.2f");
            ImGui::Text("Now %.1f m/s, gust %.2f, litter rate %.2f", weather.wind.speed, weather.wind.gust,
                litterRate(weather));
            ImGui::TreePop();
        }

        void lightningSection(Weather& weather)
        {
            if (!ImGui::TreeNode("Lightning"))
            {
                return;
            }

            LightningTuning& tuning = weather.tuning.lightning;
            ImGui::Checkbox("Strike", &tuning.enabled);
            ImGui::SameLine();
            ImGui::Checkbox("Bolts", &tuning.bolts);
            ImGui::SliderFloat("Min rain", &tuning.minRain, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Min seconds between", &tuning.minSeconds, 1.0f, 120.0f, "%.0f");
            ImGui::SliderFloat("Max seconds between", &tuning.maxSeconds, 1.0f, 300.0f, "%.0f");
            ImGui::SliderFloat("Nearest (m)", &tuning.minDistance, 200.0f, 5000.0f, "%.0f");
            ImGui::SliderFloat("Farthest (m)", &tuning.maxDistance, 500.0f, 20000.0f, "%.0f");
            ImGui::SliderFloat("Brightness", &tuning.brightness, 0.0f, 3.0f, "%.2f");
            ImGui::Text("Next strike in %.1f s, flash %.2f, %zu thunder on the way", weather.lightning.untilStrike,
                weather.lightning.flash, weather.lightning.pending.size());

            if (ImGui::Button("Strike now"))
            {
                strikeLightning(weather.lightning, tuning);
            }

            ImGui::TreePop();
        }

        void lensSection(Weather& weather)
        {
            if (!ImGui::TreeNode("Rain on the lens"))
            {
                return;
            }

            LensDropTuning& tuning = weather.tuning.lens;
            ImGui::Checkbox("Drops", &tuning.enabled);
            ImGui::SliderFloat("Drops per second", &tuning.rate, 0.0f, 60.0f, "%.1f");
            ImGui::SliderFloat("Looking up boost", &tuning.lookUpBoost, 0.0f, 10.0f, "%.1f");
            ImGui::SliderFloat("Into the wind boost", &tuning.windBoost, 0.0f, 4.0f, "%.2f");
            ImGui::SliderFloat("Drying indoors", &tuning.insideDrying, 1.0f, 10.0f, "%.1f");
            const lockstep::render::LensRainStats stats = weather.lens.drops.stats();
            ImGui::Text("%u drops (%u running), coverage %.2f, exposure %.2f%s", stats.drops, stats.running,
                stats.coverage, weather.lens.exposure, weather.lens.isOpen ? " (open sky)" : "");

            if (ImGui::Button("Wipe lens"))
            {
                weather.lens.drops.wipe(0.35f);
            }

            ImGui::SameLine();

            if (ImGui::Button("Clear lens"))
            {
                weather.lens.drops.clear();
            }

            ImGui::TreePop();
        }
    } //namespace

    void weatherDevSection(Weather& weather)
    {
        if (!ImGui::TreeNode("Weather"))
        {
            return;
        }

        bool isRaining = weather.kind == WeatherKind::Rain;

        if (ImGui::Checkbox("Rain", &isRaining))
        {
            weather.kind = isRaining ? WeatherKind::Rain : WeatherKind::Clear;
        }

        WeatherTuning& tuning = weather.tuning;
        ImGui::SliderFloat("Intensity", &tuning.intensity, 0.0f, 1.0f, "%.2f");
        ImGui::Text("Wetness %.2f, overcast look %.2f%s", weather.wetness, weather.lookAmount,
            weather.forced ? " (--weather)" : "");
        ImGui::Checkbox("Rain streaks", &tuning.particles);
        ImGui::SameLine();
        ImGui::Checkbox("Splashes", &tuning.splashes);
        ImGui::Checkbox("Wet surfaces", &tuning.wetness);
        ImGui::SameLine();
        ImGui::Checkbox("Overcast look", &tuning.overcastLook);
        ImGui::SameLine();
        ImGui::Checkbox("Sound", &tuning.sound);
        ImGui::SliderFloat("Streak density", &tuning.streakDensity, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Splashes per second", &tuning.splashRate, 0.0f, 600.0f, "%.0f");
        ImGui::SliderFloat("Soak time (s)", &tuning.soakSeconds, 0.0f, 120.0f, "%.0f");
        ImGui::SliderFloat("Dry time (s)", &tuning.drySeconds, 0.0f, 600.0f, "%.0f");

        if (ImGui::Button("Soak now"))
        {
            weather.wetness = 1.0f;
        }

        ImGui::SameLine();

        if (ImGui::Button("Dry now"))
        {
            weather.wetness = 0.0f;
        }

        windSection(weather);
        lightningSection(weather);
        lensSection(weather);
        ImGui::TreePop();
    }
} //namespace game
#endif
