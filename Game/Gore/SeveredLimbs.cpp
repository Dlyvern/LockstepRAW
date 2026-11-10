#include "Gore/SeveredLimbs.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>

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

    void releasePiece(physics::PhysicsWorld& physics, SeveredPiece& piece)
    {
        if (!piece.body.isNull())
        {
            physics.destroyBody(piece.body);
            piece.body = {};
        }
    }
} //namespace game
