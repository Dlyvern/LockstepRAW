#include "Characters/AnimDrill.hpp"

#include "Characters/PawnCharacters.hpp"
#include "Npcs/NpcPresentation.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace game
{
    namespace
    {
        constexpr std::string_view k_drillFlag{"--dev-anim-drill"};
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_standingHeight{1.8f};
        constexpr float k_crouchedHeight{1.2f};
        constexpr float k_rayLift{1.0f};
        constexpr float k_rayLength{6.0f};
        constexpr uint32_t k_firstEntityIndex{0x7FFFFF00u};

        struct DrillKey
        {
            float time{0.0f};
            float right{0.0f};
            float forward{0.0f};
            float yaw{0.0f};
            bool isCrouched{false};
        };

        constexpr std::array<DrillKey, 21> k_routine{{{0.0f, 0.0f, 0.0f, 0.0f, false},
            {1.5f, 0.0f, 0.0f, 0.0f, false},
            {4.5f, 0.0f, 3.0f, 0.0f, false},
            {5.5f, 0.0f, 3.0f, 0.0f, false},
            {8.0f, 0.0f, 1.0f, 0.0f, false},
            {8.5f, 0.0f, 1.0f, 0.0f, false},
            {10.0f, 0.6f, 1.0f, 0.0f, false},
            {11.5f, -0.6f, 1.0f, 0.0f, false},
            {13.0f, 0.0f, 2.2f, 0.0f, false},
            {13.5f, 0.0f, 2.2f, 0.0f, false},
            {13.9f, 0.0f, 2.2f, 1.4f, false},
            {16.5f, 0.0f, 2.2f, 1.4f, false},
            {17.0f, 0.0f, 2.2f, -0.6f, false},
            {19.5f, 0.0f, 2.2f, -0.6f, false},
            {21.5f, 0.0f, 2.2f, 0.0f, false},
            {23.5f, 0.0f, 0.0f, 0.0f, false},
            {24.0f, 0.0f, 0.0f, 0.0f, true},
            {27.0f, 0.0f, 2.5f, 0.0f, true},
            {28.0f, 0.0f, 2.5f, 0.0f, true},
            {30.5f, 0.0f, 0.0f, 0.0f, true},
            {k_animDrillSeconds, 0.0f, 0.0f, 0.0f, false}}};

        struct DrillPose
        {
            float right{0.0f};
            float forward{0.0f};
            float yaw{0.0f};
            bool isCrouched{false};
        };

        float smoothstep(float value)
        {
            const float t = std::clamp(value, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        DrillPose routinePose(float seconds)
        {
            for (std::size_t index = 0; index + 1 < k_routine.size(); ++index)
            {
                const DrillKey& from = k_routine[index];
                const DrillKey& to = k_routine[index + 1];

                if (seconds < to.time)
                {
                    const float u = smoothstep((seconds - from.time) / std::max(to.time - from.time, 0.001f));
                    return DrillPose{.right = std::lerp(from.right, to.right, u),
                        .forward = std::lerp(from.forward, to.forward, u),
                        .yaw = std::lerp(from.yaw, to.yaw, u),
                        .isCrouched = from.isCrouched};
                }
            }

            return DrillPose{};
        }

        std::optional<AnimDrillSetup> parseSetup(std::span<const char* const> arguments)
        {
            for (std::size_t index = 0; index + 1 < arguments.size(); ++index)
            {
                if (arguments[index] == nullptr || arguments[index + 1] == nullptr
                    || std::string_view(arguments[index]) != k_drillFlag)
                {
                    continue;
                }

                AnimDrillSetup setup{};

                if (std::sscanf(arguments[index + 1], "%f,%f,%f,%f,%f", &setup.origin.x, &setup.origin.y,
                        &setup.origin.z, &setup.yaw, &setup.spacing) >= 4)
                {
                    return setup;
                }
            }

            return std::nullopt;
        }

        float groundHeight(const lockstep::physics::PhysicsWorld& physics, const math::Vec3& point, float fallback)
        {
            const auto hit = physics.raycast(math::Vec3{point.x, fallback + k_rayLift, point.z}, -k_up, k_rayLength,
                lockstep::physics::RaycastIgnore{.allControllers = true, .dynamicBodies = true});
            return hit ? hit->point.y : fallback;
        }
    } //namespace

    void presentAnimDrill(AnimDrill& drill, std::span<const char* const> arguments, lockstep::render::Renderer& renderer,
        lockstep::physics::PhysicsWorld& physics, NpcLooks& looks, PawnCharacters* characters,
        const Visuals& visuals, float seconds)
    {
        if (!drill.isParsed)
        {
            drill.setup = parseSetup(arguments);
            drill.isParsed = true;
        }

        if (!drill.setup || characters == nullptr)
        {
            return;
        }

        drill.seconds = std::fmod(drill.seconds + std::max(seconds, 0.0f), k_animDrillSeconds);
        const AnimDrillSetup& setup = *drill.setup;
        const DrillPose pose = routinePose(drill.seconds);
        const math::Vec3 forward{-std::sin(setup.yaw), 0.0f, -std::cos(setup.yaw)};
        const math::Vec3 right{std::cos(setup.yaw), 0.0f, -std::sin(setup.yaw)};
        const float height = pose.isCrouched ? k_crouchedHeight : k_standingHeight;

        for (uint32_t index = 0; index < 3; ++index)
        {
            const bool isSquad = index == 0;
            const float side = (static_cast<float>(index) - 1.0f) * setup.spacing;
            math::Vec3 foot = setup.origin + right * (pose.right + side) + forward * pose.forward;
            foot.y = groundHeight(physics, foot, setup.origin.y);
            const lockstep::world::EntityId entity{.index = k_firstEntityIndex + index, .generation = 1};
            const math::Transform transform{.position = foot + k_up * (0.5f * height),
                .rotation = math::angleAxis(setup.yaw + pose.yaw, k_up),
                .scale = math::Vec3{1.0f, height, 1.0f}};

            if (index == 2)
            {
                drawPawn(renderer, &physics, visuals, characters, lockstep::world::EntityOwner::Player2, transform,
                    PawnLook{.isArmed = true,
                        .crouch = pose.isCrouched ? 1.0f : 0.0f,
                        .weapon = WeaponKind::Glock},
                    seconds);
                continue;
            }

            const NpcView view{.entity = entity,
                .isAlert = isSquad,
                .gearLook = isSquad ? GearLook::Full : GearLook::Balaclava,
                .body = isSquad ? NpcBody::Swat : NpcBody::Robber};
            drawNpc(renderer, physics, looks, characters, visuals, isSquad ? Archetype::Squad : Archetype::Merc, entity,
                transform, view, 1.0f, seconds);
        }
    }
} //namespace game
