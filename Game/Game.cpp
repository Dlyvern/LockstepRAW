#include "Game.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Platform/Input.hpp>
#include <Lockstep/Platform/Window.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/Render/Shapes.hpp>
#include <Lockstep/World/Entities.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#if LOCKSTEP_DEV_TOOLS
    #include <imgui.h>
#endif

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace lockstep::app
{
    struct GameState
    {
        struct Resources
        {
            render::MeshHandle cube{};
            render::MaterialHandle material{};
        };

        struct Settings
        {
            float cubeTurnRate{1.0f};
            float worldScale{1.0f};
            float bulletTimeScale{0.2f};
        };

        struct Visual
        {
            world::EntityId entity{};
            render::MeshHandle mesh{};
            render::MaterialHandle material{};
        };

        struct Simulation
        {
            world::EntityId cube{};
            float cubeAngle{0.0f};
            uint64_t lastTick{0};
            bool isLocalBulletTime{false};
            bool isRemoteBulletTime{false};
        };

        struct Presentation
        {
            float cameraYaw{0.0f};
            float cameraPitch{0.32f};
            float cameraDistance{3.16f};
        };

        Resources resources;
        Settings settings;
        Simulation simulation;
        Presentation presentation;
        std::vector<Visual> visuals;
    };
} //namespace lockstep::app

namespace game
{
    namespace
    {
        namespace math = lockstep::math;
        namespace net = lockstep::net;
        namespace platform = lockstep::platform;
        namespace render = lockstep::render;
        namespace world = lockstep::world;

        using lockstep::app::EngineContext;
        using lockstep::app::FrameInfo;
        using lockstep::app::GameState;
        using lockstep::app::TickInfo;

        constexpr uint32_t k_checkerSize{256};
        constexpr uint32_t k_checkerCellSize{32};
        constexpr math::Vec3 k_cubeAxis{1.0f, 1.0f, 0.0f};
        constexpr float k_mouseLookSensitivity{0.003f};
        constexpr float k_maxCameraPitch{1.4f};
        constexpr uint16_t k_cubeArchetype{1};
        constexpr std::byte k_bulletTimeInputBit{0x01};

        std::vector<std::byte> makeCheckerboard()
        {
            std::vector<std::byte> pixels;
            pixels.reserve(static_cast<std::size_t>(k_checkerSize) * k_checkerSize * 4);

            for (uint32_t y = 0; y < k_checkerSize; ++y)
            {
                for (uint32_t x = 0; x < k_checkerSize; ++x)
                {
                    const bool light = ((x / k_checkerCellSize) + (y / k_checkerCellSize)) % 2 == 0;
                    const std::byte red = light ? std::byte{230} : std::byte{40};
                    const std::byte green = light ? std::byte{220} : std::byte{30};
                    const std::byte blue = light ? std::byte{200} : std::byte{60};

                    pixels.insert(pixels.end(), {red, green, blue, std::byte{255}});
                }
            }

            return pixels;
        }

        lockstep::Result<GameState::Resources> createResources(render::Renderer& renderer)
        {
            const render::MeshData cubeData = render::shapes::cube();
            auto cube = renderer.createMesh(cubeData.vertices, cubeData.indices);

            if (!cube)
            {
                return cube.error();
            }

            const std::vector<std::byte> checkerPixels = makeCheckerboard();
            auto checker = renderer.createTexture({
                .pixels = checkerPixels,
                .width = k_checkerSize,
                .height = k_checkerSize,
                .filter = render::TextureFilter::Nearest});

            if (!checker)
            {
                return checker.error();
            }

            auto material = renderer.createMaterial({.albedo = checker.value()});

            if (!material)
            {
                return material.error();
            }

            return GameState::Resources{.cube = cube.value(), .material = material.value()};
        }

        lockstep::Result<GameState*> init(EngineContext& engine)
        {
            auto resources = createResources(engine.renderer);

            if (!resources)
            {
                return resources.error();
            }

            auto state = std::make_unique<GameState>();
            state->resources = resources.value();

            if (engine.session.role() == net::Role::Client)
            {
                return state.release();
            }

            auto cube = engine.entities.create({.domain = world::TimeDomain::World});

            if (!cube)
            {
                return cube.error();
            }

            if (auto tagged = engine.replication.setArchetype(cube.value(), k_cubeArchetype); !tagged)
            {
                return tagged.error();
            }

            state->simulation.cube = cube.value();
            state->visuals.push_back({
                .entity = cube.value(),
                .mesh = state->resources.cube,
                .material = state->resources.material});

            return state.release();
        }

        bool isRemoteBulletTime(const net::Replication& replication)
        {
            const std::optional<net::RemoteInput> input = replication.remoteInput();
            return input && !input->bytes.empty() && (input->bytes.front() & k_bulletTimeInputBit) != std::byte{0};
        }

        void updateBulletTime(GameState& state, EngineContext& engine, const TickInfo& tick)
        {
            GameState::Simulation& simulation = state.simulation;
            simulation.isLocalBulletTime = tick.input.isKeyDown(platform::Key::Space);

            if (engine.session.role() == net::Role::Client)
            {
                const std::byte input = simulation.isLocalBulletTime ? k_bulletTimeInputBit : std::byte{0};

                if (auto sent = engine.replication.setLocalInput(std::span(&input, 1)); !sent)
                {
                    std::clog << "Game: " << sent.error().message << '\n';
                }

                return;
            }

            simulation.isRemoteBulletTime = isRemoteBulletTime(engine.replication);
            const bool isBulletTime = simulation.isLocalBulletTime || simulation.isRemoteBulletTime;
            engine.timeDomains.setScale(world::TimeDomain::World,
                isBulletTime ? state.settings.bulletTimeScale : state.settings.worldScale);
        }

