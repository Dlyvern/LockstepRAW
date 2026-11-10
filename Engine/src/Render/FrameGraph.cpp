#include "FrameGraph.hpp"

#include "FrameAttachments.hpp"
#include "FramePasses.hpp"
#include "IndirectLighting.hpp"
#include "ParticlePass.hpp"
#include "PlanarReflections.hpp"
#include "PostPasses.hpp"
#include "RenderScale.hpp"
#include "RendererState.hpp"
#include "ScopeView.hpp"
#include "OverlayPass.hpp"
#include "VelaConversions.hpp"
#include "VolumetricFog.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_environmentPassName{"environment"};
        constexpr std::string_view k_shadowPassName{"shadow"};
        constexpr std::string_view k_scenePassName{"scene"};
        constexpr std::string_view k_reflectionPassName{"reflection"};
        constexpr std::string_view k_particlePassName{"particles"};
        constexpr float k_minReflectionScale{0.25f};
        constexpr float k_maxReflectionScale{1.0f};

        uint32_t supportedSamples(const RendererState& state, uint32_t requested)
        {
            const uint32_t maximum = state.context.getDeviceInfo().maxMsaaSamples;
            uint32_t samples = 1;

            while (samples * 2 <= std::min(requested, maximum))
            {
                samples *= 2;
            }

            return samples;
        }

        uint32_t requestedSamples(AntiAliasingMode mode)
        {
            switch (mode)
            {
                case AntiAliasingMode::Msaa2x:
                {
                    return 2;
                }
                case AntiAliasingMode::Msaa4x:
                {
                    return 4;
                }
                case AntiAliasingMode::Off:
                case AntiAliasingMode::Fxaa:
                case AntiAliasingMode::Taa:
                {
                    return 1;
                }
            }

            return 1;
        }
    } //namespace

    Status updateSceneSamples(RendererState& state)
    {
        const uint32_t samples = supportedSamples(state, requestedSamples(state.settings.antiAliasing.mode));

        if (samples == state.sceneSamples)
        {
            return {};
        }

        const uint32_t previous = state.sceneSamples;
        state.sceneSamples = samples;

        if (auto redescribed = state.graph->redescribePass(std::string(k_scenePassName)); !redescribed)
        {
            state.sceneSamples = previous;
            state.settings.antiAliasing.mode = AntiAliasingMode::Off;
            state.quality.antiAliasing = AntiAliasingMode::Off;
            return toError(redescribed.error());
        }

        return {};
    }

    Status updateReflectionScale(RendererState& state)
    {
        float& requested = state.settings.reflections.resolutionScale;
        requested = std::clamp(requested, k_minReflectionScale, k_maxReflectionScale);

        if (requested == state.reflectionScale)
        {
            return {};
        }

        const float previous = state.reflectionScale;
        state.reflectionScale = requested;

        if (auto redescribed = state.graph->redescribePass(std::string(k_reflectionPassName)); !redescribed)
        {
            state.reflectionScale = previous;
            requested = previous;
            return toError(redescribed.error());
        }

        return {};
    }

    Status buildFrameGraph(RendererState& state)
    {
        auto graph = vela::graphics::RenderGraph::create(state.context);

        if (!graph)
        {
            return toError(graph.error());
        }

        auto environmentPass = EnvironmentPass::create(state);

        if (!environmentPass)
        {
            return environmentPass.error();
        }

        auto shadowPass = ShadowPass::create(state);

        if (!shadowPass)
        {
            return shadowPass.error();
        }

        auto reflectionPass = ReflectionPass::create(state);

        if (!reflectionPass)
        {
            return reflectionPass.error();
        }

        auto scopePass = ScopePass::create(state);

        if (!scopePass)
        {
            return scopePass.error();
        }

        auto scenePass = LitScenePass::create(state);

        if (!scenePass)
        {
            return scenePass.error();
        }

        if (auto added = addScaledPass(graph.value(), state, k_environmentPassName, std::move(environmentPass).value());
            !added)
        {
            return added;
        }

        if (auto added = addScaledPass(graph.value(), state, k_shadowPassName, std::move(shadowPass).value()); !added)
        {
            return added;
        }

        if (auto added = addScaledPass(graph.value(), state, k_reflectionPassName, std::move(reflectionPass).value());
            !added)
        {
            return added;
        }

        if (auto added = addScaledPass(graph.value(), state, k_scopePassName, std::move(scopePass).value()); !added)
        {
            return added;
        }

        if (auto added = addScaledPass(graph.value(), state, k_scenePassName, std::move(scenePass).value()); !added)
        {
            return added;
        }

        if (auto added = addIndirectLightingPasses(graph.value(), state); !added)
        {
            return added;
        }

        if (auto added = addVolumetricFogPasses(graph.value(), state); !added)
        {
            return added;
        }

        auto particlePass = ParticlePass::create(state);

        if (!particlePass)
        {
            return particlePass.error();
        }

        if (auto added = addScaledPass(graph.value(), state, k_particlePassName, std::move(particlePass).value());
            !added)
        {
            return added;
        }

        if (auto added = addPostPasses(graph.value(), state); !added)
        {
            return added;
        }

        graph.value().setPresentSource(std::string(k_displayAttachment));
        state.graph.emplace(std::move(graph).value());

        return {};
    }

    Status addOverlayPass(RendererState& state)
    {
        auto overlayPass = OverlayPass::create(state.context, state);

        if (!overlayPass)
        {
            return overlayPass.error();
        }

        OverlayPass* pass = overlayPass.value().get();

        if (auto added = state.addPass(std::move(overlayPass).value()); !added)
        {
            return added;
        }

        state.overlayPass = pass;

        return {};
    }
} //namespace lockstep::render
