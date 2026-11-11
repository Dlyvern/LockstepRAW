#include "Atmosphere.hpp"

#include "CloudNoise.hpp"
#include "FrameAttachments.hpp"
#include "FrameLighting.hpp"
#include "FramePasses.hpp"
#include "RenderScale.hpp"
#include "RendererState.hpp"
#include "ScreenPass.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/ImageData.hpp>
#include <Vela/Graphics/RenderGraph.hpp>
#include <Vela/Graphics/VertexLayout.hpp>

#include <algorithm>
#include <cmath>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_fullscreenVertexShader{"fullscreen.vert.spv"};
        constexpr std::string_view k_cloudShader{"clouds.frag.spv"};
        constexpr std::string_view k_cloudCompositeShader{"clouds_composite.frag.spv"};
        constexpr std::string_view k_cloudAttachment{"clouds"};
        constexpr std::string_view k_cloudPassName{"clouds"};
        constexpr std::string_view k_cloudCompositePassName{"cloudComposite"};
        constexpr uint32_t k_lightingSlot{0};
        constexpr uint32_t k_minCloudSteps{8};
        constexpr uint32_t k_maxCloudSteps{64};
        constexpr float k_minCloudScale{0.25f};
        constexpr float k_maxCloudScale{1.0f};
        constexpr float k_baseCloudFrequency{1.0f / 9000.0f};
        constexpr float k_minCloudBase{200.0f};
        constexpr float k_minCloudThickness{100.0f};
        constexpr float k_goldenRatio{0.61803398875f};
        constexpr float k_cloudModeFlat{1.0f};
        constexpr float k_cloudModeVolumetric{2.0f};

        struct CloudConstants
        {
            math::Mat4 inverseViewProjection;
            float cameraLod[4];
        };

        static_assert(sizeof(CloudConstants) == 80);

        void store(float (&target)[4], const math::Vec3& xyz, float w)
        {
            target[0] = xyz.x;
            target[1] = xyz.y;
            target[2] = xyz.z;
            target[3] = w;
        }

        float luminance(const math::Vec3& color)
        {
            return 0.2126f * color.x + 0.7152f * color.y + 0.0722f * color.z;
        }

        math::Vec4 windBlock(const WindSettings& wind)
        {
            const float length = std::sqrt(wind.direction.x * wind.direction.x + wind.direction.y * wind.direction.y);

            if (!wind.enabled || length <= 0.0f)
            {
                return math::Vec4{1.0f, 0.0f, 0.0f, wind.time};
            }

            return math::Vec4{wind.direction.x / length, wind.direction.y / length,
                std::max(wind.speed, 0.0f) * std::max(wind.sway, 0.0f), wind.time};
        }

        float cloudMode(const CloudSettings& clouds, const SkySettings& sky)
        {
            if (!clouds.enabled || !sky.enabled)
            {
                return 0.0f;
            }

            return clouds.volumetric ? k_cloudModeVolumetric : k_cloudModeFlat;
        }

        bool isVolumetric(const RendererState& state)
        {
            return cloudMode(state.settings.clouds, state.settings.sky) == k_cloudModeVolumetric;
        }

        Result<vela::graphics::Material> cloudMaterial(RendererState& state, std::string_view shader,
            vela::graphics::BlendMode blend, bool bindsLighting)
        {
            vela::graphics::RenderState renderState{};
            renderState.depthTest = false;
            renderState.depthWrite = false;
            renderState.blend = blend;

            auto material = createBuiltinMaterial(state.context, k_fullscreenVertexShader, shader,
                vela::graphics::VertexLayout{}, renderState);

            if (!material || !bindsLighting)
            {
                return material;
            }

            if (auto bound = material.value().setStorageBuffer(k_lightingSlot, state.lightBuffer); !bound)
            {
                return toError(bound.error());
            }

            if (auto bound = material.value().setTexture(k_cloudNoiseSlot, *state.atmosphere.cloudNoise); !bound)
            {
                return toError(bound.error());
            }

            return material;
        }

        CloudConstants cloudConstants(const RendererState& state)
        {
            const math::Vec3& camera = state.camera.position;
            const float height = static_cast<float>(std::max(state.frameHeight, 1u)) * state.atmosphere.cloudScale;
            const float pixelAngle = 2.0f * std::tan(state.camera.verticalFov * 0.5f) / height;
            const float frequency = k_baseCloudFrequency / std::max(state.settings.clouds.scale, 0.05f);

            CloudConstants constants{.inverseViewProjection = math::inverse(state.projection * state.view),
                .cameraLod = {}};
            store(constants.cameraLod, camera, pixelAngle * frequency * static_cast<float>(k_cloudNoiseSize));
            return constants;
        }

        Status addCloudMarchPass(vela::graphics::RenderGraph& graph, RendererState& state)
        {
            auto material = cloudMaterial(state, k_cloudShader, vela::graphics::BlendMode::Opaque, true);

            if (!material)
            {
                return material.error();
            }

            std::vector<vela::graphics::PassInput> inputs{
                {std::string(k_sceneDepthAttachment), vela::graphics::InputUsage::Sampled}};
            std::vector<vela::graphics::AttachmentSlot> outputs{clearedScreenSlot(k_cloudAttachment, k_sceneColorFormat,
                state.atmosphere.cloudScale, vela::graphics::ClearValue{0.0f, 0.0f, 0.0f, 1.0f})};

            auto pass = std::make_unique<ScreenPass>(std::move(outputs), std::move(inputs), std::move(material).value(),
                &state.atmosphere.cloudScale, [&state](vela::graphics::PassRecorder& recorder)
                {
                    if (!isVolumetric(state))
                    {
                        return false;
                    }

                    recorder.setConstants(cloudConstants(state));
                    return true;
                });

            return addScaledPass(graph, state, k_cloudPassName, std::move(pass));
        }

        Status addCloudCompositePass(vela::graphics::RenderGraph& graph, RendererState& state)
        {
            auto material = cloudMaterial(state, k_cloudCompositeShader, vela::graphics::BlendMode::Alpha, false);

            if (!material)
            {
                return material.error();
            }

            std::vector<vela::graphics::PassInput> inputs{
                {std::string(k_cloudAttachment), vela::graphics::InputUsage::Sampled},
                {std::string(k_sceneDepthAttachment), vela::graphics::InputUsage::Sampled}};
            std::vector<vela::graphics::AttachmentSlot> outputs{loadedScreenSlot(k_sceneColorAttachment,
                k_sceneColorFormat)};

            auto pass = std::make_unique<ScreenPass>(std::move(outputs), std::move(inputs), std::move(material).value(),
                nullptr, [&state](vela::graphics::PassRecorder&) { return isVolumetric(state); });

            return addScaledPass(graph, state, k_cloudCompositePassName, std::move(pass));
        }
    } //namespace

    Status createAtmosphereResources(RendererState& state)
    {
        if (state.atmosphere.cloudNoise)
        {
            return {};
        }

        const std::vector<uint8_t> texels = cloudNoiseTexels();
        vela::graphics::ImageData image{};
        image.pixels = std::as_bytes(std::span(texels));
        image.width = k_cloudNoiseSize;
        image.height = k_cloudNoiseSize;
        image.format = vela::graphics::TextureFormat::RGBA8Unorm;

        auto texture = vela::graphics::Texture::create(state.context, image, vela::graphics::SamplerDescription{},
            vela::graphics::TextureOptions{.generateMips = true});

        if (!texture)
        {
            return toError(texture.error());
        }

        state.atmosphere.cloudNoise.emplace(std::move(texture).value());
        return {};
    }

    std::optional<DirectionalLight> lightningSun(const std::optional<DirectionalLight>& sun,
        const RenderSettings& settings)
    {
        const LightningSettings& lightning = settings.lightning;

        if (lightning.flash <= 0.0f || !settings.lighting.directional)
        {
            return sun;
        }

        const math::Vec3 radiance = lightning.color * (std::max(lightning.lightIntensity, 0.0f) * lightning.flash);

        if (sun && luminance(sun->color * sun->intensity) >= luminance(radiance))
        {
            return sun;
        }

        const float length = math::length(lightning.direction);
        const math::Vec3 towards = length > 0.0f ? lightning.direction * (1.0f / length) : math::Vec3{0.0f, 1.0f, 0.0f};

        return DirectionalLight{.direction = towards * -1.0f,
            .color = radiance,
            .intensity = 1.0f,
            .castsShadows = lightning.castsShadows};
    }

    void packAtmosphere(GpuFrameLighting& gpu, const RenderSettings& settings, AtmosphereState& atmosphere,
        uint32_t frameIndex)
    {
        const math::Vec4 wind = windBlock(settings.wind);
        const math::Vec4 previous = atmosphere.hasPreviousWind ? atmosphere.previousWind : wind;
        store(gpu.wind, math::Vec3{wind.x, wind.y, wind.z}, wind.w);
        store(gpu.windPrevious, math::Vec3{previous.x, previous.y, previous.z}, previous.w);
        atmosphere.previousWind = wind;
        atmosphere.hasPreviousWind = true;

        const CloudSettings& clouds = settings.clouds;
        const float frequency = k_baseCloudFrequency / std::max(clouds.scale, 0.05f);
        store(gpu.cloudParams, math::Vec3{std::clamp(clouds.coverage, 0.0f, 1.0f), std::max(clouds.density, 0.0f),
            std::max(clouds.baseHeight, k_minCloudBase)}, std::max(clouds.thickness, k_minCloudThickness));
        store(gpu.cloudShape, math::Vec3{clouds.offset.x, clouds.offset.y, frequency},
            cloudMode(clouds, settings.sky));
        store(gpu.cloudLight, clouds.ambient, std::clamp(clouds.silverLining, 0.0f, 0.9f));
        const float jitter = std::fmod(static_cast<float>(frameIndex % 1024u) * k_goldenRatio, 1.0f);
        store(gpu.cloudMarch, math::Vec3{std::max(clouds.sunLight, 0.0f), std::clamp(clouds.iblInfluence, 0.0f, 1.0f),
            static_cast<float>(std::clamp(clouds.steps, k_minCloudSteps, k_maxCloudSteps))}, jitter);

        const LightningSettings& lightning = settings.lightning;
        const float flash = settings.sky.enabled ? std::max(lightning.flash, 0.0f) : 0.0f;
        const float length = math::length(lightning.direction);
        const math::Vec3 towards = length > 0.0f ? lightning.direction * (1.0f / length) : math::Vec3{0.0f, 1.0f, 0.0f};
        store(gpu.lightningFlash, lightning.color * (std::max(lightning.skyIntensity, 0.0f) * flash), flash);
        store(gpu.lightningDirection, towards, settings.sky.enabled ? std::clamp(lightning.bolt, 0.0f, 1.0f) : 0.0f);
        store(gpu.lightningBolt, math::Vec3{lightning.boltSeed, 0.0f, 0.0f}, 0.0f);
    }

    Status addCloudPasses(vela::graphics::RenderGraph& graph, RendererState& state)
    {
        state.atmosphere.cloudScale = std::clamp(state.settings.clouds.resolutionScale, k_minCloudScale,
            k_maxCloudScale);

        if (auto added = addCloudMarchPass(graph, state); !added)
        {
            return added;
        }

        return addCloudCompositePass(graph, state);
    }

    Status updateCloudScale(RendererState& state)
    {
        const float requested = std::clamp(state.settings.clouds.resolutionScale, k_minCloudScale, k_maxCloudScale);

        if (requested == state.atmosphere.cloudScale)
        {
            return {};
        }

        const float previous = state.atmosphere.cloudScale;
        state.atmosphere.cloudScale = requested;

        if (auto redescribed = state.graph->redescribePass(std::string(k_cloudPassName)); !redescribed)
        {
            state.atmosphere.cloudScale = previous;
            return toError(redescribed.error());
        }

        return {};
    }
} //namespace lockstep::render
