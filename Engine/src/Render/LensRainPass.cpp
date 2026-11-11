#include "Atmosphere.hpp"

#include "Lockstep/Core/Log.hpp"

#include "FrameAttachments.hpp"
#include "FramePasses.hpp"
#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/DynamicBuffer.hpp>
#include <Vela/Graphics/RenderGraph.hpp>
#include <Vela/Graphics/VertexLayout.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_vertexShader{"lens_rain.vert.spv"};
        constexpr std::string_view k_fragmentShader{"lens_rain.frag.spv"};
        constexpr std::string_view k_passName{"lensRain"};
        constexpr uint32_t k_verticesPerDrop{6};
        constexpr uint32_t k_sourceSlot{0};
        constexpr uint32_t k_temporalSlot{1};

        struct GpuLensDroplet
        {
            float shape[4];
            float params[4];
        };

        struct LensRainConstants
        {
            float params[4];
            float fade[4];
            float options[4];
        };

        static_assert(sizeof(GpuLensDroplet) == 32);
        static_assert(sizeof(LensRainConstants) == 48);

        vela::graphics::VertexLayout dropletLayout()
        {
            using vela::graphics::VertexAttributeFormat;

            return vela::graphics::VertexLayout{
                .stride = sizeof(GpuLensDroplet),
                .attributes = {
                    {.location = 0, .format = VertexAttributeFormat::Float4, .offset = offsetof(GpuLensDroplet, shape)},
                    {.location = 1, .format = VertexAttributeFormat::Float4, .offset = offsetof(GpuLensDroplet, params)}},
                .inputRate = vela::graphics::VertexInputRate::Instance};
        }

        class LensRainPass final : public vela::graphics::Pass
        {
        public:
            LensRainPass(const RendererState& state, vela::graphics::Material&& material,
                vela::graphics::DynamicBuffer&& instances)
                : m_state(&state),
                  m_material(std::move(material)),
                  m_instances(std::move(instances))
            {
            }

            [[nodiscard]] vela::graphics::PassDescription describe() const override
            {
                vela::graphics::AttachmentSlot display{};
                display.name = std::string(k_displayAttachment);
                display.format = k_displayFormat;
                display.load = vela::graphics::LoadOp::Load;
                display.store = vela::graphics::StoreOp::Store;

                vela::graphics::PassDescription description{};
                description.colorOutputs.push_back(display);
                description.inputs.push_back({std::string(k_ldrAttachment), vela::graphics::InputUsage::Sampled});
                description.inputs.push_back({std::string(k_taaHistoryAttachment), vela::graphics::InputUsage::Sampled});

                return description;
            }

            void record(vela::graphics::PassRecorder& recorder) override
            {
                const RendererState& state = *m_state;
                const LensRainSettings& settings = state.settings.lensRain;

                if (!settings.enabled || state.atmosphere.lensDropletFrame != state.frameIndex
                    || state.atmosphere.lensDroplets.empty())
                {
                    return;
                }

                m_upload.clear();

                for (const LensDroplet& drop : state.atmosphere.lensDroplets)
                {
                    if (drop.opacity <= 0.0f || drop.radius <= 0.0f)
                    {
                        continue;
                    }

                    m_upload.push_back(GpuLensDroplet{
                        .shape = {drop.position.x, drop.position.y, drop.radius, std::max(drop.stretch, 1.0f)},
                        .params = {std::clamp(drop.opacity, 0.0f, 1.0f), 0.0f, 0.0f, 0.0f}});
                }

                if (m_upload.empty())
                {
                    return;
                }

                if (auto uploaded = m_instances.update(std::as_bytes(std::span{m_upload})); !uploaded)
                {
                    log(LogLevel::Error, "Render", "Lens rain upload failed: " + toError(uploaded.error()).message);
                    return;
                }

                const vela::graphics::Extent2D extent = recorder.extent();
                const float aspectRatio = extent.height > 0
                    ? static_cast<float>(extent.width) / static_cast<float>(extent.height) : 1.0f;
                const math::Vec4& fade = state.screenFade;
                const bool readsTemporal = state.settings.antiAliasing.mode == AntiAliasingMode::Taa;

                recorder.bind(m_material);
                recorder.bindVertexBuffer(m_instances);
                recorder.bindAttachment(k_sourceSlot, std::string(k_ldrAttachment));
                recorder.bindAttachment(k_temporalSlot, std::string(k_taaHistoryAttachment));
                recorder.setConstants(LensRainConstants{
                    .params = {aspectRatio, std::clamp(settings.refraction, 0.0f, 2.0f),
                        std::clamp(settings.blur, 0.0f, 1.0f), std::clamp(settings.opacity, 0.0f, 1.0f)},
                    .fade = {fade.x, fade.y, fade.z, fade.w},
                    .options = {readsTemporal ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f}});
                recorder.drawInstanced(k_verticesPerDrop, static_cast<uint32_t>(m_upload.size()));
            }

        private:
            const RendererState* m_state{nullptr};
            vela::graphics::Material m_material;
            vela::graphics::DynamicBuffer m_instances;
            std::vector<GpuLensDroplet> m_upload;
        };
    } //namespace

    Status addLensRainPass(vela::graphics::RenderGraph& graph, RendererState& state)
    {
        vela::graphics::RenderState renderState{};
        renderState.depthTest = false;
        renderState.depthWrite = false;
        renderState.blend = vela::graphics::BlendMode::Alpha;

        auto material = createBuiltinMaterial(state.context, k_vertexShader, k_fragmentShader, dropletLayout(),
            renderState);

        if (!material)
        {
            return material.error();
        }

        auto instances = vela::graphics::DynamicBuffer::create(state.context, vela::graphics::DynamicBuffer::Usage::Vertex,
            sizeof(GpuLensDroplet) * k_maxLensDroplets);

        if (!instances)
        {
            return toError(instances.error());
        }

        auto pass = std::make_unique<LensRainPass>(state, std::move(material).value(), std::move(instances).value());

        if (auto added = graph.addPass(std::string(k_passName), std::move(pass)); !added)
        {
            return toError(added.error());
        }

        return {};
    }
} //namespace lockstep::render
