#include "EnvironmentResources.hpp"

#include "SurfaceTextures.hpp"
#include "VelaConversions.hpp"

#include <Vela/Core/Context.hpp>
#include <Vela/Graphics/ImageData.hpp>
#include <Vela/Graphics/Sampler.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <future>
#include <span>
#include <utility>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr uint32_t k_lutSize{64};
        constexpr uint32_t k_lutSamples{256};
        constexpr std::size_t k_lutChannels{4};
        constexpr float k_pi{3.14159265358979f};
        constexpr float k_detailAnisotropy{8.0f};

        float radicalInverse(uint32_t bits)
        {
            bits = (bits << 16u) | (bits >> 16u);
            bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
            bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
            bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
            bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

            return static_cast<float>(bits) * 2.3283064365386963e-10f;
        }

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

        float smithGgxCorrelatedVisibility(float nDotV, float nDotL, float alpha)
        {
            const float alpha2 = alpha * alpha;
            const float lambdaV = nDotL * std::sqrt(nDotV * nDotV * (1.0f - alpha2) + alpha2);
            const float lambdaL = nDotV * std::sqrt(nDotL * nDotL * (1.0f - alpha2) + alpha2);

            return 0.5f / (lambdaV + lambdaL);
        }

        std::array<float, 2> integrateBrdf(float nDotV, float roughness)
        {
            const float alpha = roughness * roughness;
            const float viewX = std::sqrt(1.0f - nDotV * nDotV);
            const float viewZ = nDotV;
            float scale = 0.0f;
            float bias = 0.0f;

            for (uint32_t index = 0; index < k_lutSamples; ++index)
            {
                const float u = static_cast<float>(index) / static_cast<float>(k_lutSamples);
                const float v = radicalInverse(index);
                const float phi = 2.0f * k_pi * v;
                const float cosTheta = std::sqrt((1.0f - u) / (1.0f + (alpha * alpha - 1.0f) * u));
                const float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);
                const float halfX = sinTheta * std::cos(phi);
                const float halfZ = cosTheta;
                const float vDotH = viewX * halfX + viewZ * halfZ;
                const float lightZ = 2.0f * vDotH * halfZ - viewZ;

                if (lightZ <= 0.0f || vDotH <= 0.0f)
                {
                    continue;
                }

                const float visibility = smithGgxCorrelatedVisibility(nDotV, lightZ, alpha);
                const float weight = 4.0f * visibility * lightZ * vDotH / halfZ;
                const float fresnel = std::pow(1.0f - vDotH, 5.0f);

                scale += (1.0f - fresnel) * weight;
                bias += fresnel * weight;
            }

            return {scale / static_cast<float>(k_lutSamples), bias / static_cast<float>(k_lutSamples)};
        }

        std::vector<uint16_t> buildBrdfLut()
        {
            std::vector<uint16_t> texels(static_cast<std::size_t>(k_lutSize) * k_lutSize * k_lutChannels);

            for (uint32_t y = 0; y < k_lutSize; ++y)
            {
                const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(k_lutSize);

                for (uint32_t x = 0; x < k_lutSize; ++x)
                {
                    const float nDotV = (static_cast<float>(x) + 0.5f) / static_cast<float>(k_lutSize);
                    const auto [scale, bias] = integrateBrdf(nDotV, roughness);
                    const std::size_t offset = (static_cast<std::size_t>(y) * k_lutSize + x) * k_lutChannels;

                    texels[offset] = toHalf(scale);
                    texels[offset + 1] = toHalf(bias);
                    texels[offset + 2] = toHalf(0.0f);
                    texels[offset + 3] = toHalf(1.0f);
                }
            }

            return texels;
        }

        vela::graphics::SamplerDescription clampedSampler()
        {
            vela::graphics::SamplerDescription sampler{};
            sampler.addressU = vela::graphics::AddressMode::ClampToEdge;
            sampler.addressV = vela::graphics::AddressMode::ClampToEdge;
            sampler.addressW = vela::graphics::AddressMode::ClampToEdge;

            return sampler;
        }

        Result<vela::graphics::Texture> createTexture(vela::core::Context& context, std::span<const std::byte> pixels,
            uint32_t size, vela::graphics::TextureFormat format)
        {
            vela::graphics::ImageData image{};
            image.pixels = pixels;
            image.width = size;
            image.height = size;
            image.format = format;

            auto texture = vela::graphics::Texture::create(context, image, clampedSampler());

            if (!texture)
            {
                return toError(texture.error());
            }

            return std::move(texture).value();
        }

        Result<vela::graphics::Texture> createMipmapped(vela::core::Context& context, std::span<const uint8_t> texels,
            uint32_t size, const vela::graphics::SamplerDescription& sampler)
        {
            vela::graphics::ImageData image{};
            image.pixels = std::as_bytes(texels);
            image.width = size;
            image.height = size;
            image.format = vela::graphics::TextureFormat::RGBA8Unorm;

            auto texture = vela::graphics::Texture::create(context, image, sampler,
                vela::graphics::TextureOptions{.generateMips = true});

            if (!texture)
            {
                return toError(texture.error());
            }

            return std::move(texture).value();
        }

        Result<std::vector<vela::graphics::Texture>> createSurfaceDetail(vela::core::Context& context,
            std::vector<std::future<std::vector<uint8_t>>>& jobs)
        {
            vela::graphics::SamplerDescription sampler{};
            sampler.anisotropy = k_detailAnisotropy;
            std::vector<vela::graphics::Texture> textures;

            for (std::future<std::vector<uint8_t>>& job : jobs)
            {
                const std::vector<uint8_t> texels = job.get();
                auto texture = createMipmapped(context, texels, k_surfaceDetailSize, sampler);

                if (!texture)
                {
                    return texture.error();
                }

                textures.push_back(std::move(texture).value());
            }

            return textures;
        }

        Result<vela::graphics::Texture> createPixel(vela::core::Context& context, std::array<uint8_t, 4> rgba,
            vela::graphics::TextureFormat format)
        {
            return createTexture(context, std::as_bytes(std::span(rgba)), 1, format);
        }
    } //namespace

    Result<EnvironmentResources> EnvironmentResources::create(vela::core::Context& context)
    {
        std::vector<std::future<std::vector<uint8_t>>> detailJobs;

        for (uint32_t kind = 1; kind <= k_surfaceDetailKinds; ++kind)
        {
            detailJobs.push_back(std::async(std::launch::async, surfaceDetailTexels, static_cast<SurfaceDetail>(kind)));
        }

        std::future<std::vector<uint8_t>> dirtJob = std::async(std::launch::async, lensDirtTexels);

        auto whiteSrgb = createPixel(context, {255, 255, 255, 255}, vela::graphics::TextureFormat::RGBA8Srgb);

        if (!whiteSrgb)
        {
            return whiteSrgb.error();
        }

        auto whiteLinear = createPixel(context, {255, 255, 255, 255}, vela::graphics::TextureFormat::RGBA8Unorm);

        if (!whiteLinear)
        {
            return whiteLinear.error();
        }

        auto flatNormal = createPixel(context, {128, 128, 255, 255}, vela::graphics::TextureFormat::RGBA8Unorm);

        if (!flatNormal)
        {
            return flatNormal.error();
        }

        const std::vector<uint16_t> lutTexels = buildBrdfLut();
        auto brdfLut = createTexture(context, std::as_bytes(std::span(lutTexels)), k_lutSize,
            vela::graphics::TextureFormat::RGBA16Float);

        if (!brdfLut)
        {
            return brdfLut.error();
        }

        auto surfaceDetail = createSurfaceDetail(context, detailJobs);

        if (!surfaceDetail)
        {
            return surfaceDetail.error();
        }

        const std::vector<uint8_t> dirtTexels = dirtJob.get();
        auto lensDirt = createMipmapped(context, dirtTexels, k_lensDirtSize, clampedSampler());

        if (!lensDirt)
        {
            return lensDirt.error();
        }

        return EnvironmentResources{
            .whiteSrgb = std::move(whiteSrgb).value(),
            .whiteLinear = std::move(whiteLinear).value(),
            .flatNormal = std::move(flatNormal).value(),
            .brdfLut = std::move(brdfLut).value(),
            .surfaceDetail = std::move(surfaceDetail).value(),
            .lensDirt = std::move(lensDirt).value()};
    }
} //namespace lockstep::render
