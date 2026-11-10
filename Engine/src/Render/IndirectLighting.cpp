#include "IndirectLighting.hpp"

#include "FrameAttachments.hpp"
#include "RendererState.hpp"
#include "ScreenPass.hpp"
#include "ScreenSpaceReflections.hpp"

#include <Vela/Graphics/RenderGraph.hpp>

#include <algorithm>
#include <string>
#include <string_view>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_occlusionShader{"gtao.frag.spv"};
        constexpr std::string_view k_blurShader{"occlusion_blur.frag.spv"};
        constexpr std::string_view k_compositeShader{"indirect_composite.frag.spv"};
        constexpr std::string_view k_occlusionPassName{"occlusion"};
        constexpr std::string_view k_blurPassName{"occlusionBlur"};
        constexpr std::string_view k_compositePassName{"indirectComposite"};
        constexpr float k_halfResolution{0.5f};
        constexpr uint32_t k_maxSlices{4};
        constexpr uint32_t k_minSteps{2};
        constexpr uint32_t k_maxSteps{12};
        constexpr uint32_t k_minContactSteps{4};
        constexpr uint32_t k_maxContactSteps{24};

        struct OcclusionConstants
        {
            ProjectionTerms projection;
            float occlusion[4];
            float sampling[4];
            float sunView[4];
            float contact[4];
        };

        struct BlurConstants
        {
            ProjectionTerms projection;
        };

        struct CompositeConstants
        {
            float params[4];
        };

        static_assert(sizeof(OcclusionConstants) == 80);
        static_assert(sizeof(BlurConstants) == 16);
        static_assert(sizeof(CompositeConstants) == 16);

        vela::graphics::PassInput sampled(std::string_view name)
        {
            return {std::string(name), vela::graphics::InputUsage::Sampled};
        }

        bool occlusionActive(const RendererState& state)
        {
            return state.settings.ambientOcclusion.enabled;
        }

        bool contactShadowsActive(const RendererState& state)
        {
            const ContactShadowSettings& contact = state.settings.contactShadows;
            return contact.enabled && contact.intensity > 0.0f && contact.length > 0.0f
                && state.settings.lighting.directional && state.prepared.lighting.sunDirection[3] > 0.0f;
        }

        math::Vec3 sunInView(const RendererState& state)
        {
            const float* travel = state.prepared.lighting.sunDirection;
            const math::Vec4 toSun = state.view * math::Vec4{-travel[0], -travel[1], -travel[2], 0.0f};
            return math::normalize(math::Vec3{toSun.x, toSun.y, toSun.z});
        }

        OcclusionConstants occlusionConstants(const RendererState& state)
        {
            const AmbientOcclusionSettings& occlusion = state.settings.ambientOcclusion;
            const ContactShadowSettings& contact = state.settings.contactShadows;
            const bool hasContact = contactShadowsActive(state);
            const math::Vec3 toSun = hasContact ? sunInView(state) : math::Vec3{0.0f, 1.0f, 0.0f};

            return OcclusionConstants{
                .projection = projectionTerms(state),
                .occlusion = {std::max(occlusion.radius, 0.05f), std::clamp(occlusion.intensity, 0.0f, 2.0f),
                    std::max(occlusion.power, 0.1f), occlusionActive(state) ? 1.0f : 0.0f},
                .sampling = {static_cast<float>(std::clamp(occlusion.slices, 1u, k_maxSlices)),
                    static_cast<float>(std::clamp(occlusion.steps, k_minSteps, k_maxSteps)), temporalNoiseOffset(state),
                    occlusion.specularOcclusion ? 1.0f : 0.0f},
                .sunView = {toSun.x, toSun.y, toSun.z, contact.length},
                .contact = {std::max(contact.thickness, 0.005f), std::clamp(contact.intensity, 0.0f, 1.0f),
                    static_cast<float>(std::clamp(contact.steps, k_minContactSteps, k_maxContactSteps)),
                    hasContact ? 1.0f : 0.0f}};
        }

        CompositeConstants compositeConstants(const RendererState& state)
        {
            const AmbientOcclusionSettings& occlusion = state.settings.ambientOcclusion;
            const ScreenSpaceReflectionSettings& ssr = state.settings.screenSpaceReflections;
            const bool reflects = screenSpaceReflectionsActive(state);
            const float albedo = occlusion.enabled && occlusion.multiBounce
                ? std::clamp(occlusion.multiBounceAlbedo, 0.0f, 1.0f) : 0.0f;

            return CompositeConstants{.params = {reflects ? std::max(ssr.intensity, 0.0f) : 0.0f, albedo,
                reflects && ssr.debugView ? 1.0f : 0.0f, 0.0f}};
        }

        Status addOcclusionPasses(vela::graphics::RenderGraph& graph, RendererState& state)
        {
            const vela::graphics::ClearValue visible{1.0f, 1.0f, 1.0f, 1.0f};

            ScreenPassDescription occlusion{
                .name = k_occlusionPassName,
                .fragmentShader = k_occlusionShader,
                .outputs = {clearedScreenSlot(k_occlusionAttachment, k_occlusionFormat, k_halfResolution, visible)},
                .inputs = {sampled(k_sceneDepthAttachment), sampled(k_surfaceNormalAttachment)}};

            if (auto added = addScreenPass(graph, state, std::move(occlusion),
                [&state](vela::graphics::PassRecorder& recorder)
                {
                    recorder.setConstants(occlusionConstants(state));
                    return occlusionActive(state) || contactShadowsActive(state);
                }); !added)
            {
                return added;
            }

            ScreenPassDescription blur{
                .name = k_blurPassName,
                .fragmentShader = k_blurShader,
                .outputs = {clearedScreenSlot(k_occlusionBlurAttachment, k_occlusionFormat, k_halfResolution, visible)},
                .inputs = {sampled(k_occlusionAttachment), sampled(k_sceneDepthAttachment)}};

            return addScreenPass(graph, state, std::move(blur),
                [&state](vela::graphics::PassRecorder& recorder)
                {
                    recorder.setConstants(BlurConstants{.projection = projectionTerms(state)});
                    return occlusionActive(state) || contactShadowsActive(state);
                });
        }
    } //namespace

    Status addIndirectLightingPasses(vela::graphics::RenderGraph& graph, RendererState& state)
    {
        if (auto added = addOcclusionPasses(graph, state); !added)
        {
            return added;
        }

        if (auto added = addScreenSpaceReflectionPasses(graph, state); !added)
        {
            return added;
        }

        ScreenPassDescription composite{
            .name = k_compositePassName,
            .fragmentShader = k_compositeShader,
            .blend = vela::graphics::BlendMode::Alpha,
            .outputs = {clearedScreenSlot(k_sceneAmbientAttachment, k_sceneColorFormat, 1.0f,
                vela::graphics::ClearValue{0.0f, 0.0f, 0.0f, 0.0f}),
                loadedScreenSlot(k_sceneColorAttachment, k_sceneColorFormat)},
            .inputs = {sampled(k_sceneAmbientLitAttachment), sampled(k_surfaceSpecularAttachment),
                sampled(k_occlusionBlurAttachment), sampled(k_ssrResolvedAttachment)}};

        return addScreenPass(graph, state, std::move(composite),
            [&state](vela::graphics::PassRecorder& recorder)
            {
                recorder.setConstants(compositeConstants(state));
                return true;
            });
    }
} //namespace lockstep::render
