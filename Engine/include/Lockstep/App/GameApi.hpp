#ifndef LOCKSTEP_APP_GAME_API_HPP
#define LOCKSTEP_APP_GAME_API_HPP

#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Platform/Input.hpp"

#include <cstdint>
#include <span>

namespace lockstep::platform
{
    class Window;
} //namespace lockstep::platform

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace lockstep::net
{
    class Replication;
    class Session;
} //namespace lockstep::net

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace lockstep::assets
{
    class FileSystem;
} //namespace lockstep::assets

namespace lockstep::world
{
    class Entities;
    class TimeDomains;
} //namespace lockstep::world

namespace lockstep::app
{
    // All of Game's state. Declared here, defined by Game. The Engine only passes the pointer
    // returned from GameApi::init back into Game, so Game keeps no globals and can later be
    // reloaded as a shared library.
    struct GameState;

    // The Engine services Game may use from its callbacks. Valid for the duration of the run.
    // session:     the network session: its role tells Game whether it hosts, is a client or
    //              plays offline. Game may send and read its own messages through it.
    // replication: host-to-client entity and World-scale replication, and the client's per-tick
    //              input message to the host.
    // physics:     rigid bodies, raycasts and character controllers. Stepped by the run loop
    //              with the World delta before each fixedTick.
    // files:       the virtual file system with RunDescription::content mounted; cooked levels
    //              load through it, and its pak content hashes identify the content.
    // arguments:   the command-line options from RunDescription::arguments, for Game's own
    //              options (Engine's parsers ignore what they don't know).
    struct EngineContext
    {
        platform::Window& window;
        render::Renderer& renderer;
        world::TimeDomains& timeDomains;
        world::Entities& entities;
        net::Session& session;
        net::Replication& replication;
        physics::PhysicsWorld& physics;
        assets::FileSystem& files;
        std::span<const char* const> arguments;
    };

    // What one fixed simulation tick gets.
    // tick:        number of this tick, counting from 0.
    // tickSeconds: unscaled tick length. Per-domain deltas come from EngineContext::timeDomains.
    // input:       input since the previous tick, filtered for the dev tools.
    struct TickInfo
    {
        uint64_t tick{0};
        float tickSeconds{0.0f};
        const platform::Input& input;
    };

    // What one rendered frame gets.
    // ticksCompleted: number of fixed ticks run so far.
    // alpha:          how far real time is between the last tick and the next, in [0, 1).
    //                 Blend the previous and the current tick's state with it.
    // frameSeconds:   wall time since the previous frame. For presentation only (camera
    //                 smoothing, UI), never for simulation.
    // input:          input since the previous frame, filtered for the dev tools.
    struct FrameInfo
    {
        uint64_t ticksCompleted{0};
        float alpha{0.0f};
        float frameSeconds{0.0f};
        const platform::Input& input;
    };

    // The functions the Engine calls into Game. Plain function pointers, no captured state: the
    // Engine keeps only this table and the GameState pointer, so swapping in a reloaded Game is
    // a matter of handing over a new table. Every function except devToolsPanels is required.
    struct GameApi
    {
        // Creates the game state: resources, scene, settings. Called once, after the window and
        // renderer exist and before the first tick.
        // engine:  Engine services.
        // Returns: the new state, owned by Game until shutdown(), or an error to abort the run.
        Result<GameState*> (*init)(EngineContext& engine){nullptr};

        // Advances the simulation by one fixed tick. Never read the wall clock here.
        // state:  the state from init().
        // engine: Engine services.
        // tick:   tick number, length and input.
        void (*fixedTick)(GameState& state, EngineContext& engine, const TickInfo& tick){nullptr};

        // Presents the current state: camera, draw submissions, local-only effects. Called once
        // per frame, after that frame's ticks and before rendering.
        // state:  the state from init().
        // engine: Engine services.
        // frame:  interpolation alpha, frame time and input.
        void (*frame)(GameState& state, EngineContext& engine, const FrameInfo& frame){nullptr};

        // Draws Game's ImGui dev panels. Called only while the dev tools are visible, inside the
        // ImGui frame, after the frame's ticks. Optional; never called without LOCKSTEP_DEV_TOOLS.
        // state:  the state from init().
        // engine: Engine services.
        void (*devToolsPanels)(GameState& state, EngineContext& engine){nullptr};

        // Destroys the state. Called once when the run ends, also after an error, while the
        // window and renderer still exist.
        // state: the state from init().
        void (*shutdown)(GameState* state){nullptr};
    };
} //namespace lockstep::app

#endif //LOCKSTEP_APP_GAME_API_HPP
