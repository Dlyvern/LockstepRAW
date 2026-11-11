#include "IrradianceVolume.hpp"

#include "Lockstep/Core/Log.hpp"

#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/ImageData.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <span>
#include <string>

namespace lockstep::render
{
    namespace
    {
        constexpr uint32_t k_maxTextureSide{16384};
        constexpr float k_recomposeTolerance{0.01f};
        constexpr float k_minSunAgreement{0.97f};
        constexpr float k_fullSunAgreement{0.995f};
        constexpr uint32_t k_skyRings{8};
        constexpr uint32_t k_skySegments{16};
        constexpr float k_pi{3.14159265358979f};

        uint16_t toHalf(float value)
        {
            const uint32_t bits = std::bit_cast<uint32_t>(value);
            const uint32_t sign = (bits >> 16u) & 0x8000u;
            const int32_t exponent = static_cast<int32_t>((bits >> 23u) & 0xFFu) - 127 + 15;
            const uint32_t mantissa = bits & 0x7FFFFFu;

            if (exponent <= 0)
            {
                return static_cast<uint16_t>(sign);
            }

            if (exponent >= 31)
            {
                return static_cast<uint16_t>(sign | 0x7BFFu);
            }

            return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exponent) << 10u) | (mantissa >> 13u));
        }

        math::Vec3 vec3(const float (&values)[4])
        {
            return {values[0], values[1], values[2]};
        }

        math::Vec3 perez(const GpuFrameLighting& gpu, float cosTheta, float gamma, float cosGamma)
        {
            math::Vec3 result{};

            for (uint32_t channel = 0; channel < 3; ++channel)
            {
                const float a = gpu.skyA[channel];
                const float b = gpu.skyB[channel];
                const float c = gpu.skyC[channel];
                const float d = gpu.skyD[channel];
                const float e = gpu.skyE[channel];
                result[channel] = (1.0f + a * std::exp(b / std::max(cosTheta, 0.01f)))
                    * (1.0f + c * std::exp(d * gamma) + e * cosGamma * cosGamma);
            }

            return result;
        }

        math::Vec3 xyYToLinearRgb(const math::Vec3& yxy)
        {
            const float luminance = yxy.x;
            const float y = std::max(yxy.z, 1e-4f);
            const math::Vec3 xyz{yxy.y / y * luminance, luminance, (1.0f - yxy.y - y) / y * luminance};

            return {3.2406f * xyz.x - 1.5372f * xyz.y - 0.4986f * xyz.z,
                -0.9689f * xyz.x + 1.8758f * xyz.y + 0.0415f * xyz.z,
                0.0557f * xyz.x - 0.2040f * xyz.y + 1.0570f * xyz.z};
        }

        math::Vec3 skyRadiance(const GpuFrameLighting& gpu, const math::Vec3& direction)
        {
            const math::Vec3 toSun = vec3(gpu.sunDirection) * -1.0f;
            const float cosTheta = std::max(direction.y, 0.0f);
            const float cosGamma = std::clamp(math::dot(direction, toSun), -1.0f, 1.0f);
            const math::Vec3 yxy = vec3(gpu.skyZenith) * perez(gpu, cosTheta, std::acos(cosGamma), cosGamma);
            const math::Vec3 overheadTint = vec3(gpu.skyZenithTint);
            const float overheadBlend = std::clamp(cosTheta / 0.8f, 0.0f, 1.0f);
            const math::Vec3 overhead = math::Vec3{1.0f, 1.0f, 1.0f} * (1.0f - overheadBlend) + overheadTint * overheadBlend;
            const math::Vec3 rgb = xyYToLinearRgb(yxy);
            const math::Vec3 sky{std::max(rgb.x, 0.0f) * gpu.skyZenith[3] * overhead.x,
                std::max(rgb.y, 0.0f) * gpu.skyZenith[3] * overhead.y, std::max(rgb.z, 0.0f) * gpu.skyZenith[3] * overhead.z};
            const float band = std::exp(-std::max(direction.y, 0.0f) / std::max(gpu.skyGlow[3], 1e-3f));

            return sky + vec3(gpu.skyGlow) * (band * 0.5f);
        }

        math::Vec3 meanSkyRadiance(const GpuFrameLighting& gpu)
        {
            if (gpu.skyGround[3] < 0.5f || gpu.iblParams[3] < 0.5f)
            {
                return {0.0f, 0.0f, 0.0f};
            }

            math::Vec3 sum{0.0f, 0.0f, 0.0f};
            float weight = 0.0f;

            for (uint32_t ring = 0; ring < k_skyRings; ++ring)
            {
                const float elevation = (static_cast<float>(ring) + 0.5f) / static_cast<float>(k_skyRings) * k_pi * 0.5f;

                for (uint32_t segment = 0; segment < k_skySegments; ++segment)
                {
                    const float azimuth = static_cast<float>(segment) / static_cast<float>(k_skySegments) * 2.0f * k_pi;
                    const math::Vec3 direction{std::cos(elevation) * std::sin(azimuth), std::sin(elevation),
                        std::cos(elevation) * std::cos(azimuth)};
                    const float cosineArea = std::sin(elevation) * std::cos(elevation);
                    sum += skyRadiance(gpu, direction) * cosineArea;
                    weight += cosineArea;
                }
            }

            const math::Vec3 upper = sum * (1.0f / weight);
            const math::Vec3 ground = vec3(gpu.skyGround);

            return (upper + ground) * (0.5f * gpu.iblParams[0]);
        }

        bool isWellFormed(const IrradianceVolume& volume)
        {
            const auto axisFits = [](uint32_t count) { return count >= 2 && count <= k_maxIrradianceProbesPerAxis; };
            const uint64_t total = static_cast<uint64_t>(volume.countX) * volume.countY * volume.countZ;

            return axisFits(volume.countX) && axisFits(volume.countY) && axisFits(volume.countZ)
                && total <= k_maxIrradianceProbes && total == volume.probes.size()
                && volume.countY * volume.countZ <= k_maxTextureSide
                && volume.spacing.x > 0.0f && volume.spacing.y > 0.0f && volume.spacing.z > 0.0f;
        }

        std::vector<std::vector<uint16_t>> composeTexels(const IrradianceVolume& volume, const math::Vec3& sun,
            const math::Vec3& sky)
        {
            std::vector<std::vector<uint16_t>> texels(k_irradianceTextures,
                std::vector<uint16_t>(volume.probes.size() * 4));

            for (std::size_t index = 0; index < volume.probes.size(); ++index)
            {
                const IrradianceProbe& probe = volume.probes[index];

                for (uint32_t channel = 0; channel < 3; ++channel)
                {
                    for (uint32_t term = 0; term < 4; ++term)
                    {
                        const uint32_t at = channel * 4 + term;
                        const float value = probe.local[at] + probe.sun[at] * sun[channel] + probe.sky[at] * sky[channel];
                        texels[channel][index * 4 + term] = toHalf(value);
                    }
                }

                for (uint32_t term = 0; term < 4; ++term)
                {
                    texels[3][index * 4 + term] = toHalf(probe.skyVisibility[term]);
                }
            }

            return texels;
        }

        vela::graphics::ImageData imageOf(const IrradianceVolume& volume, const std::vector<uint16_t>& texels)
        {
            vela::graphics::ImageData image{};
            image.pixels = std::as_bytes(std::span(texels));
            image.width = volume.countX;
            image.height = volume.countY * volume.countZ;
            image.format = vela::graphics::TextureFormat::RGBA16Float;

            return image;
        }

        vela::graphics::SamplerDescription volumeSampler()
        {
            vela::graphics::SamplerDescription sampler{};
            sampler.addressU = vela::graphics::AddressMode::ClampToEdge;
            sampler.addressV = vela::graphics::AddressMode::ClampToEdge;
            sampler.addressW = vela::graphics::AddressMode::ClampToEdge;
            sampler.mipMode = vela::graphics::MipMode::Nearest;

            return sampler;
        }

        Status rebindLitMaterials(RendererState& state)
        {
            Status result{};

            state.materials.forEach([&state, &result](MaterialRecord& record)
            {
                if (record.shading != Shading::Lit || !result)
                {
                    return;
                }

                result = bindLightingTextures(state, record.material);
            });

            return result;
        }

        float sunAgreement(const IrradianceVolume& volume, const GpuFrameLighting& gpu)
        {
            if (gpu.sunDirection[3] < 0.5f || math::length(volume.sunTravel) < 0.5f)
            {
                return 0.0f;
            }

            const float agreement = math::dot(math::normalize(volume.sunTravel), vec3(gpu.sunDirection));
            const float t = std::clamp((agreement - k_minSunAgreement) / (k_fullSunAgreement - k_minSunAgreement),
                0.0f, 1.0f);

            return t * t * (3.0f - 2.0f * t);
        }

        bool differs(const math::Vec4& left, const math::Vec4& right)
        {
            const float scale = std::max({std::abs(left.x), std::abs(left.y), std::abs(left.z), std::abs(left.w),
                1e-3f});

            return std::abs(left.x - right.x) + std::abs(left.y - right.y) + std::abs(left.z - right.z)
                + std::abs(left.w - right.w) > scale * k_recomposeTolerance;
        }

        void compose(IrradianceVolumeState& irradiance, const math::Vec3& sun, const math::Vec3& sky,
            const math::Vec4& key)
        {
            const std::vector<std::vector<uint16_t>> texels = composeTexels(*irradiance.volume, sun, sky);

            for (uint32_t index = 0; index < k_irradianceTextures; ++index)
            {
                if (auto updated = irradiance.textures[index].update(imageOf(*irradiance.volume, texels[index]));
                    !updated)
                {
                    log(LogLevel::Error, "Render", "Irradiance volume upload failed: " + updated.error().message);
                    return;
                }
            }

            irradiance.composed = key;
            irradiance.isComposed = true;
        }
    } //namespace

    Status bindLightingTextures(const RendererState& state, vela::graphics::Material& material)
    {
        if (auto bound = material.setTexture(k_lightProfileBinding, state.environment.lightProfiles); !bound)
        {
            return toError(bound.error());
        }

        if (auto bound = material.setTexture(k_brdfLutBinding, state.environment.brdfLut); !bound)
        {
            return toError(bound.error());
        }

        for (uint32_t index = 0; index < k_irradianceTextures; ++index)
        {
            const vela::graphics::Texture& texture = state.irradiance.textures.size() == k_irradianceTextures
                ? state.irradiance.textures[index] : state.environment.blackFloat;

            if (auto bound = material.setTexture(k_irradianceBinding + index, texture); !bound)
            {
                return toError(bound.error());
            }
        }

        return {};
    }

    Status setIrradianceVolume(RendererState& state, const IrradianceVolume* volume)
    {
        IrradianceVolumeState& irradiance = state.irradiance;

        if (volume != nullptr && !isWellFormed(*volume))
        {
            return Error{ErrorCode::InvalidArgument, "setIrradianceVolume: malformed volume"};
        }

        irradiance.volume.reset();
        irradiance.isComposed = false;
        std::vector<vela::graphics::Texture> previous = std::move(irradiance.textures);
        irradiance.textures.clear();

        if (volume != nullptr)
        {
            const std::vector<std::vector<uint16_t>> texels = composeTexels(*volume, {}, {});

            for (uint32_t index = 0; index < k_irradianceTextures; ++index)
            {
                auto texture = vela::graphics::Texture::create(state.context, imageOf(*volume, texels[index]),
                    volumeSampler());

                if (!texture)
                {
                    irradiance.textures.clear();
                    static_cast<void>(rebindLitMaterials(state));
                    return toError(texture.error());
                }

                irradiance.textures.push_back(std::move(texture).value());
            }

            irradiance.volume = *volume;
        }

        return rebindLitMaterials(state);
    }

    void updateIrradianceVolume(RendererState& state)
    {
        GpuFrameLighting& gpu = state.prepared.lighting;
        IrradianceVolumeState& irradiance = state.irradiance;
        const IrradianceVolumeSettings& settings = state.settings.irradianceVolume;
        std::fill(std::begin(gpu.irradianceParams), std::end(gpu.irradianceParams), 0.0f);

        if (!irradiance.volume || !settings.enabled || irradiance.textures.size() != k_irradianceTextures)
        {
            return;
        }

        const IrradianceVolume& volume = *irradiance.volume;
        const math::Vec3 sun = vec3(gpu.sunRadiance) * sunAgreement(volume, gpu);
        const math::Vec3 sky = meanSkyRadiance(gpu);
        const float skyLevel = (sky.x + sky.y + sky.z) / 3.0f;
        const math::Vec4 key{sun.x, sun.y, sun.z, skyLevel};

        if (!irradiance.isComposed || differs(key, irradiance.composed))
        {
            compose(irradiance, sun, sky, key);
        }

        gpu.irradianceParams[0] = settings.debugView ? 2.0f : 1.0f;
        gpu.irradianceParams[1] = std::max(settings.intensity, 0.0f);
        gpu.irradianceParams[2] = std::max(settings.normalOffset, 0.0f);
        gpu.irradianceParams[3] = std::clamp(settings.fillKept, 0.0f, 1.0f);
        gpu.irradianceOrigin[0] = volume.origin.x;
        gpu.irradianceOrigin[1] = volume.origin.y;
        gpu.irradianceOrigin[2] = volume.origin.z;
        gpu.irradianceOrigin[3] = std::max(settings.fadeDistance, 0.01f);
        gpu.irradianceScale[0] = 1.0f / volume.spacing.x;
        gpu.irradianceScale[1] = 1.0f / volume.spacing.y;
        gpu.irradianceScale[2] = 1.0f / volume.spacing.z;
        gpu.irradianceScale[3] = static_cast<float>(volume.countZ);
        gpu.irradianceCounts[0] = static_cast<float>(volume.countX);
        gpu.irradianceCounts[1] = static_cast<float>(volume.countY);
        gpu.irradianceCounts[2] = static_cast<float>(volume.countZ);
        gpu.irradianceCounts[3] = std::clamp(settings.skyVisibility, 0.0f, 1.0f);
    }
} //namespace lockstep::render
