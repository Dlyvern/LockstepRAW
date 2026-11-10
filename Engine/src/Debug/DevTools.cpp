#include "Lockstep/Debug/DevTools.hpp"

#include "Lockstep/Core/Log.hpp"

#include "AudioPanel.hpp"
#include "EnginePanel.hpp"
#include "FrameTimeHistory.hpp"
#include "GpuTimingLog.hpp"
#include "GraphicsPanel.hpp"
#include "ImGuiInput.hpp"
#include "ImGuiOverlayPass.hpp"
#include "NetPanel.hpp"

#include "Platform/WindowState.hpp"
#include "Render/RendererState.hpp"

#include <Vela/Core/Input.hpp>

#include <imgui.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace lockstep::debug
{
    namespace
    {
        constexpr platform::Key k_toggleKey{platform::Key::F1};
        constexpr float k_fallbackDeltaTime{1.0f / 60.0f};

#if defined(NDEBUG)
        constexpr bool k_visibleByDefault{false};
#else
        constexpr bool k_visibleByDefault{true};
#endif

        void configureImGui()
        {
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            io.BackendPlatformName = "lockstep-vela-window";
            io.BackendRendererName = "lockstep-vela";
            io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;

            ImGui::StyleColorsDark();
        }

        void resetInput(ImGuiIO& io, const vela::core::Window& window)
        {
            io.ClearEventsQueue();
            io.ClearInputKeys();
            io.ClearInputMouse();

            double cursorX = 0.0;
            double cursorY = 0.0;
            window.getCursorPosition(cursorX, cursorY);
            io.AddMousePosEvent(static_cast<float>(cursorX), static_cast<float>(cursorY));
        }

        void setMouseEnabled(ImGuiIO& io, bool enabled)
        {
            if (enabled)
            {
                io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
            }
            else
            {
                io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
            }
        }

        void updateDisplay(ImGuiIO& io, const vela::core::Window& window, float deltaTime)
        {
            int windowWidth = 0;
            int windowHeight = 0;
            int framebufferWidth = 0;
            int framebufferHeight = 0;

            window.getSize(windowWidth, windowHeight);
            window.getFramebufferSize(framebufferWidth, framebufferHeight);

            io.DisplaySize = ImVec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight));

            if (windowWidth > 0 && windowHeight > 0)
            {
                io.DisplayFramebufferScale = ImVec2(static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth),
                    static_cast<float>(framebufferHeight) / static_cast<float>(windowHeight));
            }

            io.DeltaTime = deltaTime > 0.0f ? deltaTime : k_fallbackDeltaTime;
        }
    } //namespace

    struct DevToolsState
    {
        DevToolsState(render::RendererState& rendererState, platform::WindowState& windowState)
            : renderer(&rendererState),
              window(&windowState),
              imguiContext(ImGui::CreateContext())
        {
        }

        ~DevToolsState()
        {
            if (overlay != nullptr)
            {
                if (auto removed = renderer->removePass(*overlay); !removed)
                {
                    log(LogLevel::Error, "DevTools", "removing the overlay pass failed: " + removed.error().message);
                }
            }

            ImGui::DestroyContext(imguiContext);
        }

        DevToolsState(const DevToolsState&) = delete;
        DevToolsState& operator=(const DevToolsState&) = delete;
        DevToolsState(DevToolsState&&) = delete;
        DevToolsState& operator=(DevToolsState&&) = delete;

        render::RendererState* renderer{nullptr};
        platform::WindowState* window{nullptr};
        ImGuiContext* imguiContext{nullptr};
        ImGuiOverlayPass* overlay{nullptr};
        bool isVisibleRequested{k_visibleByDefault};
        bool isShown{false};
        NetPanelState netPanel;
        AudioPanelState audioPanel;
        GraphicsPanelState graphicsPanel;
        FrameTimeHistories frameTimes;
        GpuTimingLog gpuTimingLog;
    };

    Result<DevTools> DevTools::create(render::Renderer& renderer, platform::Window& window)
    {
        IMGUI_CHECKVERSION();

        auto state = std::make_unique<DevToolsState>(render::rendererState(renderer), platform::windowState(window));
        ImGui::SetCurrentContext(state->imguiContext);
        configureImGui();

        auto overlay = ImGuiOverlayPass::create(state->renderer->context);

        if (!overlay)
        {
            return overlay.error();
        }

        ImGuiOverlayPass* overlayAddress = overlay.value().get();

        if (auto added = state->renderer->addPass(std::move(overlay).value()); !added)
        {
            return added.error();
        }

        state->overlay = overlayAddress;

        return DevTools(std::move(state));
    }

    DevTools::DevTools(std::unique_ptr<DevToolsState> state)
        : m_state(std::move(state))
    {
    }

    DevTools::~DevTools() = default;

    DevTools::DevTools(DevTools&& other) noexcept = default;

    DevTools& DevTools::operator=(DevTools&& other) noexcept = default;

    void DevTools::beginFrame(float deltaTime)
    {
        if (!m_state)
        {
            return;
        }

        ImGui::SetCurrentContext(m_state->imguiContext);

        const platform::WindowState& windowState = *m_state->window;
        const vela::core::Window& window = windowState.window;

        if (windowState.input.wasKeyPressed(k_toggleKey))
        {
            m_state->isVisibleRequested = !m_state->isVisibleRequested;
        }

        const bool becameVisible = m_state->isVisibleRequested && !m_state->isShown;
        m_state->isShown = m_state->isVisibleRequested;

        if (!m_state->isShown)
        {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();

        if (becameVisible)
        {
            resetInput(io, window);
        }

        setMouseEnabled(io, !windowState.isCursorCaptured);

        for (const vela::core::Event& event : window.events())
        {
            forwardEvent(io, event);
        }

        updateDisplay(io, window, deltaTime);
        m_state->overlay->beginFrame();
        drawEnginePanel(*m_state->renderer, m_state->frameTimes);
        drawGraphicsPanel(m_state->graphicsPanel, *m_state->renderer);
    }

    void DevTools::drawNetPanel(net::Network& network, net::Session& session, net::Replication& replication)
    {
        if (!isVisible())
        {
            return;
        }

        ImGui::SetCurrentContext(m_state->imguiContext);
        debug::drawNetPanel(m_state->netPanel, network, session, replication);
    }

    void DevTools::drawPhysicsSection(const physics::PhysicsWorld& physics)
    {
        if (!isVisible())
        {
            return;
        }

        ImGui::SetCurrentContext(m_state->imguiContext);
        debug::drawPhysicsSection(physics);
    }

    void DevTools::drawAudioPanel(audio::AudioSystem& audio)
    {
        if (!isVisible())
        {
            return;
        }

        ImGui::SetCurrentContext(m_state->imguiContext);
        debug::drawAudioPanel(m_state->audioPanel, audio);
    }

    void DevTools::recordFrameTiming(const FrameTiming& timing)
    {
        if (!m_state)
        {
            return;
        }

        const vela::graphics::FrameStats stats = m_state->renderer->graph->getFrameStats();
        const float engineRenderMilliseconds = std::max(timing.renderMilliseconds - stats.cpuFrameMs, 0.0f);
        FrameTimeHistories& frameTimes = m_state->frameTimes;

        frameTimes.frame.add(timing.workMilliseconds + timing.renderMilliseconds);
        frameTimes.engineWork.add(timing.workMilliseconds + engineRenderMilliseconds);
        frameTimes.velaWork.add(stats.cpuWorkMs);
        frameTimes.velaWait.add(stats.cpuWaitMs);

        if (stats.gpuTimingValid)
        {
            frameTimes.gpu.add(stats.gpuFrameMs);
        }

        recordGpuTimings(m_state->gpuTimingLog, *m_state->renderer->graph);
    }

    void DevTools::logGpuTimings(bool enabled)
    {
        if (m_state)
        {
            m_state->gpuTimingLog.enabled = enabled;
        }
    }

    bool DevTools::isVisible() const
    {
        return m_state && m_state->isShown;
    }

    void DevTools::setVisible(bool visible)
    {
        if (m_state)
        {
            m_state->isVisibleRequested = visible;
        }
    }

    bool DevTools::wantsMouse() const
    {
        if (!isVisible())
        {
            return false;
        }

        ImGui::SetCurrentContext(m_state->imguiContext);

        return ImGui::GetIO().WantCaptureMouse;
    }

    bool DevTools::wantsKeyboard() const
    {
        if (!isVisible())
        {
            return false;
        }

        ImGui::SetCurrentContext(m_state->imguiContext);

        return ImGui::GetIO().WantCaptureKeyboard;
    }
} //namespace lockstep::debug
