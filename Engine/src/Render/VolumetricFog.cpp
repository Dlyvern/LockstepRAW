#include "VolumetricFog.hpp"

#include "FrameAttachments.hpp"
#include "FramePasses.hpp"
#include "RenderScale.hpp"
#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/RenderGraph.hpp>
#include <Vela/Graphics/VertexLayout.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_fullscreenVertexShader{"fullscreen.vert.spv"};
        constexpr std::string_view k_scatterShader{"volumetric_fog.frag.spv"};
        constexpr std::string_view k_temporalShader{"volumetric_fog_temporal.frag.spv"};
        constexpr std::string_view k_compositeShader{"volumetric_fog_composite.frag.spv"};
        constexpr std::string_view k_scatterPassName{"fogScatter"};
        constexpr std::string_view k_temporalPassName{"fogResolve"};
        constexpr std::string_view k_compositePassName{"fogComposite"};
        constexpr uint32_t k_lightingSlot{0};
        constexpr uint32_t k_shadowAtlasSlot{1};
        constexpr uint32_t k_fullscreenTriangleVertices{3};
        constexpr uint32_t k_minSteps{8};
        constexpr uint32_t k_maxSteps{64};
        constexpr float k_minScale{0.25f};
        constexpr float k_maxScale{1.0f};
        constexpr float k_maxAnisotropy{0.9f};
        constexpr float k_maxHistoryWeight{0.98f};
        constexpr float k_interiorBlendDistance{2.0f};
        constexpr float k_goldenRatio{0.61803398875f};

        struct ScatterConstants
        {
            math::Mat4 inverseViewProjection;
            float cameraDensity[4];
            float heightDistance[4];
            float albedoAnisotropy[4];
            float intensities[4];
        };

        struct TemporalConstants
        {
            math::Mat4 reprojection;
            float params[4];
        };

        struct CompositeConstants
        {
            float depthParams[4];
        };

        static_assert(sizeof(ScatterConstants) == 128);
        static_assert(sizeof(TemporalConstants) == 80);
        static_assert(sizeof(CompositeConstants) == 16);

        class FogPass final : public vela::graphics::Pass
        {
        public:
            using Recorder = std::function<bool(vela::graphics::PassRecorder& recorder)>;

            FogPass(std::vector<vela::graphics::AttachmentSlot> outputs, std::vector<vela::graphics::PassInput> inputs,
                vela::graphics::Material&& material, const float* scale, Recorder recorder)
                : m_outputs(std::move(outputs)),
                  m_inputs(std::move(inputs)),
                  m_material(std::move(material)),
                  m_scale(scale),
                  m_recorder(std::move(recorder))
            {
            }

            [[nodiscard]] vela::graphics::PassDescription describe() const override
            {
                vela::graphics::PassDescription description{};
                description.colorOutputs = m_outputs;
                description.inputs = m_inputs;

                if (m_scale != nullptr)
                {
                    for (vela::graphics::AttachmentSlot& slot : description.colorOutputs)
                    {
                        slot.scale = *m_scale;
                    }
                }

                return description;
            }

            void record(vela::graphics::PassRecorder& recorder) override
            {
                recorder.bind(m_material);

                for (uint32_t slot = 0; slot < m_inputs.size(); ++slot)
                {
                    recorder.bindAttachment(slot, m_inputs[slot].name);
                }

                if (m_recorder(recorder))
                {
                    recorder.drawInstanced(k_fullscreenTriangleVertices, 1);
                }
            }

        private:
            std::vector<vela::graphics::AttachmentSlot> m_outputs;
            std::vector<vela::graphics::PassInput> m_inputs;
            vela::graphics::Material m_material;
            const float* m_scale{nullptr};
            Recorder m_recorder;
        };

        vela::graphics::AttachmentSlot fogSlot(std::string_view name, bool persistent)
        {
            vela::graphics::AttachmentSlot slot{};
            slot.name = std::string(name);
            slot.format = k_sceneColorFormat;
            slot.load = vela::graphics::LoadOp::Clear;
            slot.store = vela::graphics::StoreOp::Store;
            slot.clear = {0.0f, 0.0f, 0.0f, 1.0f};
            slot.persistent = persistent;

            return slot;
        }

        vela::graphics::AttachmentSlot loadedSlot(std::string_view name)
        {
            vela::graphics::AttachmentSlot slot{};
            slot.name = std::string(name);
            slot.format = k_sceneColorFormat;
            slot.load = vela::graphics::LoadOp::Load;
            slot.store = vela::graphics::StoreOp::Store;

            return slot;
        }

        vela::graphics::RenderState fullscreenState(vela::graphics::BlendMode blend)
        {
            vela::graphics::RenderState renderState{};
            renderState.depthTest = false;
            renderState.depthWrite = false;
            renderState.blend = blend;

            return renderState;
        }

        Result<vela::graphics::Material> fogMaterial(RendererState& state, std::string_view fragmentShader,
            vela::graphics::BlendMode blend)
        {
            return createBuiltinMaterial(state.context, k_fullscreenVertexShader, fragmentShader,
                vela::graphics::VertexLayout{}, fullscreenState(blend));
        }

        float smoothstep(float edge0, float edge1, float value)
        {
            const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        float cameraInteriorWeight(const RendererState& state)
        {
            if (!state.settings.interiorAmbient.enabled)
            {
                return 0.0f;
            }

            const math::Vec3& position = state.camera.position;
            float weight = 0.0f;

            for (const AmbientZone& zone : state.ambientZones)
            {
                const math::Vec3 centre = (zone.bounds.min + zone.bounds.max) * 0.5f;
                const math::Vec3 halfSize = (zone.bounds.max - zone.bounds.min) * 0.5f;
                const float outside = std::max({std::abs(position.x - centre.x) - halfSize.x,
                    std::abs(position.y - centre.y) - halfSize.y, std::abs(position.z - centre.z) - halfSize.z});
                weight = std::max(weight, smoothstep(0.0f, k_interiorBlendDistance, -outside));
            }

            return weight;
        }

        bool isFogOn(const RendererState& state)
        {
            const VolumetricFogSettings& fog = state.settings.volumetricFog;
            return fog.enabled && fog.density > 0.0f && fog.maxDistance > 0.0f;
        }

        ScatterConstants scatterConstants(const RendererState& state)
        {
            const VolumetricFogSettings& fog = state.settings.volumetricFog;
            const math::Vec3& camera = state.camera.position;
            const float density = volumetricFogCameraDensity(state);
            const uint32_t steps = std::clamp(fog.steps, k_minSteps, k_maxSteps);
            const float noise = fog.temporal
                ? std::fmod(static_cast<float>(state.frameIndex % 1024u) * k_goldenRatio, 1.0f) : 0.0f;

            return ScatterConstants{
                .inverseViewProjection = math::inverse(state.projection * state.view),
                .cameraDensity = {camera.x, camera.y, camera.z, density},
                .heightDistance = {std::max(fog.heightFalloff, 0.0f), fog.baseHeight, fog.maxDistance,
                    static_cast<float>(steps)},
                .albedoAnisotropy = {std::clamp(fog.albedo.x, 0.0f, 1.0f), std::clamp(fog.albedo.y, 0.0f, 1.0f),
                    std::clamp(fog.albedo.z, 0.0f, 1.0f), std::clamp(fog.anisotropy, -k_maxAnisotropy, k_maxAnisotropy)},
                .intensities = {state.settings.lighting.directional ? std::max(fog.sunIntensity, 0.0f) : 0.0f,
                    state.settings.lighting.localLights ? std::max(fog.localLightIntensity, 0.0f) : 0.0f,
                    std::max(fog.ambientIntensity, 0.0f), noise}};
        }

        TemporalConstants temporalConstants(const RendererState& state)
        {
            const VolumetricFogSettings& fog = state.settings.volumetricFog;
            const math::Mat4 reprojection = state.temporal.previousViewProjection
                * math::inverse(state.projection * state.view);
            const float weight = fog.temporal && state.temporal.hasPrevious
                ? std::clamp(fog.historyWeight, 0.0f, k_maxHistoryWeight) : 0.0f;

            return TemporalConstants{.reprojection = reprojection, .params = {weight, 0.0f, 0.0f, 0.0f}};
        }

        Status addFogPass(vela::graphics::RenderGraph& graph, RendererState& state, std::string_view name,
            std::unique_ptr<FogPass> pass)
        {
            return addScaledPass(graph, state, name, std::move(pass));
        }

        Status addScatterPass(vela::graphics::RenderGraph& graph, RendererState& state)
        {
            auto material = fogMaterial(state, k_scatterShader, vela::graphics::BlendMode::Opaque);

            if (!material)
            {
                return material.error();
            }

            if (auto bound = material.value().setStorageBuffer(k_lightingSlot, state.lightBuffer); !bound)
            {
                return toError(bound.error());
            }

            std::vector<vela::graphics::PassInput> inputs{
                {std::string(k_sceneDepthAttachment), vela::graphics::InputUsage::Sampled},
                {std::string(k_shadowMapAttachment), vela::graphics::InputUsage::Sampled}};

            auto pass = std::make_unique<FogPass>(
                std::vector<vela::graphics::AttachmentSlot>{fogSlot(k_fogScatterAttachment, false)}, std::move(inputs),
                std::move(material).value(), &state.volumetricFogScale,
                [&state](vela::graphics::PassRecorder& recorder)
                {
                    if (!isFogOn(state))
                    {
                        return false;
                    }

                    recorder.bindAttachment(k_shadowAtlasSlot, std::string(k_shadowMapAttachment),
                        shadowCompareSampler());
                    recorder.setConstants(scatterConstants(state));
                    return true;
                });

            return addFogPass(graph, state, k_scatterPassName, std::move(pass));
        }

        Status addFogResolvePass(vela::graphics::RenderGraph& graph, RendererState& state)
        {
            auto material = fogMaterial(state, k_temporalShader, vela::graphics::BlendMode::Opaque);

            if (!material)
            {
                return material.error();
            }

            std::vector<vela::graphics::PassInput> inputs{
                {std::string(k_fogScatterAttachment), vela::graphics::InputUsage::Sampled},
                {std::string(k_sceneDepthAttachment), vela::graphics::InputUsage::Sampled},
                {std::string(k_fogResolvedAttachment), vela::graphics::InputUsage::History}};

            auto pass = std::make_unique<FogPass>(
                std::vector<vela::graphics::AttachmentSlot>{fogSlot(k_fogResolvedAttachment, true)}, std::move(inputs),
                std::move(material).value(), &state.volumetricFogScale,
                [&state](vela::graphics::PassRecorder& recorder)
                {
                    if (!isFogOn(state))
                    {
                        return false;
                    }

                    recorder.setConstants(temporalConstants(state));
                    return true;
                });

            return addFogPass(graph, state, k_temporalPassName, std::move(pass));
        }

        Status addCompositePass(vela::graphics::RenderGraph& graph, RendererState& state)
        {
            auto material = fogMaterial(state, k_compositeShader, vela::graphics::BlendMode::Alpha);

            if (!material)
            {
                return material.error();
            }

            std::vector<vela::graphics::PassInput> inputs{
                {std::string(k_fogResolvedAttachment), vela::graphics::InputUsage::Sampled},
                {std::string(k_sceneDepthAttachment), vela::graphics::InputUsage::Sampled}};

            auto pass = std::make_unique<FogPass>(
                std::vector<vela::graphics::AttachmentSlot>{loadedSlot(k_sceneColorAttachment),
                    loadedSlot(k_sceneAmbientAttachment)},
                std::move(inputs), std::move(material).value(), nullptr,
                [&state](vela::graphics::PassRecorder& recorder)
                {
                    if (!isFogOn(state))
                    {
                        return false;
                    }

                    const math::Mat4& projection = state.projection;
                    recorder.setConstants(CompositeConstants{
                        .depthParams = {projection[2][2], projection[3][2], 0.0f, 0.0f}});
                    return true;
                });

            return addFogPass(graph, state, k_compositePassName, std::move(pass));
        }
    } //namespace

    Status addVolumetricFogPasses(vela::graphics::RenderGraph& graph, RendererState& state)
    {
        state.volumetricFogScale = std::clamp(state.settings.volumetricFog.resolutionScale, k_minScale, k_maxScale);

        if (auto added = addScatterPass(graph, state); !added)
        {
            return added;
        }

        if (auto added = addFogResolvePass(graph, state); !added)
        {
            return added;
        }

        return addCompositePass(graph, state);
    }

    float volumetricFogCameraDensity(const RendererState& state)
    {
        const VolumetricFogSettings& fog = state.settings.volumetricFog;

        if (!isFogOn(state))
        {
            return 0.0f;
        }

        const float interior = cameraInteriorWeight(state);

        return fog.density * (1.0f + (std::max(fog.interiorDensityScale, 0.0f) - 1.0f) * interior);
    }

    Status updateVolumetricFogScale(RendererState& state)
    {
        float& requested = state.settings.volumetricFog.resolutionScale;
        requested = std::clamp(requested, k_minScale, k_maxScale);

        if (requested == state.volumetricFogScale)
        {
            return {};
        }

        const float previous = state.volumetricFogScale;
        state.volumetricFogScale = requested;

        for (const std::string_view name : {k_scatterPassName, k_temporalPassName})
        {
            if (auto redescribed = state.graph->redescribePass(std::string(name)); !redescribed)
            {
                state.volumetricFogScale = previous;
                requested = previous;
                return toError(redescribed.error());
            }
        }

        return {};
    }
} //namespace lockstep::render
