#include "Weather/RainLook.hpp"

#include <Lockstep/Core/Math.hpp>

#include <algorithm>

namespace game
{
    namespace
    {
        namespace math = lockstep::math;
        namespace render = lockstep::render;

        constexpr float k_rainSunScale{0.3f};
        constexpr float k_rainSkyScale{0.55f};
        constexpr float k_rainTurbidity{9.0f};
        constexpr float k_rainGlowScale{0.35f};
        constexpr math::Vec3 k_rainZenithTint{0.75f, 0.8f, 0.9f};
        constexpr math::Vec3 k_rainSunColor{0.85f, 0.62f, 0.45f};
        constexpr float k_rainDiffuseScale{0.85f};
        constexpr float k_rainReflectionScale{0.9f};
        constexpr math::Vec3 k_rainFogColor{0.3f, 0.31f, 0.34f};
        constexpr float k_rainFogDensity{0.012f};
        constexpr float k_rainFogStart{6.0f};
        constexpr float k_rainFogMaxAmount{0.85f};
        constexpr float k_rainFogSkyBlend{0.5f};
        constexpr float k_rainHazeScale{1.8f};
        constexpr math::Vec3 k_rainHazeAlbedo{0.82f, 0.85f, 0.9f};
        constexpr float k_rainHazeAmbient{0.012f};
        constexpr float k_rainHazeAnisotropy{0.35f};
        constexpr float k_rainSaturationScale{0.85f};
        constexpr float k_rainContrastScale{0.95f};
        constexpr float k_hideSunDiskAbove{0.5f};

        float mixed(float from, float to, float amount)
        {
            return from + (to - from) * amount;
        }

        math::Vec3 mixed(const math::Vec3& from, const math::Vec3& to, float amount)
        {
            return math::lerp(from, to, amount);
        }

        void blendSky(render::RenderSettings& settings, const RainLookBase& base, float amount)
        {
            render::SkySettings& sky = settings.sky;
            sky = base.sky;
            sky.sunIntensity = base.sky.sunIntensity * mixed(1.0f, k_rainSunScale, amount);
            sky.sunColor = mixed(base.sky.sunColor, k_rainSunColor, amount * 0.5f);
            sky.skyIntensity = base.sky.skyIntensity * mixed(1.0f, k_rainSkyScale, amount);
            sky.turbidity = mixed(base.sky.turbidity, std::max(base.sky.turbidity, k_rainTurbidity), amount);
            sky.glowIntensity = base.sky.glowIntensity * mixed(1.0f, k_rainGlowScale, amount);
            sky.zenithTint = mixed(base.sky.zenithTint, k_rainZenithTint, amount);
            sky.sunDisk = base.sky.sunDisk && amount < k_hideSunDiskAbove;

            settings.ibl = base.ibl;
            settings.ibl.diffuseIntensity = base.ibl.diffuseIntensity * mixed(1.0f, k_rainDiffuseScale, amount);
            settings.ibl.specularIntensity = base.ibl.specularIntensity * mixed(1.0f, k_rainReflectionScale, amount);
        }

        void blendFog(render::RenderSettings& settings, const RainLookBase& base, float amount)
        {
            render::FogSettings& fog = settings.fog;
            fog = base.fog;
            const float baseDensity = base.fog.enabled ? base.fog.density : 0.0f;
            fog.enabled = base.fog.enabled || amount > 0.0f;
            fog.color = base.fog.enabled ? mixed(base.fog.color, k_rainFogColor, amount) : k_rainFogColor;
            fog.density = mixed(baseDensity, std::max(baseDensity, k_rainFogDensity), amount);
            fog.start = base.fog.enabled ? mixed(base.fog.start, k_rainFogStart, amount) : k_rainFogStart;
            fog.maxAmount = base.fog.enabled ? mixed(base.fog.maxAmount, k_rainFogMaxAmount, amount) : k_rainFogMaxAmount;
            fog.skyBlend = base.fog.enabled ? mixed(base.fog.skyBlend, k_rainFogSkyBlend, amount) : k_rainFogSkyBlend;

            render::VolumetricFogSettings& haze = settings.volumetricFog;
            haze = base.volumetricFog;
            haze.density = base.volumetricFog.density * mixed(1.0f, k_rainHazeScale, amount);
            haze.albedo = mixed(base.volumetricFog.albedo, k_rainHazeAlbedo, amount);
            haze.anisotropy = mixed(base.volumetricFog.anisotropy, k_rainHazeAnisotropy, amount);
            haze.ambientIntensity = mixed(base.volumetricFog.ambientIntensity,
                std::max(base.volumetricFog.ambientIntensity, k_rainHazeAmbient), amount);
        }
    } //namespace

    RainLookBase captureRainLookBase(const render::RenderSettings& settings)
    {
        return RainLookBase{.sky = settings.sky,
            .ibl = settings.ibl,
            .fog = settings.fog,
            .volumetricFog = settings.volumetricFog,
            .colorGrading = settings.colorGrading};
    }

    void applyRainLook(render::RenderSettings& settings, const RainLookBase& base, float amount)
    {
        const float clamped = std::clamp(amount, 0.0f, 1.0f);
        blendSky(settings, base, clamped);
        blendFog(settings, base, clamped);
        settings.colorGrading = base.colorGrading;
        settings.colorGrading.saturation = base.colorGrading.saturation * mixed(1.0f, k_rainSaturationScale, clamped);
        settings.colorGrading.contrast = base.colorGrading.contrast * mixed(1.0f, k_rainContrastScale, clamped);
    }
} //namespace game
