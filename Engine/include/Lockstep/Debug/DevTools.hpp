#ifndef LOCKSTEP_DEBUG_DEV_TOOLS_HPP
#define LOCKSTEP_DEBUG_DEV_TOOLS_HPP

#include "Lockstep/Core/Result.hpp"

#include <memory>

namespace lockstep::audio
{
    class AudioSystem;
} //namespace lockstep::audio

namespace lockstep::net
{
    class Network;
    class Replication;
    class Session;
} //namespace lockstep::net

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace lockstep::platform
{
    class Window;
} //namespace lockstep::platform

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace lockstep::debug
{
    struct DevToolsState;

    // Wall-clock split of one run-loop iteration, measured by the run loop.
    // workMilliseconds:   from the start of the iteration (before window.pollEvents()) to the
    //                     call of renderer.render().
    // renderMilliseconds: duration of renderer.render(). Includes Vela's wait for a free frame
    //                     and a swapchain image (the vsync wait), recording, submit and present.
    struct FrameTiming
    {
        float workMilliseconds{0.0f};
        float renderMilliseconds{0.0f};
    };

    // The in-game ImGui dev tools overlay: an engine stats panel, a graphics panel (renderer
    // feature toggles and tunables) plus any panels Game draws with ImGui. F1 toggles it; it starts visible in debug builds and hidden otherwise.
    //
    // Each frame: window.pollEvents(), then beginFrame(), then Game's own ImGui calls (inside
    // `#if LOCKSTEP_DEV_TOOLS`, and only while isVisible()), then renderer.render(), which draws
    // the overlay on top of the scene. The run loop (Lockstep/App) does this; Game only draws its
    // panels from GameApi::devToolsPanels. While the window's cursor is captured, ImGui gets no
    // mouse input.
    //
    // When the Engine is built without LOCKSTEP_DEV_TOOLS, every function is a no-op, isVisible()
    // and the wants*() queries return false, and no ImGui is linked. Game code calling these
    // functions needs no `#if`.
    class DevTools
    {
    public:
        // Creates the ImGui context and adds the overlay pass to the renderer.
        // renderer: the renderer to draw on top of. It must outlive the DevTools and must not be
        //           move-assigned to or destroyed while the DevTools exists.
        // window:   the window whose events and input feed ImGui. Same lifetime rule as
        //           `renderer`.
        // Returns:  the dev tools, or an error if the overlay shaders or GPU resources could not
        //           be created.
        [[nodiscard]] static Result<DevTools> create(render::Renderer& renderer, platform::Window& window);

        // Removes the overlay pass from the renderer (waiting for the GPU) and destroys the ImGui
        // context.
        ~DevTools();

        // Takes over another DevTools' ImGui context and overlay. `other` becomes empty.
        DevTools(DevTools&& other) noexcept;

        // Destroys this DevTools' overlay, then takes over `other`'s. `other` becomes empty.
        DevTools& operator=(DevTools&& other) noexcept;

        DevTools(const DevTools&) = delete;
        DevTools& operator=(const DevTools&) = delete;

        // Feeds the window events of the last window.pollEvents() to ImGui, toggles visibility
        // on F1, and while visible starts the ImGui frame and draws the engine and graphics
        // panels. Call once per frame, after window.pollEvents() and before any Game ImGui call.
        // deltaTime: seconds since the previous frame.
        void beginFrame(float deltaTime);

        // Draws the Net panel: host / connect / disconnect, connection state, ping, loss,
        // simulated lag and loss sliders, replication counters and the interpolation delay. Does
        // nothing while the overlay is hidden. Call once per frame, after beginFrame() and
        // before renderer.render().
        // network:     the networking library, for the simulated conditions.
        // session:     the session to show and control.
        // replication: the session's replication, to show and tune.
        void drawNetPanel(net::Network& network, net::Session& session, net::Replication& replication);

        // Adds a Physics section to the Engine panel: body and controller counts, the last step's
        // simulated time, substeps and wall time, and PhysX error and warning counts. Does
        // nothing while the overlay is hidden. Call once per frame, after beginFrame() and before
        // renderer.render().
        // physics: the physics world to report on.
        void drawPhysicsSection(const physics::PhysicsWorld& physics);

        // Draws the Audio panel: backend, sound and voice counts, mixer CPU use, bus volumes,
        // the World bus' bullet-time effect and test-tone buttons. Does nothing while the overlay
        // is hidden. Call once per frame, after beginFrame() and before renderer.render().
        // audio: the audio system to show and control.
        void drawAudioPanel(audio::AudioSystem& audio);

        // Records one frame for the Engine panel's frame time, CPU work and Vela frame figures
        // (min/avg/max over the last few seconds). CPU work is the frame minus the time spent
        // inside Vela's RenderGraph::execute(), which blocks on vsync. Call once per frame right
        // after renderer.render(), whether or not the overlay is visible.
        // timing: the frame's measured split.
        void recordFrameTiming(const FrameTiming& timing);

        // Logs the GPU time of the frame and of each render graph pass, averaged over a few
        // seconds, to the "Render" channel (the run loop turns it on for `--dev-gpu-timings`).
        // Does nothing in builds without dev tools.
        // enabled: true to log.
        void logGpuTimings(bool enabled);

        // Returns: true if the last beginFrame() showed the overlay. This does not mean an ImGui
        //          frame is open: Game ImGui calls are only valid while this is true AND between
        //          this frame's beginFrame() and renderer.render().
        [[nodiscard]] bool isVisible() const;

        // Shows or hides the overlay from the next beginFrame() on. isVisible() keeps reporting
        // the current frame until then.
        // visible: true to show it.
        void setVisible(bool visible);

        // Returns: true if ImGui uses the mouse this frame (the cursor is over a panel or a panel
        //          is being dragged), so Game should ignore mouse input.
        [[nodiscard]] bool wantsMouse() const;

        // Returns: true if ImGui uses the keyboard this frame (a text field has focus), so Game
        //          should ignore keyboard input.
        [[nodiscard]] bool wantsKeyboard() const;

    private:
        explicit DevTools(std::unique_ptr<DevToolsState> state);

        std::unique_ptr<DevToolsState> m_state;
    };
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_DEV_TOOLS_HPP
