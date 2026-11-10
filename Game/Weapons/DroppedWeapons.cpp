#include "Weapons/DroppedWeapons.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <array>
#include <limits>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace physics = lockstep::physics;

        constexpr std::string_view k_handJoint{"RightHand"};
        constexpr float k_hullBack{-0.014f};
        constexpr float k_hullFront{0.059f};
        constexpr std::array<math::Vec2, 9> k_hullOutline{{
            {-0.222f, 0.222f}, {-0.100f, -0.212f}, {0.040f, -0.216f}, {0.077f, -0.099f}, {0.136f, 0.099f},
            {0.149f, 0.400f}, {0.100f, 0.463f}, {0.050f, 0.470f}, {-0.063f, 0.381f}}};
        constexpr float k_weaponDensity{300.0f};
        constexpr float k_friction{0.7f};
        constexpr float k_restitution{0.1f};
        constexpr float k_boundsMargin{0.05f};

        bool isJointNamed(std::string_view name, std::string_view wanted)
        {
            if (!name.ends_with(wanted))
            {
                return false;
            }

            const std::size_t before = name.size() - wanted.size();
            return before == 0 || name[before - 1] == ':' || name[before - 1] == '_';
        }

        math::Vec3 place(const math::Transform& transform, const math::Vec3& point)
        {
            const math::Vec3 scaled{point.x * transform.scale.x, point.y * transform.scale.y, point.z * transform.scale.z};
            return transform.position + transform.rotation * scaled;
        }

        void removeOldest(DroppedWeapons& weapons, physics::PhysicsWorld& physics)
        {
            releasePiece(physics, weapons.weapons.front());
            weapons.weapons.erase(weapons.weapons.begin());
        }
    } //namespace

    std::optional<HeldWeapon> findHeldWeapon(const anim::SkinnedModel& model)
    {
        HeldWeapon weapon{};

        for (std::size_t index = 0; index < model.parts.size() && index < std::numeric_limits<anim::PartMask>::digits;
             ++index)
        {
            if (model.parts[index].materialName == k_heldWeaponMaterial)
            {
                weapon.parts |= anim::PartMask{1} << index;
            }
        }

        for (std::size_t index = 0; index < model.skeleton.joints.size(); ++index)
        {
            if (isJointNamed(model.skeleton.joints[index].name, k_handJoint))
            {
                weapon.hand = static_cast<uint32_t>(index);
                break;
            }
        }

        if (weapon.parts == 0 || weapon.hand == anim::k_noJoint)
        {
            return std::nullopt;
        }

        weapon.limb = anim::jointLimb(model, weapon.hand);
        return weapon;
    }

    lockstep::Status dropHeldWeapon(DroppedWeapons& weapons, physics::PhysicsWorld& physics, const WeaponDrop& drop)
    {
        const std::span<const math::Mat4> joints = drop.instance.jointMatrices();
        const std::span<const math::Mat4> skin = drop.instance.skinMatrices();

        if (drop.weapon.hand >= joints.size() || skin.size() != joints.size())
        {
            return lockstep::Error{lockstep::ErrorCode::InvalidArgument, "the character has no pose to drop from"};
        }

        SeveredPiece piece{.model = &drop.model,
            .skin = std::vector<math::Mat4>(skin.begin(), skin.end()),
            .parts = drop.weapon.parts,
            .modelStart = drop.transform};
        std::vector<math::Vec3> world;
        math::Vec3 centre{0.0f, 0.0f, 0.0f};

        for (const math::Vec2& outline : k_hullOutline)
        {
            for (const float depth : {k_hullBack, k_hullFront})
            {
                const math::Vec3 local = math::transformPoint(joints[drop.weapon.hand],
                    math::Vec3{outline.x, outline.y, depth});
                math::expand(piece.bounds, local);
                world.push_back(place(drop.transform, local));
                centre += world.back() / static_cast<float>(k_hullOutline.size() * 2);
            }
        }

        piece.bounds.min -= math::Vec3{k_boundsMargin, k_boundsMargin, k_boundsMargin};
        piece.bounds.max += math::Vec3{k_boundsMargin, k_boundsMargin, k_boundsMargin};

        for (math::Vec3& point : world)
        {
            point -= centre;
        }

        auto body = physics.createBody(physics::BodyDescription{.shape = physics::ConvexShape{.points = std::move(world)},
            .type = physics::BodyType::Dynamic,
            .transform = math::Transform{.position = centre},
            .density = k_weaponDensity,
            .friction = k_friction,
            .restitution = k_restitution,
            .isDebris = true});

        if (!body)
        {
            return body.error();
        }

        piece.body = body.value();
        piece.bodyStart = math::Transform{.position = centre};
        piece.previous = piece.bodyStart;
        piece.current = piece.bodyStart;
        piece.cut = math::center(piece.bounds);
        physics.setVelocity(piece.body, drop.linear, drop.angular);

        while (!weapons.weapons.empty() && weapons.weapons.size() >= weapons.tuning.maxWeapons)
        {
            removeOldest(weapons, physics);
        }

        if (weapons.tuning.maxWeapons == 0)
        {
            releasePiece(physics, piece);
            return {};
        }

        weapons.weapons.push_back(std::move(piece));
        return {};
    }

    void presentDroppedWeapons(DroppedWeapons& weapons, physics::PhysicsWorld& physics,
        lockstep::render::Renderer& renderer, float alpha, float seconds)
    {
        std::erase_if(weapons.weapons, [&](SeveredPiece& weapon)
        {
            weapon.seconds += seconds;

            if (weapon.seconds > weapons.tuning.lifeSeconds || !physics.isAlive(weapon.body))
            {
                releasePiece(physics, weapon);
                return true;
            }

            trackPiece(weapon, physics);
            return false;
        });

        for (const SeveredPiece& weapon : weapons.weapons)
        {
            drawPiece(renderer, weapon, alpha);
        }
    }

    void clearDroppedWeapons(DroppedWeapons& weapons, physics::PhysicsWorld& physics)
    {
        for (SeveredPiece& weapon : weapons.weapons)
        {
            releasePiece(physics, weapon);
        }

        weapons.weapons.clear();
    }
} //namespace game
