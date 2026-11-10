#include "ScreenSpaceReflections.hpp"

#include "FrameAttachments.hpp"
#include "RendererState.hpp"
#include "ScreenPass.hpp"

#include <Vela/Graphics/RenderGraph.hpp>

#include <algorithm>
#include <string>
#include <string_view>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_traceShader{"ssr_trace.frag.spv"};
        constexpr std::string_view k_resolveShader{"ssr_resolve.frag.spv"};
        constexpr std::string_view k_sourceShader{"ssr_source.frag.spv"};
        constexpr std::string_view k_tracePassName{"ssrTrace"};
        constexpr std::string_view k_resolvePassName{"ssrResolve"};
        constexpr std::string_view k_sourcePassName{"ssrSource"};
        constexpr float k_halfResolution{0.5f};
        constexpr uint32_t k_minSteps{8};
        constexpr uint32_t k_maxSteps{64};
        constexpr float k_maxHistoryWeight{0.97f};
        constexpr uint32_t k_noisePeriod{64};

        struct TraceConstants
        {
            ProjectionTerms projection;
            float ray[4];
            float fade[4];
        };

        struct ResolveConstants
        {
            float params[4];
        };

        static_assert(sizeof(TraceConstants) == 48);
        static_assert(sizeof(ResolveConstants) == 16);

        vela::graphics::PassInput sampled(std::string_view name)
        {
            return {std::string(name), vela::graphics::InputUsage::Sampled};
        }

        vela::graphics::PassInput history(std::string_view name)
        {
            return {std::string(name), vela::graphics::InputUsage::History};
        }

        vela::graphics::AttachmentSlot persistentSlot(std::string_view name)
        {
            vela::graphics::AttachmentSlot slot = clearedScreenSlot(name, k_sceneColorFormat, k_halfResolution,
                vela::graphics::ClearValue{0.0f, 0.0f, 0.0f, 0.0f});
            slot.persistent = true;

            return slot;
        }

        TraceConstants traceConstants(const RendererState& state)
        {
            const ScreenSpaceReflectionSettings& ssr = state.settings.screenSpaceReflections;

            return TraceConstants{
                .projection = projectionTerms(state),
                .ray = {std::max(ssr.maxDistance, 0.5f), std::max(ssr.thickness, 0.01f),
                    static_cast<float>(std::clamp(ssr.steps, k_minSteps, k_maxSteps)),
                    std::clamp(ssr.maxRoughness, 0.05f, 1.0f)},
                .fade = {std::clamp(ssr.edgeFade, 0.001f, 0.5f), temporalNoiseOffset(state), 0.0f, 0.0f}};
        }

        ResolveConstants resolveConstants(const RendererState& state)
        {
            const ScreenSpaceReflectionSettings& ssr = state.settings.screenSpaceReflections;
            const float weight = ssr.temporal && state.temporal.hasPrevious
                ? std::clamp(ssr.historyWeight, 0.0f, k_maxHistoryWeight) : 0.0f;

            return ResolveConstants{.params = {weight, 0.0f, 0.0f, 0.0f}};
        }
    } //namespace

    bool screenSpaceReflectionsActive(const RendererState& state)
    {
        const ScreenSpaceReflectionSettings& ssr = state.settings.screenSpaceReflections;
        return ssr.enabled && ssr.intensity > 0.0f && state.settings.lighting.specular;
    }

    ProjectionTerms projectionTerms(const RendererState& state)
    {
        const math::Mat4& projection = state.projection;
        return ProjectionTerms{{projection[0][0], projection[1][1], projection[2][2], projection[3][2]}};
    }

    float temporalNoiseOffset(const RendererState& state)
    {
        const bool accumulates = state.settings.antiAliasing.mode == AntiAliasingMode::Taa;
        return accumulates ? static_cast<float>(state.frameIndex % k_noisePeriod) : 0.0f;
    }

    Status addScreenSpaceReflectionPasses(vela::graphics::RenderGraph& graph, RendererState& state)
    {
        ScreenPassDescription trace{
            .name = k_tracePassName,
            .fragmentShader = k_traceShader,
            .outputs = {clearedScreenSlot(k_ssrHitAttachment, k_sceneColorFormat, k_halfResolution,
                vela::graphics::ClearValue{0.0f, 0.0f, 0.0f, 0.0f})},
            .inputs = {sampled(k_sceneDepthAttachment), sampled(k_surfaceNormalAttachment),
                sampled(k_surfaceSpecularAttachment)}};

        if (auto added = addScreenPass(graph, state, std::move(trace),
            [&state](vela::graphics::PassRecorder& recorder)
            {
                recorder.setConstants(traceConstants(state));
                return screenSpaceReflectionsActive(state);
            }); !added)
        {
            return added;
        }

        ScreenPassDescription resolve{
            .name = k_resolvePassName,
            .fragmentShader = k_resolveShader,
            .outputs = {persistentSlot(k_ssrResolvedAttachment)},
            .inputs = {sampled(k_ssrHitAttachment), history(k_ssrSourceAttachment),
                sampled(k_sceneVelocityAttachment), history(k_ssrResolvedAttachment)}};

        if (auto added = addScreenPass(graph, state, std::move(resolve),
            [&state](vela::graphics::PassRecorder& recorder)
            {
                recorder.setConstants(resolveConstants(state));
                return screenSpaceReflectionsActive(state);
            }); !added)
        {
            return added;
        }

        ScreenPassDescription source{
            .name = k_sourcePassName,
            .fragmentShader = k_sourceShader,
            .outputs = {persistentSlot(k_ssrSourceAttachment)},
            .inputs = {sampled(k_sceneColorAttachment), sampled(k_sceneAmbientAttachment),
                sampled(k_occlusionBlurAttachment)}};

        return addScreenPass(graph, state, std::move(source),
            [&state](vela::graphics::PassRecorder&)
            {
                return screenSpaceReflectionsActive(state);
            });
    }
} //namespace lockstep::render
