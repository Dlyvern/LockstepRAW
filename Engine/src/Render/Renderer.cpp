#include "Lockstep/Render/Renderer.hpp"

#include "RendererState.hpp"
#include "ShaderLoader.hpp"
#include "VelaConversions.hpp"

#include "Platform/WindowState.hpp"

#include <Vela/Builtins/ForwardGraph.hpp>
#include <Vela/Core/ContextPreferences.hpp>

#include <algorithm>
#include <cassert>
#include <string>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_unlitVertexShader{"unlit.vert.spv"};
        constexpr std::string_view k_unlitFragmentShader{"unlit.frag.spv"};
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
    } //namespace

    RendererState::RendererState(vela::core::Window& window, vela::core::Context&& context,
        std::vector<uint32_t>&& unlitVertex, std::vector<uint32_t>&& unlitFragment)
        : window(window),
          context(std::move(context)),
          unlitVertexShader(std::move(unlitVertex)),
          unlitFragmentShader(std::move(unlitFragment))
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
        auto unlitVertex = loadShader(k_unlitVertexShader);

        if (!unlitVertex)
        {
            return unlitVertex.error();
        }

        auto unlitFragment = loadShader(k_unlitFragmentShader);

        if (!unlitFragment)
        {
            return unlitFragment.error();
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

        auto state = std::make_unique<RendererState>(velaWindow, std::move(context).value(),
            std::move(unlitVertex).value(), std::move(unlitFragment).value());

        auto graph = vela::builtins::forwardGraph(state->context, state->scene);

        if (!graph)
        {
            return toError(graph.error());
        }

        state->graph.emplace(std::move(graph).value());
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
        const vela::graphics::Material* velaMaterial = m_state->materials.find(material);

        assert(velaMesh != nullptr && "submit: invalid mesh handle");
        assert(velaMaterial != nullptr && "submit: invalid material handle");

        if (velaMesh == nullptr || velaMaterial == nullptr)
        {
            return;
        }

        m_state->scene.add(*velaMesh, *velaMaterial, math::toMatrix(transform));
    }

    Status Renderer::render()
    {
        updateAspectRatio(*m_state);

        for (RegisteredPass& registered : m_state->passes)
        {
            registered.pass->prepareFrame();
        }

        m_state->graph->setView(toViewMatrix(m_state->camera),
            toProjectionMatrix(m_state->camera, m_state->aspectRatio));

        auto frame = m_state->graph->execute();
        m_state->scene.clear();

        if (!frame)
        {
            return toError(frame.error());
        }

        return {};
    }
} //namespace lockstep::render
