#include "Gore/SeveredLimbs.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace physics = lockstep::physics;

        constexpr std::size_t k_minHullPoints{4};
        constexpr float k_boundsMargin{0.05f};
        constexpr float k_fleshDensity{1050.0f};
        constexpr float k_friction{0.9f};
        constexpr float k_restitution{0.05f};
        constexpr float k_fallbackRadius{0.08f};
        constexpr float k_parallelRay{1.0e-8f};
        constexpr float k_hitSpin{25.0f};

        anim::LimbMask hiddenLimbs(anim::LimbMask severed)
        {
            anim::LimbMask hidden = 0;

            for (std::size_t index = 0; index < anim::k_limbCount; ++index)
            {
                const auto limb = static_cast<anim::Limb>(index);

                if ((severed & anim::limbBit(limb)) != 0)
                {
                    hidden = static_cast<anim::LimbMask>(hidden | anim::limbSubtree(limb));
                }
            }

            return hidden;
        }

        anim::LimbMask outermostCuts(anim::LimbMask severed)
        {
            const anim::LimbMask hidden = hiddenLimbs(severed);
            anim::LimbMask outermost = 0;

            for (std::size_t index = 0; index < anim::k_limbCount; ++index)
            {
                const auto limb = static_cast<anim::Limb>(index);

                if ((severed & anim::limbBit(limb)) != 0 && (hidden & anim::limbBit(anim::limbParent(limb))) == 0)
                {
                    outermost = static_cast<anim::LimbMask>(outermost | anim::limbBit(limb));
                }
            }

            return outermost;
        }

        math::Vec3 place(const math::Transform& transform, const math::Vec3& point)
        {
            const math::Vec3 scaled{point.x * transform.scale.x, point.y * transform.scale.y, point.z * transform.scale.z};
            return transform.position + transform.rotation * scaled;
        }

        physics::BodyDescription debrisBody(physics::BodyShape shape, const math::Vec3& centre)
        {
            return physics::BodyDescription{.shape = std::move(shape),
                .type = physics::BodyType::Dynamic,
                .transform = math::Transform{.position = centre},
                .density = k_fleshDensity,
                .friction = k_friction,
                .restitution = k_restitution,
                .isDebris = true};
        }
    } //namespace

    anim::PartMask severedBodyParts(const anim::SkinnedModel& model, anim::PartMask gearParts, anim::LimbMask severed)
    {
        const anim::PartMask base = gearParts & ~anim::woundCapParts(model);

        if (severed == 0)
        {
            return base;
        }

        return (base & ~anim::limbParts(model, hiddenLimbs(severed), anim::LimbPartRole::Region))
            | anim::limbParts(model, outermostCuts(severed), anim::LimbPartRole::StumpCap);
    }

    bool canSever(const SeverSource& source, anim::Limb limb)
    {
        return anim::hasLimbs(source.model) && limb != anim::Limb::Torso && limb != anim::Limb::None
            && (hiddenLimbs(source.severed) & anim::limbBit(limb)) == 0;
    }

    lockstep::Result<SeveredPiece> severLimb(physics::PhysicsWorld& physics, const SeverSource& source, anim::Limb limb,
        const SeverThrow& motion)
    {
        const anim::LimbInfo* info = anim::findLimb(source.model, limb);
        const std::span<const math::Mat4> skin = source.instance.skinMatrices();
        const std::span<const math::Mat4> joints = source.instance.jointMatrices();

        if (info == nullptr || !canSever(source, limb) || skin.size() != source.model.skeleton.joints.size()
            || info->rootJoint >= joints.size())
        {
            return lockstep::Error{lockstep::ErrorCode::InvalidArgument, "the limb can't be severed"};
        }

        const anim::LimbMask regions = static_cast<anim::LimbMask>(anim::limbSubtree(limb) & ~hiddenLimbs(source.severed));
        const anim::LimbMask innerStumps = static_cast<anim::LimbMask>(outermostCuts(source.severed) & anim::limbSubtree(limb));
        const std::vector<math::Vec3> hull = anim::poseLimbHull(source.model, regions, skin);

        if (hull.size() < k_minHullPoints)
        {
            return lockstep::Error{lockstep::ErrorCode::InvalidArgument, "the limb has too few hull points"};
        }

        SeveredPiece piece{.model = &source.model,
            .skin = std::vector<math::Mat4>(skin.begin(), skin.end()),
            .parts = (source.gearParts & ~anim::woundCapParts(source.model)
                         & anim::limbParts(source.model, regions, anim::LimbPartRole::Region))
                | anim::limbParts(source.model, anim::limbBit(limb), anim::LimbPartRole::PieceCap)
                | anim::limbParts(source.model, innerStumps, anim::LimbPartRole::StumpCap),
            .modelStart = source.transform,
            .cut = math::transformPoint(joints[info->rootJoint], math::Vec3{0.0f, 0.0f, 0.0f})};
        math::Vec3 centre{0.0f, 0.0f, 0.0f};
        std::vector<math::Vec3> world;

        for (const math::Vec3& point : hull)
        {
            math::expand(piece.bounds, point);
            world.push_back(place(source.transform, point));
            centre += world.back() / static_cast<float>(hull.size());
        }

        piece.bounds.min -= math::Vec3{k_boundsMargin, k_boundsMargin, k_boundsMargin};
        piece.bounds.max += math::Vec3{k_boundsMargin, k_boundsMargin, k_boundsMargin};

        for (math::Vec3& point : world)
        {
            point -= centre;
        }

        auto body = physics.createBody(debrisBody(physics::ConvexShape{.points = std::move(world)}, centre));

        if (!body)
        {
            body = physics.createBody(debrisBody(physics::SphereShape{.radius = k_fallbackRadius}, centre));
        }

        if (!body)
        {
            return body.error();
        }

        piece.body = body.value();
        piece.bodyStart = math::Transform{.position = centre};
        piece.previous = piece.bodyStart;
        piece.current = piece.bodyStart;
        piece.lastSmear = centre;
        physics.setVelocity(piece.body, motion.linear, motion.angular);

        return piece;
    }

    void trackPiece(SeveredPiece& piece, const physics::PhysicsWorld& physics)
    {
        const std::optional<math::Transform> placement = physics.transform(piece.body);

        if (!placement)
        {
            return;
        }

        const bool hasMoved = math::lengthSquared(placement->position - piece.current.position) > 0.0f
            || math::dot(placement->rotation, piece.current.rotation) < 1.0f;

        if (hasMoved)
        {
            piece.previous = piece.current;
            piece.current = *placement;
        }
    }

    math::Transform pieceTransform(const SeveredPiece& piece, float alpha)
    {
        const math::Transform body = math::interpolate(piece.previous, piece.current, alpha);
        const math::Quat turn = body.rotation * math::conjugate(piece.bodyStart.rotation);

        return math::Transform{.position = body.position + turn * (piece.modelStart.position - piece.bodyStart.position),
            .rotation = turn * piece.modelStart.rotation,
            .scale = piece.modelStart.scale};
    }

    void drawPiece(lockstep::render::Renderer& renderer, const SeveredPiece& piece, float alpha)
    {
        if (piece.model == nullptr)
        {
            return;
        }

        const lockstep::render::JointPalette palette = renderer.submitJointPalette(piece.skin, piece.skin);
        const math::Transform transform = pieceTransform(piece, alpha);

        for (std::size_t index = 0; index < piece.model->parts.size() && index < 64; ++index)
        {
            if (((piece.parts >> index) & 1u) != 0)
            {
                renderer.submitSkinned(piece.model->parts[index].mesh, piece.model->parts[index].material, transform,
                    palette, piece.bounds);
            }
        }
    }

    std::optional<float> tracePiece(const SeveredPiece& piece, const math::Vec3& origin, const math::Vec3& direction,
        float maxDistance)
    {
        const math::Transform placement = pieceTransform(piece, 1.0f);
        const math::Quat inverse = math::conjugate(placement.rotation);
        const math::Vec3 from = inverse * (origin - placement.position);
        const math::Vec3 way = inverse * direction;
        const std::array<float, 3> start{from.x / placement.scale.x, from.y / placement.scale.y, from.z / placement.scale.z};
        const std::array<float, 3> step{way.x / placement.scale.x, way.y / placement.scale.y, way.z / placement.scale.z};
        const std::array<float, 3> low{piece.bounds.min.x, piece.bounds.min.y, piece.bounds.min.z};
        const std::array<float, 3> high{piece.bounds.max.x, piece.bounds.max.y, piece.bounds.max.z};
        float near = 0.0f;
        float far = maxDistance;

        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            if (std::abs(step[axis]) < k_parallelRay)
            {
                if (start[axis] < low[axis] || start[axis] > high[axis])
                {
                    return std::nullopt;
                }

                continue;
            }

            const float enter = (low[axis] - start[axis]) / step[axis];
            const float leave = (high[axis] - start[axis]) / step[axis];
            near = std::max(near, std::min(enter, leave));
            far = std::min(far, std::max(enter, leave));
        }

        return near <= far ? std::optional<float>(near) : std::nullopt;
    }

    void pushPiece(physics::PhysicsWorld& physics, const SeveredPiece& piece, const math::Vec3& point,
        const math::Vec3& push)
    {
        const std::optional<math::Vec3> linear = physics.linearVelocity(piece.body);
        const std::optional<math::Vec3> angular = physics.angularVelocity(piece.body);

        if (!linear || !angular)
        {
            return;
        }

        const math::Vec3 lever = point - piece.current.position;
        physics.setVelocity(piece.body, *linear + push, *angular + math::cross(lever, push) * k_hitSpin);
    }

    void releasePiece(physics::PhysicsWorld& physics, SeveredPiece& piece)
    {
        if (!piece.body.isNull())
        {
            physics.destroyBody(piece.body);
            piece.body = {};
        }
    }
} //namespace game
