#include "Lockstep/App/Run.hpp"

#include "Lockstep/Assets/FileSystem.hpp"
#include "Lockstep/Audio/AudioSystem.hpp"
#include "Lockstep/Core/Log.hpp"
#include "Lockstep/Core/FixedTimestep.hpp"
#include "Lockstep/Debug/DevTools.hpp"
#include "Lockstep/Net/Network.hpp"
#include "Lockstep/Net/Replication.hpp"
#include "Lockstep/Net/Session.hpp"
#include "Lockstep/Physics/PhysicsWorld.hpp"
#include "Lockstep/World/Entities.hpp"
#include "Lockstep/World/TimeDomains.hpp"

#include "Core/VelaLog.hpp"
#include "Platform/InputAccess.hpp"
#include "Render/RendererState.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace lockstep::app
{
    namespace
    {
        using Clock = std::chrono::steady_clock;

        constexpr std::string_view k_gpuTimingsFlag{"--dev-gpu-timings"};
        constexpr uint64_t k_memoryReportFrame{30};
        constexpr uint64_t k_bytesPerMebibyte{1024 * 1024};

        float toMilliseconds(Clock::duration duration)
        {
            return std::chrono::duration<float, std::milli>(duration).count();
        }

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

        Result<assets::FileSystem> mountContent(const std::vector<std::filesystem::path>& content)
        {
            assets::FileSystem files;

            for (const std::filesystem::path& source : content)
            {
                if (auto mounted = files.mount(source); !mounted)
                {
                    return mounted.error();
                }
            }

            for (const assets::MountInfo& mount : files.mounts())
            {
                if (mount.kind == assets::MountKind::Directory)
                {
                    log(LogLevel::Info, "Assets", "mounted folder '" + mount.source.string() + "'");
                    continue;
                }

                char hash[17]{};
                std::snprintf(hash, sizeof(hash), "%016llx", static_cast<unsigned long long>(mount.contentHash));
                log(LogLevel::Info, "Assets", "mounted pak '" + mount.source.string() + "': "
                    + std::to_string(mount.fileCount) + " files, content hash " + hash);
            }

            return files;
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

        audio::ListenerPose cameraListener(render::Renderer& renderer)
        {
            const render::Camera& camera = render::rendererState(renderer).camera;

            return audio::ListenerPose{.position = camera.position, .forward = camera.target - camera.position, .up = camera.up};
        }

        void logGpuMemory(const render::Renderer& renderer, std::string_view moment)
        {
            const render::GpuMemoryStats memory = renderer.memoryStats();
            const render::GraphicsQuality& quality = renderer.graphicsQuality();
            log(LogLevel::Info, "Render", "GPU memory " + std::string(moment) + " (graphics "
                + render::toString(quality.preset) + "): " + std::to_string(memory.deviceBytesUsed / k_bytesPerMebibyte)
                + " MiB used of " + std::to_string(memory.deviceBytesBudget / k_bytesPerMebibyte) + " MiB budget ("
                + (memory.isBudgetFromDriver ? "driver" : "estimated") + "), "
                + std::to_string(memory.deviceBytesAllocated / k_bytesPerMebibyte) + " MiB allocated, "
                + std::to_string(memory.textures) + " textures, " + std::to_string(memory.meshes) + " meshes");
        }

        void updateAudio(EngineContext& engine, float frameSeconds)
        {
            engine.audio.setBusTimeScale(audio::Bus::World, engine.timeDomains.scale(world::TimeDomain::World));
            engine.audio.update(frameSeconds, cameraListener(engine.renderer));
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
                loop.engine.physics.step(loop.engine.timeDomains.delta(world::TimeDomain::World));
                loop.game.fixedTick(loop.state, loop.engine,
                    TickInfo{.tick = loop.ticksCompleted, .tickSeconds = tickSeconds, .input = loop.tickInput});
                loop.replication.sendTick(loop.ticksCompleted);
                loop.session.pump(loop.ticksCompleted);
                loop.replication.receiveTick();
                ++loop.ticksCompleted;
                platform::InputAccess::clearTransient(loop.tickInput);
            }
        }

        void waitForFrameSlot(Clock::time_point& deadline, uint32_t framesPerSecond)
        {
            const Clock::time_point now = Clock::now();

            if (framesPerSecond == 0)
            {
                deadline = now;
                return;
            }

            const auto period = std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(1.0 / static_cast<double>(framesPerSecond)));
            deadline = std::max(deadline + period, now - period);

            if (deadline > now)
            {
                std::this_thread::sleep_until(deadline);
            }
        }

        Status runFrames(Loop& loop)
        {
            platform::Window& window = loop.engine.window;
            Clock::time_point previousTime = Clock::now();
            Clock::time_point frameDeadline = previousTime;
            uint64_t framesRendered = 0;

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
                loop.devTools.drawPhysicsSection(loop.engine.physics);
                loop.devTools.drawAudioPanel(loop.engine.audio);

                if (loop.devTools.isVisible() && loop.game.devToolsPanels != nullptr)
                {
                    loop.game.devToolsPanels(loop.state, loop.engine);
                }

                loop.game.frame(loop.state, loop.engine, FrameInfo{
                    .ticksCompleted = loop.ticksCompleted,
                    .alpha = loop.timestep.alpha(),
                    .frameSeconds = static_cast<float>(frameSeconds),
                    .input = frameInput});
                updateAudio(loop.engine, static_cast<float>(frameSeconds));

                const Clock::time_point renderStart = Clock::now();

                if (auto rendered = loop.engine.renderer.render(); !rendered)
                {
                    return rendered;
                }

                if (++framesRendered == k_memoryReportFrame)
                {
                    logGpuMemory(loop.engine.renderer, "after " + std::to_string(k_memoryReportFrame) + " frames");
                }

                loop.devTools.recordFrameTiming(debug::FrameTiming{
                    .workMilliseconds = toMilliseconds(renderStart - now),
                    .renderMilliseconds = toMilliseconds(Clock::now() - renderStart)});
                waitForFrameSlot(frameDeadline, loop.engine.renderer.frameRateLimit());
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

        routeVelaLogs();

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

        logGpuMemory(renderer.value(), "at startup");

        auto devTools = debug::DevTools::create(renderer.value(), window.value());

        if (!devTools)
        {
            return devTools.error();
        }

        devTools.value().logGpuTimings(std::any_of(description.arguments.begin(), description.arguments.end(),
            [](const char* argument)
            {
                return argument != nullptr && std::string_view(argument) == k_gpuTimingsFlag;
            }));

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

        auto physics = physics::PhysicsWorld::create();

        if (!physics)
        {
            return physics.error();
        }

        auto files = mountContent(description.content);

        if (!files)
        {
            return files.error();
        }

        auto audioSystem = audio::AudioSystem::create();

        if (!audioSystem)
        {
            return audioSystem.error();
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
            .replication = replication,
            .physics = physics.value(),
            .files = files.value(),
            .audio = audioSystem.value(),
            .arguments = description.arguments};

        auto state = game.init(engine);

        if (!state)
        {
            return state.error();
        }

        if (state.value() == nullptr)
        {
            return Error{ErrorCode::InvalidArgument, "run: GameApi::init returned a null state"};
        }

        logGpuMemory(renderer.value(), "after Game init");

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
