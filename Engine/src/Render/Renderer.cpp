#include "Lockstep/Render/Renderer.hpp"

#include "FrameAttachments.hpp"
#include "FrameCulling.hpp"
#include "FramePasses.hpp"
#include "PostPasses.hpp"
#include "RendererState.hpp"
#include "ShaderLoader.hpp"
#include "ShadowMaterials.hpp"
#include "VelaConversions.hpp"

#include "Platform/WindowState.hpp"

#include <Vela/Core/ContextPreferences.hpp>

#include <algorithm>
#include <cassert>
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
        constexpr std::string_view k_unlitVertexShader{"unlit.vert.spv"};
        constexpr std::string_view k_unlitFragmentShader{"unlit.frag.spv"};
        constexpr std::string_view k_pbrVertexShader{"pbr.vert.spv"};
        constexpr std::string_view k_pbrFragmentShader{"pbr.frag.spv"};
        constexpr std::string_view k_shadowVertexShader{"shadow.vert.spv"};
        constexpr std::string_view k_shadowFragmentShader{"shadow.frag.spv"};
        constexpr std::string_view k_shadowMaskVertexShader{"shadow_mask.vert.spv"};
        constexpr std::string_view k_shadowMaskFragmentShader{"shadow_mask.frag.spv"};
        constexpr std::string_view k_environmentPassName{"environment"};
        constexpr std::string_view k_shadowPassName{"shadow"};
        constexpr std::string_view k_scenePassName{"scene"};
        constexpr std::string_view k_reflectionPassName{"reflection"};
        constexpr float k_minReflectionScale{0.25f};
        constexpr float k_maxReflectionScale{1.0f};
        constexpr const char* k_engineName{"Lockstep"};

        Result<vela::core::Context> createPresentingContext(vela::core::Window& window,
            vela::core::ContextPreferences preferences, PresentMode mode)
        {
            preferences.preferredVSync = toVelaVSync(mode, {});
            auto context = vela::core::Context::create(window, preferences);

            if (!context)
            {
                return toError(context.error());
            }

            if (auto attached = context.value().attach(window); !attached)
            {
                return toError(attached.error());
            }

            const vela::core::VSync best = toVelaVSync(mode, context.value().getSupportedVSync());

            if (best != preferences.preferredVSync)
            {
                if (auto changed = context.value().setVSync(best); !changed)
                {
                    return toError(changed.error());
                }
            }

            return std::move(context).value();
        }

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

        vela::graphics::Extent2D updateAspectRatio(RendererState& state)
        {
            int width = 0;
            int height = 0;
            state.window.getFramebufferSize(width, height);

            if (width > 0 && height > 0)
            {
                state.aspectRatio = static_cast<float>(width) / static_cast<float>(height);
                return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
            }

            return {};
        }

        void writeCameraMotion(RendererState& state)
        {
            GpuFrameLighting& lighting = state.prepared.lighting;
            lighting.viewProjection = state.temporal.viewProjection;
            lighting.previousViewProjection = state.temporal.previousViewProjection;
        }

        Result<MaterialShaders> loadMaterialShaders()
        {
            MaterialShaders shaders;
            const std::pair<std::string_view, std::vector<uint32_t>*> files[]{
                {k_unlitVertexShader, &shaders.unlitVertex},
                {k_unlitFragmentShader, &shaders.unlitFragment},
                {k_pbrVertexShader, &shaders.pbrVertex},
                {k_pbrFragmentShader, &shaders.pbrFragment},
                {k_shadowVertexShader, &shaders.shadowVertex},
                {k_shadowFragmentShader, &shaders.shadowFragment},
                {k_shadowMaskVertexShader, &shaders.shadowMaskVertex},
                {k_shadowMaskFragmentShader, &shaders.shadowMaskFragment}};

            for (const auto& [fileName, target] : files)
            {
                auto loaded = loadShader(fileName);

                if (!loaded)
                {
                    return loaded.error();
                }

                *target = std::move(loaded).value();
            }

            return shaders;
        }

        Status addFramePass(vela::graphics::RenderGraph& graph, std::string_view name,
            std::unique_ptr<vela::graphics::Pass> pass)
        {
            if (auto added = graph.addPass(std::string(name), std::move(pass)); !added)
            {
                return toError(added.error());
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

            auto scenePass = LitScenePass::create(state);

            if (!scenePass)
            {
                return scenePass.error();
            }

            if (auto added = addFramePass(graph.value(), k_environmentPassName, std::move(environmentPass).value());
                !added)
            {
                return added;
            }

            if (auto added = addFramePass(graph.value(), k_shadowPassName, std::move(shadowPass).value()); !added)
            {
                return added;
            }

            if (auto added = addFramePass(graph.value(), k_reflectionPassName, std::move(reflectionPass).value());
                !added)
            {
                return added;
            }

            if (auto added = addFramePass(graph.value(), k_scenePassName, std::move(scenePass).value()); !added)
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
    } //namespace

    RendererState& rendererState(Renderer& renderer)
    {
        return *renderer.m_state;
    }

    Result<Renderer> Renderer::create(platform::Window& window, const RendererDescription& description)
    {
        auto shaders = loadMaterialShaders();

        if (!shaders)
        {
            return shaders.error();
        }

        vela::core::Window& velaWindow = platform::windowState(window).window;

        vela::core::ContextPreferences preferences{};
        preferences.applicationName = description.applicationName;
        preferences.engineName = k_engineName;
        preferences.optionalFeatures.push_back(vela::core::Feature::SamplerAnisotropy);

        auto context = createPresentingContext(velaWindow, preferences, description.presentMode);

        if (!context)
        {
            return context.error();
        }

        auto lightBuffer = vela::graphics::DynamicBuffer::create(context.value(),
            vela::graphics::DynamicBuffer::Usage::Storage, sizeof(GpuFrameLighting));

        if (!lightBuffer)
        {
            return toError(lightBuffer.error());
        }

        auto environment = EnvironmentResources::create(context.value());

        if (!environment)
        {
            return environment.error();
        }

        auto state = std::make_unique<RendererState>(velaWindow, std::move(context).value(),
            std::move(shaders).value(), std::move(lightBuffer).value(), std::move(environment).value());

        state->presentMode = description.presentMode;

        if (auto shadowMaterials = updateShadowMaterials(*state); !shadowMaterials)
        {
            return shadowMaterials.error();
        }

        if (auto built = buildFrameGraph(*state); !built)
        {
            return built.error();
        }

        static_cast<void>(updateAspectRatio(*state));

        return Renderer(std::move(state));
    }

    Renderer::Renderer(std::unique_ptr<RendererState> state)
        : m_state(std::move(state))
    {
    }

    Renderer::~Renderer() = default;

    Renderer::Renderer(Renderer&& other) noexcept = default;

    Renderer& Renderer::operator=(Renderer&& other) noexcept = default;

    void Renderer::setCamera(const Camera& camera)
    {
        m_state->camera = camera;
    }

    void Renderer::submit(MeshHandle mesh, MaterialHandle material, const math::Transform& transform)
    {
        submit(mesh, material, transform, k_defaultRenderLayer);
    }

    void Renderer::submit(MeshHandle mesh, MaterialHandle material, const math::Transform& transform,
        RenderLayers layers)
    {
        const MeshRecord* record = m_state->meshes.find(mesh);
        const MaterialRecord* materialRecord = m_state->materials.find(material);

        assert(record != nullptr && "submit: invalid mesh handle");
        assert(materialRecord != nullptr && "submit: invalid material handle");

        if (record == nullptr || materialRecord == nullptr)
        {
            return;
        }

        const math::Mat4 model = math::toMatrix(transform);
        m_state->draws.push_back({
            .mesh = &record->mesh,
            .material = materialRecord,
            .model = model,
            .previousModel = model,
            .worldBounds = math::transformBounds(record->bounds, transform),
            .cameraDistance = 0.0f,
            .layers = layers,
            .isReflector = false});
    }

    void Renderer::submitReflector(const PlanarReflector& reflector)
    {
        if (m_state->reflectors.size() < k_maxPlanarReflectors)
        {
            m_state->reflectors.push_back(reflector);
        }
    }

    void Renderer::submitLight(const DirectionalLight& light)
    {
        m_state->lights.directional = light;
    }

    void Renderer::submitLight(const PointLight& light)
    {
        if (!m_state->lights.hasRoom())
        {
            ++m_state->lights.dropped;
            return;
        }

        m_state->lights.points.push_back(light);
    }

    void Renderer::submitLight(const SpotLight& light)
    {
        if (!m_state->lights.hasRoom())
        {
            ++m_state->lights.dropped;
            return;
        }

        m_state->lights.spots.push_back(light);
    }

    Status Renderer::setPresentMode(PresentMode mode)
    {
        return m_state->setPresentMode(mode);
    }

    PresentMode Renderer::presentMode() const
    {
        return m_state->presentMode;
    }

    RenderSettings& Renderer::settings()
    {
        return m_state->settings;
    }

    const RenderSettings& Renderer::settings() const
    {
        return m_state->settings;
    }

    Status Renderer::render()
    {
        RendererState& state = *m_state;
        const vela::graphics::Extent2D extent = updateAspectRatio(state);

        for (RegisteredPass& registered : state.passes)
        {
            registered.pass->prepareFrame();
        }

        state.view = toViewMatrix(state.camera);
        state.unjitteredProjection = toProjectionMatrix(state.camera, state.aspectRatio);
        state.projection = state.unjitteredProjection;
        state.projection = beginTemporalFrame(state, extent.width, extent.height);
        state.prepared = prepareLighting(state.lights, state.settings, state.camera, state.aspectRatio);
        writeCameraMotion(state);
        assignPreviousModels(state);
        buildDrawLists(state, state.temporal.viewProjection);
        prepareReflection(state);
        state.graph->setView(state.view, state.projection);

        Status frame = updateShadowMaterials(state);

        if (frame)
        {
            frame = updateSceneSamples(state);
        }

        if (frame)
        {
            frame = updateReflectionScale(state);
        }

        if (frame)
        {
            if (auto executed = state.graph->execute(); !executed)
            {
                frame = toError(executed.error());
            }
        }

        uint32_t shadowDraws = 0;

        for (const std::vector<uint32_t>& casters : state.drawLists.shadowCasters)
        {
            shadowDraws += static_cast<uint32_t>(casters.size());
        }

        state.lastFrame = {
            .local = static_cast<uint32_t>(state.lights.points.size() + state.lights.spots.size()),
            .dropped = state.lights.dropped,
            .hasDirectional = state.prepared.hasSun,
            .spotShadow = state.prepared.spotTile.has_value(),
            .cascades = state.prepared.cascadeCount,
            .submitted = static_cast<uint32_t>(state.draws.size()),
            .visible = static_cast<uint32_t>(state.drawLists.opaque.size() + state.drawLists.blended.size()),
            .shadowDraws = shadowDraws};
        endTemporalFrame(state);
        state.draws.clear();
        state.drawLists.clear();
        state.lights.clear();
        state.reflectors.clear();
        ++state.frameIndex;

        return frame;
    }
} //namespace lockstep::render