        void fixedTick(GameState& state, EngineContext& engine, const TickInfo& tick)
        {
            updateBulletTime(state, engine, tick);

            GameState::Simulation& simulation = state.simulation;
            simulation.lastTick = tick.tick;

            world::Entity* cube = engine.entities.find(simulation.cube);

            if (cube == nullptr)
            {
                return;
            }

            simulation.cubeAngle += state.settings.cubeTurnRate * engine.timeDomains.delta(cube->domain());
            simulation.cubeAngle = std::fmod(simulation.cubeAngle, 2.0f * math::k_pi);
            cube->transform().rotation = math::angleAxis(simulation.cubeAngle, k_cubeAxis);
        }

        void updateCursorCapture(platform::Window& window, const platform::Input& input)
        {
            if (input.wasKeyPressed(platform::Key::Escape))
            {
                if (window.isCursorCaptured())
                {
                    window.setCursorCaptured(false);
                }
                else
                {
                    window.close();
                }
            }
            else if (input.wasMouseButtonPressed(platform::MouseButton::Left))
            {
                window.setCursorCaptured(true);
            }
        }

        render::Camera orbitCamera(GameState::Presentation& presentation, const platform::Input& input, bool isMouseLook)
        {
            if (isMouseLook)
            {
                const math::Vec2 delta = input.mouseDelta();
                presentation.cameraYaw -= delta.x * k_mouseLookSensitivity;
                presentation.cameraPitch = math::clamp(presentation.cameraPitch + delta.y * k_mouseLookSensitivity,
                    -k_maxCameraPitch, k_maxCameraPitch);
            }

            const float horizontal = std::cos(presentation.cameraPitch) * presentation.cameraDistance;
            const math::Vec3 position{horizontal * std::sin(presentation.cameraYaw),
                std::sin(presentation.cameraPitch) * presentation.cameraDistance,
                horizontal * std::cos(presentation.cameraYaw)};

            return render::Camera{.position = position, .target = {0.0f, 0.0f, 0.0f}};
        }

        void frame(GameState& state, EngineContext& engine, const FrameInfo& frameInfo)
        {
            updateCursorCapture(engine.window, frameInfo.input);

            engine.renderer.setCamera(orbitCamera(state.presentation, frameInfo.input, engine.window.isCursorCaptured()));

            for (const GameState::Visual& visual : state.visuals)
            {
                if (const world::Entity* entity = engine.entities.find(visual.entity))
                {
                    engine.renderer.submit(visual.mesh, visual.material, entity->interpolatedTransform(frameInfo.alpha));
                }
            }

            for (const net::MirroredEntity& mirrored : engine.replication.mirrored())
            {
                const world::Entity* entity = engine.entities.find(mirrored.entity);

                if (entity != nullptr && mirrored.archetype == k_cubeArchetype)
                {
                    engine.renderer.submit(state.resources.cube, state.resources.material,
                        entity->interpolatedTransform(frameInfo.alpha));
                }
            }
        }

        void shutdown(GameState* state)
        {
            std::unique_ptr<GameState> owned(state);
        }

#if LOCKSTEP_DEV_TOOLS
        constexpr float k_maxCubeTurnRate{10.0f};
        constexpr float k_maxWorldScale{2.0f};
        constexpr float k_panelMargin{10.0f};

        void devToolsPanels(GameState& state, EngineContext& engine)
        {
            const ImVec2 topRight{ImGui::GetIO().DisplaySize.x - k_panelMargin, k_panelMargin};
            ImGui::SetNextWindowPos(topRight, ImGuiCond_FirstUseEver, ImVec2{1.0f, 0.0f});

            if (ImGui::Begin("Game", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                const world::TimeDomains& domains = engine.timeDomains;

                ImGui::SliderFloat("Cube turn rate (rad/s)", &state.settings.cubeTurnRate, 0.0f, k_maxCubeTurnRate);
                ImGui::SliderFloat("World scale", &state.settings.worldScale, 0.0f, k_maxWorldScale);
                ImGui::SliderFloat("Bullet-time scale", &state.settings.bulletTimeScale, 0.0f, 1.0f);
                ImGui::Separator();
                ImGui::Text("Tick        %llu", static_cast<unsigned long long>(state.simulation.lastTick));
                ImGui::Text("Player      x%.2f  %.2f s", domains.scale(world::TimeDomain::Player),
                    domains.elapsed(world::TimeDomain::Player));
                ImGui::Text("World       x%.2f  %.2f s", domains.scale(world::TimeDomain::World),
                    domains.elapsed(world::TimeDomain::World));
                ImGui::Text("Entities    %zu", engine.entities.count());
                ImGui::Text("Bullet-time local %s, remote %s", state.simulation.isLocalBulletTime ? "on" : "off",
                    state.simulation.isRemoteBulletTime ? "on" : "off");
                ImGui::Separator();
                ImGui::TextDisabled("Hold Space for bullet-time (either player)");
                ImGui::TextDisabled("Click to capture the mouse, Esc releases it");
            }

            ImGui::End();
        }
#endif
    } //namespace

    lockstep::app::GameApi gameApi()
    {
        lockstep::app::GameApi api{};
        api.init = &init;
        api.fixedTick = &fixedTick;
        api.frame = &frame;
        api.shutdown = &shutdown;

#if LOCKSTEP_DEV_TOOLS
        api.devToolsPanels = &devToolsPanels;
#endif

        return api;
    }
} //namespace game
