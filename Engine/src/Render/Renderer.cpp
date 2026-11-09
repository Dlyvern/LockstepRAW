#include "Lockstep/Render/Renderer.hpp"

#include "FrameAttachments.hpp"
#include "FrameCulling.hpp"
#include "FramePasses.hpp"
#include "PostPasses.hpp"
#include "RendererState.hpp"
#include "ShaderLoader.hpp"
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
        constexpr std::string_view k_shadowMaskVertexShader{"shadow_mask.vert.spv"};
        constexpr std::string_view k_shadowMaskFragmentShader{"shadow_mask.frag.spv"};
        constexpr std::string_view k_environmentPassName{"environment"};
        constexpr std::string_view k_shadowPassName{"shadow"};
        constexpr std::string_view k_scenePassName{"scene"};
        constexpr const char* k_engineName{"Lockstep"};
        constexpr std::string_view k_enginePassPrefix{"engine-pass-"};
        constexpr vela::core::VSync k_uncappedChain[]{vela::core::VSync::Off, vela::core::VSync::Fast};
        constexpr vela::core::VSync k_lowLatencyChain[]{vela::core::VSync::Fast};

        std::span<const vela::core::VSync> fallbackChain(PresentMode mode)
        {
            switch (mode)
            {
                case PresentMode::Uncapped:
                {
                    return k_uncappedChain;
                }
                case PresentMode::LowLatency:
                {
                    return k_lowLatencyChain;
                }
                case PresentMode::VSync:
                {
                    break;
                }
            }

            return {};
        }

        Result<vela::core::Context> createAttachedContext(vela::core::Window& window,
            vela::core::ContextPreferences& preferences, vela::core::VSync vsync)
        {
            preferences.preferredVSync = vsync;
            auto context = vela::core::Context::create(window, preferences);

            if (!context)
            {
                return toError(context.error());
            }

            if (auto attached = context.value().attach(window); !attached)
            {
                return toError(attached.error());
            }

            return std::move(context).value();
        }

        Result<vela::core::Context> createPresentingContext(vela::core::Window& window,
            vela::core::ContextPreferences preferences, PresentMode mode)
        {
            for (const vela::core::VSync vsync : fallbackChain(mode))
            {
                auto context = createAttachedContext(window, preferences, vsync);

                if (!context || context.value().getSwapchainInfo().actualVSync == vsync)
                {
                    return context;
                }
            }

            return createAttachedContext(window, preferences, vela::core::VSync::On);
        }

        void updateAspectRatio(RendererState& state)
        {
            int width = 0;
            int height = 0;
            state.window.getFramebufferSize(width, height);

            if (width > 0 && height > 0)
            {
                state.aspectRatio = static_cast<float>(width) / static_cast<float>(height);
            }
        }

        Result<MaterialShaders> loadMaterialShaders()
        {
            MaterialShaders shaders;
            const std::pair<std::string_view, std::vector<uint32_t>*> files[]{
                {k_unlitVertexShader, &shaders.unlitVertex},
                {k_unlitFragmentShader, &shaders.unlitFragment},
                {k_pbrVertexShader, &shaders.pbrVertex},
                {k_pbrFragmentShader, &shaders.pbrFragment},
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

    RendererState::RendererState(vela::core::Window& window, vela::core::Context&& context, MaterialShaders&& shaders,
        vela::graphics::DynamicBuffer&& lightBuffer, EnvironmentResources&& environment)
        : window(window),
          context(std::move(context)),
          shaders(std::move(shaders)),
          environment(std::move(environment)),
          lightBuffer(std::move(lightBuffer))
    {
    }

    RendererState::~RendererState()
    {
        context.waitIdle();
    }

    Status RendererState::addPass(std::unique_ptr<RenderPass> pass)
    {
        assert(pass != nullptr && "addPass: pass must not be null");

        if (pass == nullptr)
        {
            return Error{ErrorCode::InvalidArgument, "addPass: pass must not be null"};
        }

        auto adapter = std::make_unique<RenderPassAdapter>(*pass);
        RenderPassAdapter* adapterAddress = adapter.get();
        const std::string name = std::string(k_enginePassPrefix) + std::to_string(passNameCounter++);

        if (auto added = graph->addPass(name, std::move(adapter)); !added)
        {
            return toError(added.error());
        }

        passes.push_back({.pass = std::move(pass), .adapter = adapterAddress});

        return {};
    }

    void RendererState::removePass(const RenderPass& pass)
    {
        const auto registered = std::find_if(passes.begin(), passes.end(), [&pass](const RegisteredPass& entry)
        {
            return entry.adapter->isAttachedTo(pass);
        });

        if (registered == passes.end())
        {
            return;
        }

        context.waitIdle();
        registered->adapter->detach();
        passes.erase(registered);
    }

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

        state->requestedPresentMode = description.presentMode;

        if (auto built = buildFrameGraph(*state); !built)
        {
            return built.error();
        }

        updateAspectRatio(*state);

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
        const MeshRecord* record = m_state->meshes.find(mesh);
        const MaterialRecord* materialRecord = m_state->materials.find(material);

        assert(record != nullptr && "submit: invalid mesh handle");
        assert(materialRecord != nullptr && "submit: invalid material handle");

        if (record == nullptr || materialRecord == nullptr)
        {
            return;
        }

        m_state->draws.push_back({
            .mesh = &record->mesh,
            .material = materialRecord,
            .model = math::toMatrix(transform),
            .worldBounds = math::transformBounds(record->bounds, transform),
            .cameraDistance = 0.0f});
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
        updateAspectRatio(state);

        for (RegisteredPass& registered : state.passes)
        {
            registered.pass->prepareFrame();
        }

        state.view = toViewMatrix(state.camera);
        state.projection = toProjectionMatrix(state.camera, state.aspectRatio);
        state.prepared = prepareLighting(state.lights, state.settings, state.camera, state.aspectRatio);
        buildDrawLists(state, state.projection * state.view);
        state.graph->setView(state.view, state.projection);

        auto frame = state.graph->execute();

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
        state.draws.clear();
        state.drawLists.clear();
        state.lights.clear();
        ++state.frameIndex;

        if (!frame)
        {
            return toError(frame.error());
        }

        return {};
    }
} //namespace lockstep::render
