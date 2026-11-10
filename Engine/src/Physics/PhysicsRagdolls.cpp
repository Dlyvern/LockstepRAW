#include "Lockstep/Physics/PhysicsWorld.hpp"

#include "PhysXConversions.hpp"
#include "PhysicsState.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::physics
{
    namespace
    {
        constexpr float k_minDirectionLength{1.0e-5f};
        constexpr float k_overlapInflation{0.01f};
        const physx::PxVec3 k_xAxis{1.0f, 0.0f, 0.0f};

        Error invalid(const char* message)
        {
            return Error{ErrorCode::InvalidArgument, message};
        }

        bool isAngle(float value) noexcept
        {
            return std::isfinite(value) && std::abs(value) < physx::PxPi;
        }

        bool areLimitsValid(const RagdollJointLimits& limits) noexcept
        {
            return isAngle(limits.twist) && isAngle(limits.bendMin) && isAngle(limits.bendMax) && isAngle(limits.side)
                && limits.twist > 0.0f && limits.side > 0.0f && limits.bendMin < limits.bendMax;
        }

        Status validate(const RagdollDescription& description)
        {
            if (description.bodies.empty() || description.bodies.size() > k_maxRagdollBodies)
            {
                return invalid("PhysicsWorld::createRagdoll: needs 1 to k_maxRagdollBodies bodies");
            }

            for (std::size_t index = 0; index < description.bodies.size(); ++index)
            {
                const RagdollBodyDescription& body = description.bodies[index];
                const bool isRoot = index == 0;

                if (isRoot ? body.parent != k_noRagdollBody : body.parent >= index)
                {
                    return invalid("PhysicsWorld::createRagdoll: the first body is the root; every other body's "
                                   "parent must come before it");
                }

                if (!(body.radius > 0.0f) || !(body.density > 0.0f))
                {
                    return invalid("PhysicsWorld::createRagdoll: radius and density must be > 0");
                }

                if (!isRoot && !areLimitsValid(body.limits))
                {
                    return invalid("PhysicsWorld::createRagdoll: joint limits must be within (-pi, pi), twist and "
                                   "side > 0, bendMin < bendMax");
                }
            }

            return {};
        }

        physx::PxQuat arcFromX(const physx::PxVec3& direction)
        {
            const float length = direction.magnitude();

            if (length < k_minDirectionLength)
            {
                return physx::PxQuat(physx::PxIdentity);
            }

            const physx::PxVec3 unit = direction / length;
            const float cosine = k_xAxis.dot(unit);

            if (cosine < -1.0f + k_minDirectionLength)
            {
                return physx::PxQuat(physx::PxPi, physx::PxVec3(0.0f, 1.0f, 0.0f));
            }

            const physx::PxVec3 axis = k_xAxis.cross(unit);
            return physx::PxQuat(axis.x, axis.y, axis.z, 1.0f + cosine).getNormalized();
        }

        physx::PxVec3 anyPerpendicular(const physx::PxVec3& unit)
        {
            const physx::PxVec3 helper = std::abs(unit.x) < 0.9f ? k_xAxis : physx::PxVec3(0.0f, 1.0f, 0.0f);
            return unit.cross(helper).getNormalized();
        }

        physx::PxTransform restJointFrame(const RagdollBodyDescription& body)
        {
            physx::PxVec3 twist = toPx(body.twistAxis);
            twist = twist.magnitude() > k_minDirectionLength ? twist.getNormalized() : physx::PxVec3(0.0f, 1.0f, 0.0f);
            physx::PxVec3 hinge = twist.cross(toPx(body.bendDirection));
            hinge = hinge.magnitude() > k_minDirectionLength ? hinge.getNormalized() : anyPerpendicular(twist);
            const physx::PxVec3 bend = hinge.cross(twist);
            return physx::PxTransform(toPx(body.rest.position), physx::PxQuat(physx::PxMat33(twist, bend, hinge)).getNormalized());
        }

        struct CapsulePart
        {
            physx::PxCapsuleGeometry geometry;
            physx::PxTransform localPose;
        };

        CapsulePart bodyShape(const RagdollBodyDescription& body)
        {
            const physx::PxTransform toBone = toPx(body.rest).getInverse();
            const physx::PxVec3 start = toBone.transform(toPx(body.capsuleStart));
            const physx::PxVec3 end = toBone.transform(toPx(body.capsuleEnd));
            const physx::PxVec3 axis = end - start;
            return CapsulePart{.geometry = physx::PxCapsuleGeometry(body.radius, 0.5f * axis.magnitude()),
                .localPose = physx::PxTransform(0.5f * (start + end), arcFromX(axis))};
        }

        uint32_t neighbourMask(const RagdollDescription& description, uint32_t body)
        {
            uint32_t mask = 0;
            const uint32_t parent = description.bodies[body].parent;

            for (uint32_t other = 0; other < description.bodies.size(); ++other)
            {
                const uint32_t otherParent = description.bodies[other].parent;
                const bool isParent = other == parent;
                const bool isChild = otherParent == body;
                const bool isSibling = other != body && parent != k_noRagdollBody && otherParent == parent;
                const bool isGrandparent = parent != k_noRagdollBody && other == description.bodies[parent].parent;

                if (other == body || isParent || isChild || isSibling || isGrandparent)
                {
                    mask |= 1u << other;
                }
            }

            return mask;
        }

        void ignoreStartingOverlaps(const std::vector<CapsulePart>& shapes, const std::vector<physx::PxTransform>& poses,
            std::vector<uint32_t>& ignored)
        {
            for (uint32_t first = 0; first < shapes.size(); ++first)
            {
                for (uint32_t second = first + 1; second < shapes.size(); ++second)
                {
                    const physx::PxCapsuleGeometry inflated(shapes[first].geometry.radius + k_overlapInflation,
                        shapes[first].geometry.halfHeight);
                    const bool overlaps = physx::PxGeometryQuery::overlap(inflated, poses[first] * shapes[first].localPose,
                        shapes[second].geometry, poses[second] * shapes[second].localPose);

                    if (overlaps)
                    {
                        ignored[first] |= 1u << second;
                        ignored[second] |= 1u << first;
                    }
                }
            }
        }

        void configureActor(physx::PxRigidDynamic& actor, const RagdollSettings& settings)
        {
            actor.setLinearDamping(settings.linearDamping);
            actor.setAngularDamping(settings.angularDamping);
            actor.setSolverIterationCounts(std::max(settings.positionIterations, 1u),
                std::max(settings.velocityIterations, 1u));
            actor.setSleepThreshold(settings.sleepThreshold);
            actor.setMaxDepenetrationVelocity(settings.maxDepenetration);
            actor.setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD, true);
        }

        physx::PxD6Joint* createJoint(physx::PxPhysics& physics, const RagdollDescription& description, uint32_t body,
            physx::PxRigidDynamic& parentActor, physx::PxRigidDynamic& childActor)
        {
            const RagdollBodyDescription& child = description.bodies[body];
            const RagdollBodyDescription& parent = description.bodies[child.parent];
            const physx::PxTransform frame = restJointFrame(child);
            physx::PxD6Joint* joint = physx::PxD6JointCreate(physics, &parentActor, toPx(parent.rest).getInverse() * frame,
                &childActor, toPx(child.rest).getInverse() * frame);

            if (joint == nullptr)
            {
                return nullptr;
            }

            const RagdollJointLimits& limits = child.limits;
            joint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eLIMITED);
            joint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eLIMITED);
            joint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eLIMITED);
            joint->setTwistLimit(physx::PxJointAngularLimitPair(-limits.twist, limits.twist));
            joint->setPyramidSwingLimit(
                physx::PxJointLimitPyramid(-limits.side, limits.side, limits.bendMin, limits.bendMax));

            if (description.settings.jointDamping > 0.0f)
            {
                joint->setAngularDriveConfig(physx::PxD6AngularDriveConfig::eSLERP);
                joint->setDrive(physx::PxD6Drive::eSLERP,
                    physx::PxD6JointDrive(0.0f, description.settings.jointDamping, PX_MAX_F32, true));
            }

            return joint;
        }

        void setStartVelocity(physx::PxRigidDynamic& actor, const RagdollDescription& description,
            const physx::PxVec3& rootPosition)
        {
            const physx::PxVec3 centre = actor.getGlobalPose().transform(actor.getCMassLocalPose().p);
            const physx::PxVec3 spin = toPx(description.angularVelocity);
            actor.setLinearVelocity(toPx(description.linearVelocity) + spin.cross(centre - rootPosition));
            actor.setAngularVelocity(spin);
        }

        bool isSubtreeOf(const RagdollSlot& slot, uint32_t body, uint32_t subtreeRoot)
        {
            for (uint32_t current = body; current != k_noRagdollBody; current = slot.parts[current].parent)
            {
                if (current == subtreeRoot)
                {
                    return true;
                }
            }

            return false;
        }
    } //namespace

    Result<RagdollId> PhysicsWorld::createRagdoll(const RagdollDescription& description)
    {
        if (auto valid = validate(description); !valid)
        {
            return valid.error();
        }

        PhysicsState& state = *m_state;
        const RagdollSettings& settings = description.settings;
        const auto bodyCount = static_cast<uint32_t>(description.bodies.size());
        physx::PxMaterial* material = state.physics->createMaterial(settings.friction, settings.friction,
            settings.restitution);
        physx::PxAggregate* aggregate = state.physics->createAggregate(bodyCount, bodyCount,
            physx::PxGetAggregateFilterHint(physx::PxAggregateType::eGENERIC, settings.selfCollision));
        const std::optional<HandlePool<RagdollSlot>::Key> key = state.ragdolls.insert(RagdollSlot{.aggregate = aggregate, .parts = {}});

        if (material == nullptr || aggregate == nullptr || !key)
        {
            if (material != nullptr)
            {
                material->release();
            }

            if (aggregate != nullptr)
            {
                aggregate->release();
            }

            return Error{ErrorCode::PhysicsFailure,
                state.errors.describeFailure("PhysicsWorld::createRagdoll: material or aggregate not created")};
        }

        RagdollSlot& slot = *state.ragdolls.find(*key);
        std::vector<CapsulePart> shapes;
        std::vector<physx::PxTransform> poses;
        std::vector<uint32_t> ignored;

        for (uint32_t body = 0; body < bodyCount; ++body)
        {
            shapes.push_back(bodyShape(description.bodies[body]));
            poses.push_back(toPx(description.bodies[body].pose));
            ignored.push_back(neighbourMask(description, body));
        }

        ignoreStartingOverlaps(shapes, poses, ignored);
        bool isComplete = true;

        for (uint32_t body = 0; body < bodyCount && isComplete; ++body)
        {
            const RagdollBodyDescription& source = description.bodies[body];
            physx::PxRigidDynamic* actor = state.physics->createRigidDynamic(poses[body]);
            physx::PxShape* shape = actor != nullptr
                ? physx::PxRigidActorExt::createExclusiveShape(*actor, shapes[body].geometry, *material)
                : nullptr;
            slot.parts.push_back(RagdollPart{.actor = actor,
                .parent = source.parent,
                .previous = fromPx(poses[body]),
                .current = fromPx(poses[body])});

            if (shape == nullptr)
            {
                isComplete = false;
                break;
            }

            shape->setLocalPose(shapes[body].localPose);
            shape->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, false);
            shape->setSimulationFilterData(ragdollFilterData(key->index, body, ignored[body], settings.selfCollision));
            physx::PxRigidBodyExt::updateMassAndInertia(*actor, source.density);
            configureActor(*actor, settings);
            setStartVelocity(*actor, description, poses[0].p);

            if (body > 0)
            {
                slot.parts.back().joint = createJoint(*state.physics, description, body, *slot.parts[source.parent].actor,
                    *actor);
                isComplete = slot.parts.back().joint != nullptr;
            }

            isComplete = isComplete && aggregate->addActor(*actor);
        }

        material->release();

        if (!isComplete || !state.scene->addAggregate(*aggregate))
        {
            if (auto removed = state.ragdolls.remove(*key))
            {
                releaseRagdoll(*removed);
            }

            return Error{ErrorCode::PhysicsFailure,
                state.errors.describeFailure("PhysicsWorld::createRagdoll: a body or joint could not be created")};
        }

        state.ragdollBodyCount += bodyCount;
        return RagdollId{.index = key->index, .generation = key->generation};
    }

    bool PhysicsWorld::destroyRagdoll(RagdollId ragdoll)
    {
        std::optional<RagdollSlot> removed = m_state->ragdolls.remove({ragdoll.index, ragdoll.generation});

        if (!removed)
        {
            return false;
        }

        for (const RagdollPart& part : removed->parts)
        {
            m_state->ragdollBodyCount -= part.actor != nullptr ? 1u : 0u;
        }

        releaseRagdoll(*removed);
        return true;
    }

    bool PhysicsWorld::isAlive(RagdollId ragdoll) const noexcept
    {
        return m_state->findRagdoll(ragdoll) != nullptr;
    }

    bool PhysicsWorld::applyRagdollImpulse(RagdollId ragdoll, uint32_t body, const math::Vec3& impulse,
        const std::optional<math::Vec3>& point)
    {
        RagdollSlot* slot = m_state->findRagdoll(ragdoll);

        if (slot == nullptr || body >= slot->parts.size() || slot->parts[body].actor == nullptr)
        {
            return false;
        }

        physx::PxRigidDynamic& actor = *slot->parts[body].actor;

        if (point)
        {
            physx::PxRigidBodyExt::addForceAtPos(actor, toPx(impulse), toPx(*point), physx::PxForceMode::eIMPULSE);
        }
        else
        {
            actor.addForce(toPx(impulse), physx::PxForceMode::eIMPULSE);
        }

        return true;
    }

    bool PhysicsWorld::ragdollPoses(RagdollId ragdoll, float alpha, std::span<math::Transform> poses) const
    {
        const RagdollSlot* slot = m_state->findRagdoll(ragdoll);

        if (slot == nullptr || poses.size() != slot->parts.size())
        {
            return false;
        }

        const float t = std::clamp(alpha, 0.0f, 1.0f);

        for (std::size_t index = 0; index < poses.size(); ++index)
        {
            poses[index] = math::interpolate(slot->parts[index].previous, slot->parts[index].current, t);
        }

        return true;
    }

    std::optional<RagdollStatus> PhysicsWorld::ragdollStatus(RagdollId ragdoll) const
    {
        const RagdollSlot* slot = m_state->findRagdoll(ragdoll);

        if (slot == nullptr)
        {
            return std::nullopt;
        }

        RagdollStatus status{.isAsleep = true};

        for (const RagdollPart& part : slot->parts)
        {
            if (part.actor == nullptr)
            {
                continue;
            }

            ++status.simulatedCount;
            status.isAsleep = status.isAsleep && part.actor->isSleeping();
            status.maxSpeed = std::max(status.maxSpeed, part.actor->getLinearVelocity().magnitude());
        }

        return status;
    }

    bool PhysicsWorld::detachRagdollBody(RagdollId ragdoll, uint32_t body)
    {
        RagdollSlot* slot = m_state->findRagdoll(ragdoll);

        if (slot == nullptr || body >= slot->parts.size() || slot->parts[body].joint == nullptr)
        {
            return false;
        }

        slot->parts[body].joint->release();
        slot->parts[body].joint = nullptr;
        slot->parts[body].actor->wakeUp();

        if (const uint32_t parent = slot->parts[body].parent; slot->parts[parent].actor != nullptr)
        {
            slot->parts[parent].actor->wakeUp();
        }

        return true;
    }

    bool PhysicsWorld::removeRagdollBodies(RagdollId ragdoll, uint32_t body)
    {
        RagdollSlot* slot = m_state->findRagdoll(ragdoll);

        if (slot == nullptr || body >= slot->parts.size() || slot->parts[body].actor == nullptr)
        {
            return false;
        }

        for (uint32_t index = 0; index < slot->parts.size(); ++index)
        {
            RagdollPart& part = slot->parts[index];

            if (part.actor == nullptr || !isSubtreeOf(*slot, index, body))
            {
                continue;
            }

            if (part.joint != nullptr)
            {
                part.joint->release();
                part.joint = nullptr;
            }

            part.previous = part.current;
            part.actor->release();
            part.actor = nullptr;
            --m_state->ragdollBodyCount;
        }

        if (const uint32_t parent = slot->parts[body].parent;
            parent != k_noRagdollBody && slot->parts[parent].actor != nullptr)
        {
            slot->parts[parent].actor->wakeUp();
        }

        return true;
    }
} //namespace lockstep::physics
