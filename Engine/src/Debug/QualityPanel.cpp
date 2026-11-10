#include "QualityPanel.hpp"

#include "Lockstep/Core/Log.hpp"
#include "Lockstep/Render/GraphicsConfig.hpp"
#include "Lockstep/Render/GraphicsQuality.hpp"

#include <imgui.h>

#include <cstdint>
#include <string>

namespace lockstep::debug
{
    namespace
    {
        constexpr double k_bytesPerMebibyte{1024.0 * 1024.0};
        constexpr int k_maxCascades{static_cast<int>(render::k_maxShadowCascades)};
        constexpr int k_maxShadowedLights{static_cast<int>(render::k_maxShadowedLocalLights)};
        constexpr int k_maxMipDrop{static_cast<int>(render::k_maxTextureMipDrop)};
        constexpr const char* k_presetNames{"Low\0" "Medium\0" "High\0" "Ultra\0" "Custom\0"};
        constexpr const char* k_antiAliasingNames{"Off\0" "FXAA\0" "MSAA 2x\0" "MSAA 4x\0" "TAA\0"};
        constexpr const char* k_atlasSizes[]{"1024", "2048", "4096", "8192"};

        bool sliderUnsigned(const char* label, uint32_t& value, int lowest, int highest)
        {
            int edited = static_cast<int>(value);

            if (!ImGui::SliderInt(label, &edited, lowest, highest))
            {
                return false;
            }

            value = static_cast<uint32_t>(edited);
            return true;
        }

        bool atlasCombo(uint32_t& size)
        {
            int index = 0;

            while (index < 3 && (render::k_minShadowAtlasSize << index) < size)
            {
                ++index;
            }

            if (!ImGui::Combo("Shadow atlas width", &index, k_atlasSizes, 4))
            {
                return false;
            }

            size = render::k_minShadowAtlasSize << index;
            return true;
        }

        bool drawResolution(render::GraphicsQuality& quality)
        {
            bool changed = ImGui::SliderFloat("Render scale", &quality.renderScale, render::k_minRenderScale,
                render::k_maxRenderScale, "%.2f");
            int antiAliasing = static_cast<int>(quality.antiAliasing);

            if (ImGui::Combo("Anti-aliasing", &antiAliasing, k_antiAliasingNames))
            {
                quality.antiAliasing = static_cast<render::AntiAliasingMode>(antiAliasing);
                changed = true;
            }

            changed |= sliderUnsigned("Texture mips dropped", quality.textureMipDrop, 0, k_maxMipDrop);
            ImGui::TextDisabled("Texture resolution applies to textures loaded later (reload the level).");
            changed |= atlasCombo(quality.shadowAtlasSize);
            changed |= sliderUnsigned("Sun cascades", quality.shadowCascades, 1, k_maxCascades);
            changed |= sliderUnsigned("Shadowed local lights", quality.shadowedLocalLights, 0, k_maxShadowedLights);

            return changed;
        }

        bool drawEffects(render::GraphicsQuality& quality)
        {
            bool changed = ImGui::SliderFloat("Effect steps", &quality.effectSteps, 0.25f, 2.0f, "%.2f");
            changed |= ImGui::Checkbox("Volumetric fog", &quality.volumetricFog);
            changed |= ImGui::SliderFloat("Fog resolution", &quality.volumetricFogResolution, 0.25f, 2.0f, "%.2f");
            changed |= ImGui::Checkbox("Screen-space reflections", &quality.screenSpaceReflections);
            changed |= ImGui::Checkbox("Ambient occlusion", &quality.ambientOcclusion);
            changed |= ImGui::Checkbox("Contact shadows", &quality.contactShadows);
            changed |= ImGui::Checkbox("Parallax", &quality.parallax);
            changed |= ImGui::Checkbox("Surface detail", &quality.surfaceDetail);
            changed |= ImGui::Checkbox("Motion blur", &quality.motionBlur);
            changed |= ImGui::Checkbox("Depth of field", &quality.depthOfField);
            changed |= ImGui::Checkbox("Lens effects", &quality.lensEffects);
            changed |= ImGui::Checkbox("Bloom##quality", &quality.bloom);
            changed |= ImGui::Checkbox("Mirrors##quality", &quality.planarReflections);
            changed |= ImGui::SliderFloat("Mirror resolution", &quality.reflectionResolution, 0.25f, 2.0f, "%.2f");
            changed |= sliderUnsigned("Scope resolution cap", quality.scopeResolution, 256, 1024);
            changed |= sliderUnsigned("Particle budget", quality.particleBudget, 1000, 50000);
            ImGui::TextDisabled("Particle budget applies when Game creates its particles (restart).");

            return changed;
        }

        void drawMemory(const render::RendererState& renderer)
        {
            const vela::core::MemoryStats memory = renderer.context.getMemoryStats();
            ImGui::Text("GPU memory: %.0f / %.0f MiB (%s)",
                static_cast<double>(memory.deviceBytesUsed) / k_bytesPerMebibyte,
                static_cast<double>(memory.deviceBytesBudget) / k_bytesPerMebibyte,
                memory.budgetFromDriver ? "driver" : "estimate");
        }

        void saveQuality(const render::RendererState& renderer)
        {
            if (renderer.settingsFile.empty())
            {
                log(LogLevel::Warning, "Render", "no settings file to save the graphics quality to");
                return;
            }

            if (auto saved = render::saveGraphicsQuality(renderer.settingsFile, renderer.quality); !saved)
            {
                log(LogLevel::Error, "Render", saved.error().message);
                return;
            }

            log(LogLevel::Info, "Render", "graphics quality saved to '" + renderer.settingsFile.string() + "'");
        }
    } //namespace

    void drawQualitySection(render::RendererState& renderer)
    {
        if (!ImGui::CollapsingHeader("Quality", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        drawMemory(renderer);
        render::GraphicsQuality quality = renderer.quality;
        int preset = static_cast<int>(quality.preset);

        if (ImGui::Combo("Preset", &preset, k_presetNames)
            && static_cast<render::GraphicsPreset>(preset) != render::GraphicsPreset::Custom)
        {
            quality = render::graphicsQualityPreset(static_cast<render::GraphicsPreset>(preset));
            renderer.quality = render::clampedQuality(quality);
        }

        bool changed = drawResolution(quality);
        changed |= drawEffects(quality);

        if (changed)
        {
            quality.preset = render::GraphicsPreset::Custom;
            renderer.quality = render::clampedQuality(quality);
        }

        if (ImGui::Button("Save to settings file"))
        {
            saveQuality(renderer);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%s", renderer.settingsFile.empty() ? "(none)" : renderer.settingsFile.string().c_str());
    }
} //namespace lockstep::debug
