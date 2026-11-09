#include "TemporalAntiAliasing.hpp"

#include "FrameAttachments.hpp"
#include "PostPasses.hpp"
#include "RendererState.hpp"

#include <Vela/Graphics/RenderGraph.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_temporalShader{"taa.frag.spv"};
        constexpr uint32_t k_jitterSamples{8};
        constexpr float k_depthTolerance{0.05f};
        constexpr uint64_t k_occurrenceMix{0x9E3779B97F4A7C15ull};

        struct TemporalConstants
        {
            float texel[4];
            float depthParams[4];
            float blend[4];
        };

        static_assert(sizeof(TemporalConstants) == 48);

        float halton(uint32_t index, uint32_t base)
        {
            float fraction = 1.0f;
            float result = 0.0f;

            while (index > 0)
            {
                fraction /= static_cast<float>(base);
                result += fraction * static_cast<float>(index % base);
                index /= base;
            }

            return result;
        }

        uint64_t pairKey(const DrawItem& item)
        {
            const uint64_t mesh = static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(item.mesh));
            const uint64_t material = static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(item.material));

            return mesh * 31u + std::hash<uint64_t>{}(material);
        }

        bool isTemporal(const RendererState& state)
        {
            return state.settings.antiAliasing.mode == AntiAliasingMode::Taa;
        }
    } //namespace

    math::Mat4 beginTemporalFrame(RendererState& state, uint32_t width, uint32_t height)
    {
        TemporalState& temporal = state.temporal;
        temporal.viewProjection = state.projection * state.view;

        if (!temporal.hasPrevious)
        {
            temporal.previousViewProjection = temporal.viewProjection;
        }

        if (!isTemporal(state) || !state.settings.antiAliasing.jitter || width == 0 || height == 0)
        {
            temporal.jitter = {0.0f, 0.0f};
            return state.projection;
        }

        temporal.sampleIndex = (temporal.sampleIndex + 1) % k_jitterSamples;
        temporal.jitter = {halton(temporal.sampleIndex + 1, 2) - 0.5f, halton(temporal.sampleIndex + 1, 3) - 0.5f};

        math::Mat4 jittered = state.projection;
        const float offsetX = 2.0f * temporal.jitter.x / static_cast<float>(width);
        const float offsetY = 2.0f * temporal.jitter.y / static_cast<float>(height);
        jittered[2][0] += offsetX * jittered[2][3];
        jittered[2][1] += offsetY * jittered[2][3];

        return jittered;
    }

    void assignPreviousModels(RendererState& state)
    {
        TemporalState& temporal = state.temporal;
        temporal.currentModels.clear();
        temporal.occurrences.clear();

        if (!isTemporal(state))
        {
            for (DrawItem& item : state.draws)
            {
                item.previousModel = item.model;
            }

            return;
        }

        temporal.currentModels.reserve(state.draws.size());

        for (DrawItem& item : state.draws)
        {
            const uint64_t pair = pairKey(item);
            const uint32_t occurrence = temporal.occurrences[pair]++;
            const uint64_t key = pair ^ (static_cast<uint64_t>(occurrence + 1) * k_occurrenceMix);

            temporal.currentModels.insert_or_assign(key, item.model);

            const auto previous = temporal.previousModels.find(key);
            item.previousModel = previous != temporal.previousModels.end() ? previous->second : item.model;
        }
    }

    void endTemporalFrame(RendererState& state)
    {
        TemporalState& temporal = state.temporal;
        std::swap(temporal.previousModels, temporal.currentModels);
        temporal.previousViewProjection = temporal.viewProjection;
        temporal.hasPrevious = true;
    }

    Status addTemporalPass(vela::graphics::RenderGraph& graph, RendererState& state)
    {
        vela::graphics::AttachmentSlot history{};
        history.name = std::string(k_taaHistoryAttachment);
        history.format = k_displayFormat;
        history.load = vela::graphics::LoadOp::Clear;
        history.store = vela::graphics::StoreOp::Store;
        history.clear = {0.0f, 0.0f, 0.0f, 0.0f};
        history.persistent = true;

        std::vector<vela::graphics::PassInput> inputs{
            {std::string(k_ldrAttachment), vela::graphics::InputUsage::Sampled},
            {std::string(k_sceneVelocityAttachment), vela::graphics::InputUsage::Sampled},
            {std::string(k_sceneDepthAttachment), vela::graphics::InputUsage::Sampled},
            {std::string(k_taaHistoryAttachment), vela::graphics::InputUsage::History}};

        return addFullscreenPass(graph, state, k_temporalShader, std::move(history), std::move(inputs),
            [&state](vela::graphics::PassRecorder& recorder)
            {
                if (!isTemporal(state))
                {
                    return false;
                }

                const vela::graphics::Extent2D extent = recorder.extent();
                const float width = static_cast<float>(extent.width);
                const float height = static_cast<float>(extent.height);
                const AntiAliasingSettings& settings = state.settings.antiAliasing;
                const math::Mat4& projection = state.unjitteredProjection;

                recorder.setConstants(TemporalConstants{
                    .texel = {1.0f / width, 1.0f / height, width, height},
                    .depthParams = {projection[2][2], projection[3][2], k_depthTolerance, 0.0f},
                    .blend = {settings.historyWeight, settings.motionWeight, settings.motionPixels, 0.0f}});
                return true;
            });
    }
} //namespace lockstep::render
