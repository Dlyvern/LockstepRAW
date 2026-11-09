#include "Presentation/Visuals.hpp"

#include "Presentation/Lighting.hpp"

#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/Render/Shapes.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;
        namespace world = lockstep::world;

        using Colour = std::array<uint8_t, 3>;

        constexpr uint32_t k_checkerSize{256};
        constexpr float k_aimFovDegrees{52.0f};
        constexpr uint32_t k_checkerCellSize{16};
        constexpr uint32_t k_solidSize{4};
        constexpr float k_crosshairDistance{0.5f};
        constexpr float k_crosshairSize{0.004f};
        constexpr int32_t k_healthPips{10};
        constexpr float k_healthPipSize{0.01f};
        constexpr float k_healthPipSpacing{0.014f};
        constexpr float k_healthRowDrop{0.2f};
        constexpr math::Vec3 k_worldUp{0.0f, 1.0f, 0.0f};

        constexpr Colour k_floorLight{120, 118, 110};
        constexpr Colour k_floorDark{40, 38, 44};
        constexpr Colour k_player1Colour{60, 120, 230};
        constexpr Colour k_player2Colour{70, 200, 90};
        constexpr Colour k_enemyColour{200, 30, 30};
        constexpr Colour k_crosshairColour{255, 255, 255};
        constexpr Colour k_solidColour{120, 117, 112};
        constexpr Colour k_healthFullColour{220, 40, 40};
        constexpr Colour k_healthEmptyColour{35, 35, 40};
        constexpr Colour k_promptColour{255, 210, 120};
        constexpr Colour k_ghostColour{170, 200, 255};
        constexpr float k_promptMarkSize{0.003f};
        constexpr float k_promptMarkOffset{0.012f};

        constexpr render::MaterialDescription k_floorLook{.specular = 0.35f, .shininess = 40.0f};
        constexpr render::MaterialDescription k_concreteLook{.specular = 0.12f, .shininess = 12.0f};
        constexpr render::MaterialDescription k_pawnLook{.specular = 0.3f, .shininess = 32.0f, .emissive = 0.02f};
        constexpr render::MaterialDescription k_enemyLook{.specular = 0.4f, .shininess = 48.0f, .emissive = 0.3f};
        constexpr render::MaterialDescription k_hudLook{.shading = render::Shading::Unlit};
        constexpr render::MaterialDescription k_ghostLook{.specular = 0.1f, .shininess = 8.0f, .emissive = 0.6f};

        void appendPixel(std::vector<std::byte>& pixels, const Colour& colour)
        {
            pixels.insert(pixels.end(), {std::byte{colour[0]}, std::byte{colour[1]}, std::byte{colour[2]}, std::byte{255}});
        }

        lockstep::Result<render::MaterialHandle> createMaterial(render::Renderer& renderer,
            std::span<const std::byte> pixels, uint32_t size, render::TextureFilter filter,
            render::MaterialDescription look)
        {
            auto texture = renderer.createTexture({.pixels = pixels, .width = size, .height = size, .filter = filter});

            if (!texture)
            {
                return texture.error();
            }

            look.albedo = texture.value();

            return renderer.createMaterial(look);
        }

        lockstep::Result<render::MaterialHandle> createCheckerMaterial(render::Renderer& renderer)
        {
            std::vector<std::byte> pixels;
            pixels.reserve(static_cast<std::size_t>(k_checkerSize) * k_checkerSize * 4);

            for (uint32_t y = 0; y < k_checkerSize; ++y)
            {
                for (uint32_t x = 0; x < k_checkerSize; ++x)
                {
                    const bool light = ((x / k_checkerCellSize) + (y / k_checkerCellSize)) % 2 == 0;
                    appendPixel(pixels, light ? k_floorLight : k_floorDark);
                }
            }

            return createMaterial(renderer, pixels, k_checkerSize, render::TextureFilter::Nearest, k_floorLook);
        }

        lockstep::Result<render::MaterialHandle> createSolidMaterial(render::Renderer& renderer, const Colour& colour,
            const render::MaterialDescription& look)
        {
            std::vector<std::byte> pixels;
            pixels.reserve(static_cast<std::size_t>(k_solidSize) * k_solidSize * 4);

            for (uint32_t pixel = 0; pixel < k_solidSize * k_solidSize; ++pixel)
            {
                appendPixel(pixels, colour);
            }

            return createMaterial(renderer, pixels, k_solidSize, render::TextureFilter::Nearest, look);
        }
    } //namespace

    lockstep::Result<Visuals> createVisuals(render::Renderer& renderer)
    {
        const render::MeshData cubeData = render::shapes::cube();
        auto cube = renderer.createMesh(cubeData.vertices, cubeData.indices);

        if (!cube)
        {
            return cube.error();
        }

        auto floor = createCheckerMaterial(renderer);

        if (!floor)
        {
            return floor.error();
        }

        Visuals visuals{.cube = cube.value(), .floor = floor.value()};
        const std::array<std::tuple<render::MaterialHandle*, Colour, render::MaterialDescription>, 9> solids{{
            {&visuals.solid, k_solidColour, k_concreteLook},
            {&visuals.player1, k_player1Colour, k_pawnLook},
            {&visuals.player2, k_player2Colour, k_pawnLook},
            {&visuals.enemy, k_enemyColour, k_enemyLook},
            {&visuals.crosshair, k_crosshairColour, k_hudLook},
            {&visuals.healthFull, k_healthFullColour, k_hudLook},
            {&visuals.healthEmpty, k_healthEmptyColour, k_hudLook},
            {&visuals.prompt, k_promptColour, k_hudLook},
            {&visuals.ghost, k_ghostColour, k_ghostLook}}};

        for (const auto& [material, colour, look] : solids)
        {
            auto created = createSolidMaterial(renderer, colour, look);

            if (!created)
            {
                return created.error();
            }

            *material = created.value();
        }

        return visuals;
    }

    void drawArena(render::Renderer& renderer, const Visuals& visuals, const Arena& arena)
    {
        for (const ArenaBox& box : arena.boxes)
        {
            renderer.submit(visuals.cube, box.surface == ArenaSurface::Floor ? visuals.floor : visuals.solid,
                arenaBoxTransform(box));
        }
    }

    void drawEntity(render::Renderer& renderer, const Visuals& visuals, Archetype archetype, world::EntityOwner owner,
        const math::Transform& transform)
    {
        switch (archetype)
        {
            case Archetype::Pawn:
                renderer.submit(visuals.cube,
                    owner == world::EntityOwner::Player2 ? visuals.player2 : visuals.player1, transform);
                break;
            case Archetype::Enemy:
                renderer.submit(visuals.cube, visuals.enemy, transform);
                break;
            case Archetype::Toy:
            case Archetype::None:
                break;
        }
    }

    void drawCrosshair(render::Renderer& renderer, const Visuals& visuals, const math::Vec3& eye,
        const math::Vec3& direction)
    {
        renderer.submit(visuals.cube, visuals.crosshair, {
            .position = eye + direction * k_crosshairDistance,
            .scale = {k_crosshairSize, k_crosshairSize, k_crosshairSize}});
    }

    void drawInteractPrompt(render::Renderer& renderer, const Visuals& visuals, const math::Vec3& eye,
        const math::Vec3& direction)
    {
        const math::Vec3 right = math::normalize(math::cross(direction, k_worldUp));
        const math::Vec3 up = math::cross(right, direction);
        const math::Vec3 centre = eye + direction * k_crosshairDistance;
        const std::array<math::Vec3, 4> offsets{right, -right, up, -up};

        for (const math::Vec3& offset : offsets)
        {
            renderer.submit(visuals.cube, visuals.prompt, {
                .position = centre + offset * k_promptMarkOffset,
                .scale = {k_promptMarkSize, k_promptMarkSize, k_promptMarkSize}});
        }
    }

    void drawHealth(render::Renderer& renderer, const Visuals& visuals, const math::Vec3& eye,
        const math::Vec3& direction, const PlayerStatus& status, int32_t maxHealth)
    {
        const math::Vec3 right = math::normalize(math::cross(direction, k_worldUp));
        const math::Vec3 up = math::cross(right, direction);
        const math::Vec3 rowCentre = eye + direction * k_crosshairDistance - up * k_healthRowDrop;
        const int32_t health = status.isDead ? 0 : status.health;
        const int32_t filled = maxHealth > 0 ? (health * k_healthPips + maxHealth - 1) / maxHealth : 0;
        const math::Quat facing = math::fromRotationMatrix(math::inverse(math::lookAt(eye, eye + direction, up)));

        for (int32_t pip = 0; pip < k_healthPips; ++pip)
        {
            const float offset = (static_cast<float>(pip) - static_cast<float>(k_healthPips - 1) * 0.5f)
                * k_healthPipSpacing;
            renderer.submit(visuals.cube, pip < filled ? visuals.healthFull : visuals.healthEmpty, {
                .position = rowCentre + right * offset,
                .rotation = facing,
                .scale = {k_healthPipSize, k_healthPipSize, k_healthPipSize}});
        }
    }

    render::Camera firstPersonCamera(const math::Vec3& eye, const math::Vec3& direction, float zoom)
    {
        render::Camera camera{.position = eye, .target = eye + direction};
        camera.verticalFov += (math::radians(k_aimFovDegrees) - camera.verticalFov) * math::clamp(zoom, 0.0f, 1.0f);

        return camera;
    }
} //namespace game
