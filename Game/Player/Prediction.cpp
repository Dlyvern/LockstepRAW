#include "Player/Prediction.hpp"

#include "Player/WireBytes.hpp"

#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace physics = lockstep::physics;
        namespace world = lockstep::world;

        constexpr std::byte k_aliveBit{0x01};
        constexpr std::byte k_groundedBit{0x02};
        constexpr std::size_t k_flagsOffset{0};
        constexpr std::size_t k_lifeOffset{1};
        constexpr std::size_t k_footOffset{2};
        constexpr std::size_t k_verticalSpeedOffset{14};
        constexpr std::size_t k_planarVelocityOffset{18};
        constexpr std::size_t k_floatBytes{4};
        constexpr std::size_t k_maxHistory{240};
        constexpr float k_verticalSpeedTolerance{0.05f};
        constexpr float k_planarVelocityTolerance{0.05f};
        constexpr float k_offsetFadePerTick{0.85f};
        constexpr float k_minVisibleOffset{0.001f};
        constexpr float k_maxSmoothedOffset{1.5f};

        void removePawn(Prediction& prediction, world::Entities& entities, physics::PhysicsWorld& physics)
        {
            entities.destroy(prediction.entity);
            physics.destroyController(prediction.controller);
            prediction.entity = {};
            prediction.controller = {};
            prediction.visualOffset = {};
        }

        float yawOf(const Prediction& prediction)
        {
            return prediction.history.empty() ? 0.0f : prediction.history.back().command.yaw;
        }

        void replay(Prediction& prediction, physics::PhysicsWorld& physics, const PawnSettings& settings, float delta)
        {
            for (PredictedMove& move : prediction.history)
            {
                move.foot = stepPawn(prediction.motion, prediction.controller, physics, move.command, settings, delta);
                move.motion = prediction.motion;
            }

            prediction.lastReplayed = prediction.history.size();
        }

        void dropOlder(Prediction& prediction, uint64_t inputTick)
        {
            while (!prediction.history.empty() && prediction.history.front().tick < inputTick)
            {
                prediction.history.pop_front();
            }
        }

        void dropAcknowledged(Prediction& prediction, uint64_t inputTick)
        {
            while (!prediction.history.empty() && prediction.history.front().tick <= inputTick)
            {
                prediction.history.pop_front();
            }
        }

        bool spawnPawn(Prediction& prediction, world::Entities& entities, physics::PhysicsWorld& physics,
            const OwnPawnState& state)
        {
            auto controller = physics.createController(pawnController(state.foot));

            if (!controller)
            {
                return false;
            }

            auto entity = entities.create({.domain = world::TimeDomain::Player,
                .owner = world::EntityOwner::Player2,
                .clientOnly = true});

            if (!entity)
            {
                physics.destroyController(controller.value());
                return false;
            }

            prediction.controller = controller.value();
            prediction.entity = entity.value();
            prediction.life = state.life;

            return true;
        }

        void placeEntity(Prediction& prediction, world::Entities& entities, const math::Vec3& foot, bool isTeleport)
        {
            world::Entity* entity = entities.find(prediction.entity);

            if (entity == nullptr)
            {
                return;
            }

            placePawnEntity(*entity, foot, yawOf(prediction));

            if (isTeleport)
            {
                entity->teleport(entity->transform());
            }
        }

        bool isMispredicted(const PredictedMove* acked, const OwnPawnState& state, float& error)
        {
            if (acked == nullptr || !acked->foot)
            {
                error = 0.0f;
                return true;
            }

            error = math::length(*acked->foot - state.foot);
            const bool isSpeedOff = std::abs(acked->motion.verticalSpeed - state.motion.verticalSpeed)
                    > k_verticalSpeedTolerance
                || math::length(acked->motion.planarVelocity - state.motion.planarVelocity) > k_planarVelocityTolerance;

            return error > k_correctionThreshold || isSpeedOff || acked->motion.isGrounded != state.motion.isGrounded;
        }

        void restart(Prediction& prediction, world::Entities& entities, physics::PhysicsWorld& physics,
            const OwnPawnState& state, const PawnSettings& settings, float delta)
        {
            removePawn(prediction, entities, physics);

            if (!spawnPawn(prediction, entities, physics, state))
            {
                return;
            }

            prediction.motion = state.motion;
            replay(prediction, physics, settings, delta);
            placeEntity(prediction, entities, physics.footPosition(prediction.controller).value_or(state.foot), true);
        }
    } //namespace

    std::array<std::byte, k_ownPawnStateBytes> encodeOwnPawnState(const OwnPawnState& state)
    {
        std::array<std::byte, k_ownPawnStateBytes> bytes{};
        bytes[k_flagsOffset] = (state.isAlive ? k_aliveBit : std::byte{0})
            | (state.motion.isGrounded ? k_groundedBit : std::byte{0});
        bytes[k_lifeOffset] = static_cast<std::byte>(state.life);
        writeFloat(bytes, k_footOffset, state.foot.x);
        writeFloat(bytes, k_footOffset + k_floatBytes, state.foot.y);
        writeFloat(bytes, k_footOffset + 2 * k_floatBytes, state.foot.z);
        writeFloat(bytes, k_verticalSpeedOffset, state.motion.verticalSpeed);
        writeFloat(bytes, k_planarVelocityOffset, state.motion.planarVelocity.x);
        writeFloat(bytes, k_planarVelocityOffset + k_floatBytes, state.motion.planarVelocity.z);

        return bytes;
    }

    std::optional<OwnPawnState> decodeOwnPawnState(std::span<const std::byte> bytes)
    {
        if (bytes.size() != k_ownPawnStateBytes)
        {
            return std::nullopt;
        }

        const OwnPawnState state{
            .isAlive = (bytes[k_flagsOffset] & k_aliveBit) != std::byte{0},
            .life = static_cast<uint8_t>(bytes[k_lifeOffset]),
            .foot = {readFloat(bytes, k_footOffset), readFloat(bytes, k_footOffset + k_floatBytes),
                readFloat(bytes, k_footOffset + 2 * k_floatBytes)},
            .motion = {.verticalSpeed = readFloat(bytes, k_verticalSpeedOffset),
                .isGrounded = (bytes[k_flagsOffset] & k_groundedBit) != std::byte{0},
                .planarVelocity = {readFloat(bytes, k_planarVelocityOffset), 0.0f,
                    readFloat(bytes, k_planarVelocityOffset + k_floatBytes)}}};

        const bool isFinite = std::isfinite(state.foot.x) && std::isfinite(state.foot.y) && std::isfinite(state.foot.z)
            && std::isfinite(state.motion.verticalSpeed) && std::isfinite(state.motion.planarVelocity.x)
            && std::isfinite(state.motion.planarVelocity.z);

        if (!isFinite)
        {
            return std::nullopt;
        }

        return state;
    }

    std::optional<float> reconcilePrediction(Prediction& prediction, world::Entities& entities,
        physics::PhysicsWorld& physics, const lockstep::net::InputAck& ack, const PawnSettings& settings, float delta)
    {
        const std::optional<OwnPawnState> state = decodeOwnPawnState(ack.clientState);

        if (!state)
        {
            return std::nullopt;
        }

        if (!state->isAlive)
        {
            removePawn(prediction, entities, physics);
            return std::nullopt;
        }

        if (ack.inputTick)
        {
            dropOlder(prediction, *ack.inputTick);
        }

        const bool isCurrentLife = physics.isAlive(prediction.controller) && prediction.life == state->life;
        const bool isNewInput = ack.inputTick && (!prediction.ackedTick || *ack.inputTick > *prediction.ackedTick);

        if (ack.inputTick)
        {
            prediction.ackedTick = ack.inputTick;
        }

        if (isCurrentLife && !isNewInput)
        {
            return std::nullopt;
        }

        if (!isCurrentLife || !ack.inputTick)
        {
            if (ack.inputTick)
            {
                dropAcknowledged(prediction, *ack.inputTick);
            }

            if (!isCurrentLife)
            {
                restart(prediction, entities, physics, *state, settings, delta);
            }

            return std::nullopt;
        }

        const PredictedMove* acked = !prediction.history.empty() && prediction.history.front().tick == *ack.inputTick
            ? &prediction.history.front()
            : nullptr;
        float error = 0.0f;
        const bool isOff = isMispredicted(acked, *state, error);
        dropAcknowledged(prediction, *ack.inputTick);

        if (!isOff)
        {
            prediction.lastReplayed = 0;
            return std::nullopt;
        }

        const math::Vec3 before = physics.footPosition(prediction.controller).value_or(state->foot);
        physics.setFootPosition(prediction.controller, state->foot);
        prediction.motion = state->motion;
        replay(prediction, physics, settings, delta);
        const math::Vec3 after = physics.footPosition(prediction.controller).value_or(state->foot);

        prediction.visualOffset = prediction.visualOffset + (before - after);

        if (math::length(prediction.visualOffset) > k_maxSmoothedOffset)
        {
            prediction.visualOffset = {};
            placeEntity(prediction, entities, after, true);
        }
        else
        {
            placeEntity(prediction, entities, after, false);
        }

        ++prediction.corrections;
        prediction.lastError = error;

        return error;
    }

    void predictPawn(Prediction& prediction, world::Entities& entities, physics::PhysicsWorld& physics, uint64_t tick,
        const PlayerCommand& command, const PawnSettings& settings, float delta)
    {
        PredictedMove move{.tick = tick, .command = command, .foot = std::nullopt, .motion = prediction.motion};

        if (physics.isAlive(prediction.controller))
        {
            move.foot = stepPawn(prediction.motion, prediction.controller, physics, command, settings, delta);
            move.motion = prediction.motion;
        }

        prediction.history.push_back(move);

        while (prediction.history.size() > k_maxHistory)
        {
            prediction.history.pop_front();
        }

        if (move.foot)
        {
            placeEntity(prediction, entities, *move.foot, false);
        }

        prediction.visualOffset = prediction.visualOffset * k_offsetFadePerTick;

        if (math::length(prediction.visualOffset) < k_minVisibleOffset)
        {
            prediction.visualOffset = {};
        }
    }

    void clearPrediction(Prediction& prediction, world::Entities& entities, physics::PhysicsWorld& physics)
    {
        removePawn(prediction, entities, physics);
        prediction.history.clear();
        prediction.motion = {};
        prediction.ackedTick.reset();
    }

    std::optional<math::Transform> predictedTransform(const Prediction& prediction, const world::Entities& entities,
        float alpha)
    {
        const world::Entity* entity = entities.find(prediction.entity);

        if (entity == nullptr)
        {
            return std::nullopt;
        }

        math::Transform transform = entity->interpolatedTransform(alpha);
        transform.position = transform.position + prediction.visualOffset;

        return transform;
    }
} //namespace game
