#include "Lockstep/Render/Renderer.hpp"

#include "FrameAttachments.hpp"
#include "FramePasses.hpp"
#include "RendererState.hpp"
#include "ShaderLoader.hpp"
#include "VelaConversions.hpp"

#include "Platform/WindowState.hpp"

#include <Vela/Core/ContextPreferences.hpp>

#include <algorithm>
#include <cassert>
#include <memory>
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
        constexpr std::string_view k_litVertexShader{"lit.vert.spv"};
        constexpr std::string_view k_litFragmentShader{"lit.frag.spv"};
        constexpr std::string_view k_shadowPassName{"shadow"};
        constexpr std::string_view k_scenePassName{"scene"};
        constexpr std::string_view k_tonemapPassName{"tonemap"};
        constexpr const char* k_engineName{"Lockstep"};
        constexpr std::string_view k_enginePassPrefix{"engine-pass-"};

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
                {k_litVertexShader, &shaders.litVertex},
                {k_litFragmentShader, &shaders.litFragment}};

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

            auto shadowPass = ShadowPass::create(state);

            if (!shadowPass)
            {
                return shadowPass.error();
            }

            auto tonemapPass = TonemapPass::create(state);

            if (!tonemapPass)
            {
                return tonemapPass.error();
            }

            if (auto added = addFramePass(graph.value(), k_shadowPassName, std::move(shadowPass).value()); !added)
            {
                return added;
            }

            if (auto added = addFramePass(graph.value(), k_scenePassName, std::make_unique<LitScenePass>(state)); !added)
            {
                return added;
            }

            if (auto added = addFramePass(graph.value(), k_tonemapPassName, std::move(tonemapPass).value()); !added)
            {
                return added;
            }

            graph.value().setPresentSource(std::string(k_displayAttachment));
            state.graph.emplace(std::move(graph).value());

            return {};
        }
    } //namespace

    RendererState::RendererState(vela::core::Window& window, vela::core::Context&& context, MaterialShaders&& shaders,
        vela::graphics::DynamicBuffer&& lightBuffer)
        : window(window),
          context(std::move(context)),
          shaders(std::move(shaders)),
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
        preferences.preferredVSync = description.vsync ? vela::core::VSync::On : vela::core::VSync::Off;

        auto context = vela::core::Context::create(velaWindow, preferences);

        if (!context)
        {
            return toError(context.error());
        }

        if (auto attached = context.value().attach(velaWindow); !attached)
        {
            return toError(attached.error());
        }

        auto lightBuffer = vela::graphics::DynamicBuffer::create(context.value(),
            vela::graphics::DynamicBuffer::Usage::Storage, sizeof(GpuFrameLighting));

        if (!lightBuffer)
        {
            return toError(lightBuffer.error());
        }

        auto state = std::make_unique<RendererState>(velaWindow, std::move(context).value(),
            std::move(shaders).value(), std::move(lightBuffer).value());

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
        const vela::graphics::Mesh* velaMesh = m_state->meshes.find(mesh);
        const MaterialRecord* record = m_state->materials.find(material);

        assert(velaMesh != nullptr && "submit: invalid mesh handle");
        assert(record != nullptr && "submit: invalid material handle");

        if (velaMesh == nullptr || record == nullptr)
        {
            return;
        }

        m_state->draws.push_back({.mesh = velaMesh, .material = record, .model = math::toMatrix(transform)});
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
        updateAspectRatio(*m_state);

        for (RegisteredPass& registered : m_state->passes)
        {
            registered.pass->prepareFrame();
        }

        m_state->prepared = prepareLighting(m_state->lights, m_state->settings);
        m_state->graph->setView(toViewMatrix(m_state->camera),
            toProjectionMatrix(m_state->camera, m_state->aspectRatio));

        auto frame = m_state->graph->execute();
        m_state->lastFrameLights = {
            .local = static_cast<uint32_t>(m_state->lights.points.size() + m_state->lights.spots.size()),
            .dropped = m_state->lights.dropped,
            .hasDirectional = m_state->lights.directional.has_value(),
            .caster = m_state->prepared.caster};
        m_state->draws.clear();
        m_state->lights.clear();
        ++m_state->frameIndex;

        if (!frame)
        {
            return toError(frame.error());
        }

        return {};
    }
} //namespace lockstep::render
