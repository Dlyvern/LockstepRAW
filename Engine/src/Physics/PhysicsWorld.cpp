#include "Lockstep/Physics/PhysicsWorld.hpp"

#include "PhysXConversions.hpp"
#include "PhysicsState.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace lockstep::physics
{
    namespace
    {
        constexpr uintptr_t k_tagKindShift{32};
        constexpr uintptr_t k_tagIndexMask{0xffffffffu};
        constexpr float k_substepRoundingSlack{1.0e-4f};

        Error physicsFailure(const PhysicsState& state, const char* what)
        {
            return Error{ErrorCode::PhysicsFailure, state.errors.describeFailure(what)};
        }

        Status createScene(PhysicsState& state, const PhysicsDescription& description)
        {
            state.dispatcher = physx::PxDefaultCpuDispatcherCreate(description.workerThreads);

            if (state.dispatcher == nullptr)
            {
                return physicsFailure(state, "PxDefaultCpuDispatcherCreate failed");
            }

            physx::PxSceneDesc sceneDescription(state.physics->getTolerancesScale());
            sceneDescription.gravity = toPx(description.gravity);
            sceneDescription.cpuDispatcher = state.dispatcher;
            sceneDescription.filterShader = ragdollAwareFilterShader;
            state.scene = state.physics->createScene(sceneDescription);

            if (state.scene == nullptr)
            {
                return physicsFailure(state, "PxPhysics::createScene failed");
            }

            state.defaultMaterial = state.physics->createMaterial(0.5f, 0.5f, 0.0f);

            if (state.defaultMaterial == nullptr)
            {
                return physicsFailure(state, "PxPhysics::createMaterial failed");
            }

            state.controllerManager = PxCreateControllerManager(*state.scene);

            if (state.controllerManager == nullptr)
            {
                return physicsFailure(state, "PxCreateControllerManager failed");
            }

            return {};
        }

        void forEachRagdoll(PhysicsState& state, void (*visit)(RagdollSlot&) noexcept)
        {
            for (uint32_t index = 0; index < state.ragdolls.capacity(); ++index)
            {
                if (const auto key = state.ragdolls.keyAt(index))
                {
                    visit(*state.ragdolls.find(*key));
                }
            }
        }
    } //namespace

    void* encodeActorTag(ActorKind kind, uint32_t index) noexcept
    {
        const uintptr_t value = (static_cast<uintptr_t>(kind) << k_tagKindShift) | static_cast<uintptr_t>(index);
        return reinterpret_cast<void*>(value);
    }

    ActorTag decodeActorTag(const void* userData) noexcept
    {
        const uintptr_t value = reinterpret_cast<uintptr_t>(userData);
        const auto kind = static_cast<ActorKind>(value >> k_tagKindShift);

        if (kind != ActorKind::Body && kind != ActorKind::Controller)
        {
            return ActorTag{};
        }

        return ActorTag{.kind = kind, .index = static_cast<uint32_t>(value & k_tagIndexMask)};
    }

    PhysicsState::~PhysicsState()
    {
        if (controllerManager != nullptr)
        {
            controllerManager->release();
        }

        for (uint32_t index = 0; ragdolls.count() > 0; ++index)
        {
            if (auto key = ragdolls.keyAt(index))
            {
                if (auto ragdoll = ragdolls.remove(*key))
                {
                    releaseRagdoll(*ragdoll);
                }
            }
        }

        for (uint32_t index = 0; bodies.count() > 0; ++index)
        {
            if (auto key = bodies.keyAt(index))
            {
                if (auto actor = bodies.remove(*key))
                {
                    (*actor)->release();
                }
            }
        }

        if (scene != nullptr)
        {
            scene->release();
        }

        if (defaultMaterial != nullptr)
        {
            defaultMaterial->release();
        }

        if (dispatcher != nullptr)
        {
            dispatcher->release();
        }

        if (physics != nullptr)
        {
            physics->release();
        }

        if (foundation != nullptr)
        {
            foundation->release();
        }
    }

    physx::PxRigidActor* PhysicsState::findBody(BodyId body) const noexcept
    {
        physx::PxRigidActor* const* actor = bodies.find({body.index, body.generation});
        return actor != nullptr ? *actor : nullptr;
    }

    ControllerSlot* PhysicsState::findController(ControllerId controller) noexcept
    {
        return controllers.find({controller.index, controller.generation});
    }

    const ControllerSlot* PhysicsState::findController(ControllerId controller) const noexcept
    {
        return controllers.find({controller.index, controller.generation});
    }

    RagdollSlot* PhysicsState::findRagdoll(RagdollId ragdoll) noexcept
    {
        return ragdolls.find({ragdoll.index, ragdoll.generation});
    }

    const RagdollSlot* PhysicsState::findRagdoll(RagdollId ragdoll) const noexcept
    {
        return ragdolls.find({ragdoll.index, ragdoll.generation});
    }

    Result<PhysicsWorld> PhysicsWorld::create(const PhysicsDescription& description)
    {
        auto state = std::make_unique<PhysicsState>();
        state->foundation = PxCreateFoundation(PX_PHYSICS_VERSION, state->allocator, state->errors);

        if (state->foundation == nullptr)
        {
            return physicsFailure(*state, "PxCreateFoundation failed (is another PhysicsWorld alive?)");
        }

        state->physics = PxCreatePhysics(PX_PHYSICS_VERSION, *state->foundation, physx::PxTolerancesScale());

        if (state->physics == nullptr)
        {
            return physicsFailure(*state, "PxCreatePhysics failed");
        }

        if (auto created = createScene(*state, description); !created)
        {
            return created.error();
        }

        return PhysicsWorld(std::move(state));
    }

    PhysicsWorld::PhysicsWorld(std::unique_ptr<PhysicsState> state) noexcept
        : m_state(std::move(state))
    {
    }

    PhysicsWorld::~PhysicsWorld() = default;

    PhysicsWorld::PhysicsWorld(PhysicsWorld&& other) noexcept = default;

    PhysicsWorld& PhysicsWorld::operator=(PhysicsWorld&& other) noexcept = default;

    void PhysicsWorld::step(float deltaSeconds)
    {
        m_state->lastStepSeconds = 0.0f;
        m_state->lastSubsteps = 0;
        m_state->lastStepMilliseconds = 0.0f;

        if (!(deltaSeconds > 0.0f))
        {
            return;
        }

        const auto startTime = std::chrono::steady_clock::now();
        const float wantedSubsteps = std::ceil(deltaSeconds / k_maxSubstepSeconds - k_substepRoundingSlack);
        const uint32_t substeps = std::clamp(static_cast<uint32_t>(wantedSubsteps), 1u, k_maxSubsteps);
        const float substepSeconds = std::min(deltaSeconds / static_cast<float>(substeps), k_maxSubstepSeconds);

        forEachRagdoll(*m_state, beginRagdollStep);

        for (uint32_t substep = 0; substep < substeps; ++substep)
        {
            m_state->scene->simulate(substepSeconds);
            m_state->scene->fetchResults(true);
        }

        forEachRagdoll(*m_state, endRagdollStep);

        const auto elapsed = std::chrono::steady_clock::now() - startTime;
        m_state->lastStepSeconds = substepSeconds * static_cast<float>(substeps);
        m_state->lastSubsteps = substeps;
        m_state->lastStepMilliseconds = std::chrono::duration<float, std::milli>(elapsed).count();
    }

    PhysicsStats PhysicsWorld::stats() const noexcept
    {
        return PhysicsStats{
            .bodyCount = m_state->bodies.count(),
            .controllerCount = m_state->controllers.count(),
            .ragdollCount = m_state->ragdolls.count(),
            .ragdollBodyCount = m_state->ragdollBodyCount,
            .lastStepSeconds = m_state->lastStepSeconds,
            .lastSubsteps = m_state->lastSubsteps,
            .lastStepMilliseconds = m_state->lastStepMilliseconds,
            .errorCount = m_state->errors.errorCount(),
            .warningCount = m_state->errors.warningCount()};
    }
} //namespace lockstep::physics
