#include "Lockstep/App/Run.hpp"

#include "Lockstep/Core/FixedTimestep.hpp"
#include "Lockstep/Debug/DevTools.hpp"
#include "Lockstep/Net/Network.hpp"
#include "Lockstep/Net/Replication.hpp"
#include "Lockstep/Net/Session.hpp"
#include "Lockstep/World/Entities.hpp"
#include "Lockstep/World/TimeDomains.hpp"

#include "Platform/InputAccess.hpp"

#include <chrono>
#include <utility>

namespace lockstep::app
{
    namespace
    {
        using Clock = std::chrono::steady_clock;

        Status validate(const RunDescription& description, const GameApi& game)
        {
            if (!(description.tickRate > 0.0) || description.maxTicksPerFrame == 0)
            {
                return Error{ErrorCode::InvalidArgument, "run: tickRate must be > 0 and maxTicksPerFrame >= 1"};
            }

            if (game.init == nullptr || game.fixedTick == nullptr || game.frame == nullptr || game.shutdown == nullptr)
            {
                return Error{ErrorCode::InvalidArgument, "run: GameApi init, fixedTick, frame and shutdown are required"};
            }

            return {};
        }

        platform::Input filterForGame(const platform::Window& window, const debug::DevTools& devTools)
        {
            platform::Input input = window.input();

            if (devTools.wantsKeyboard())
            {
                platform::InputAccess::hideKeyboard(input);
            }

            if (devTools.wantsMouse() && !window.isCursorCaptured())
            {
                platform::InputAccess::hideMouse(input);
            }

            return input;
        }

        Status startSession(net::Session& session, const net::SessionLaunch& launch)
        {
            switch (launch.role)
            {
                case net::Role::Host:
                {
                    return session.host(launch.port);
                }
                case net::Role::Client:
                {
                    return session.connect(launch.address);
                }
                case net::Role::None:
                {
                    break;
                }
            }

            return {};
        }

        struct Loop
        {
            const GameApi& game;
            GameState& state;
            EngineContext& engine;
            debug::DevTools& devTools;
            net::Network& network;
            net::Session& session;
            net::Replication& replication;
            FixedTimestep timestep;
            platform::Input tickInput{};
            uint64_t ticksCompleted{0};
        };

        void runTicks(Loop& loop, uint32_t tickCount)
        {
            const float tickSeconds = loop.timestep.tickSeconds();

            for (uint32_t index = 0; index < tickCount; ++index)
            {
                loop.engine.timeDomains.advance(tickSeconds);
                loop.engine.entities.beginTick();
                loop.game.fixedTick(loop.state, loop.engine,
                    TickInfo{.tick = loop.ticksCompleted, .tickSeconds = tickSeconds, .input = loop.tickInput});
                loop.replication.sendTick(loop.ticksCompleted);
                loop.session.pump(loop.ticksCompleted);
                loop.replication.receiveTick();
                ++loop.ticksCompleted;
                platform::InputAccess::clearTransient(loop.tickInput);
            }
        }

        Status runFrames(Loop& loop)
        {
            platform::Window& window = loop.engine.window;
            Clock::time_point previousTime = Clock::now();

            while (window.isOpen())
            {
                const Clock::time_point now = Clock::now();
                const double frameSeconds = std::chrono::duration<double>(now - previousTime).count();
                previousTime = now;

                window.pollEvents();
                loop.devTools.beginFrame(static_cast<float>(frameSeconds));

                const platform::Input frameInput = filterForGame(window, loop.devTools);
                platform::InputAccess::accumulate(loop.tickInput, frameInput);

                runTicks(loop, loop.timestep.advance(frameSeconds));
                loop.devTools.drawNetPanel(loop.network, loop.session, loop.replication);

                if (loop.devTools.isVisible() && loop.game.devToolsPanels != nullptr)
                {
                    loop.game.devToolsPanels(loop.state, loop.engine);
                }

                loop.game.frame(loop.state, loop.engine, FrameInfo{
                    .ticksCompleted = loop.ticksCompleted,
                    .alpha = loop.timestep.alpha(),
                    .frameSeconds = static_cast<float>(frameSeconds),
                    .input = frameInput});

                if (auto rendered = loop.engine.renderer.render(); !rendered)
                {
                    return rendered;
                }
            }

            return {};
        }
    } //namespace

    Status run(const RunDescription& description, const GameApi& game)
    {
        if (auto valid = validate(description, game); !valid)
        {
            return valid;
        }

        auto window = platform::Window::create(description.window);

        if (!window)
        {
            return window.error();
        }

        auto renderer = render::Renderer::create(window.value(), description.renderer);

        if (!renderer)
        {
            return renderer.error();
        }

        auto devTools = debug::DevTools::create(renderer.value(), window.value());

        if (!devTools)
        {
            return devTools.error();
        }

        auto network = net::Network::create();

        if (!network)
        {
            return network.error();
        }

        net::Session session(network.value());

        if (auto started = startSession(session, description.session); !started)
        {
            return started;
        }

        world::TimeDomains timeDomains;
        world::Entities entities;
        net::Replication replication(session, entities, timeDomains);
        EngineContext engine{
            .window = window.value(),
            .renderer = renderer.value(),
            .timeDomains = timeDomains,
            .entities = entities,
            .session = session,
            .replication = replication};

        auto state = game.init(engine);

        if (!state)
        {
            return state.error();
        }

        if (state.value() == nullptr)
        {
            return Error{ErrorCode::InvalidArgument, "run: GameApi::init returned a null state"};
        }

        Loop loop{
            .game = game,
            .state = *state.value(),
            .engine = engine,
            .devTools = devTools.value(),
            .network = network.value(),
            .session = session,
            .replication = replication,
            .timestep = FixedTimestep(description.tickRate, description.maxTicksPerFrame)};

        Status result = runFrames(loop);
        game.shutdown(state.value());

        return result;
    }
} //namespace lockstep::app
