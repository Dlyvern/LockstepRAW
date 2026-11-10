#include "Player/Prediction.hpp"

#include "Player/PawnLean.hpp"
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
        constexpr std::byte k_crouchHeldBit{0x04};
        constexpr std::byte k_jumpHeldBit{0x08};
        constexpr std::byte k_vaultingBit{0x10};
        constexpr std::byte k_kickHeldBit{0x20};
        constexpr std::size_t k_flagsOffset{0};
        constexpr std::size_t k_lifeOffset{1};
        constexpr std::size_t k_footOffset{2};
        constexpr std::size_t k_verticalSpeedOffset{14};
        constexpr std::size_t k_planarVelocityOffset{18};
        constexpr std::size_t k_stanceOffset{26};
        constexpr std::size_t k_slideSecondsOffset{27};
        constexpr std::size_t k_slideCooldownOffset{31};
        constexpr std::size_t k_jumpBufferOffset{35};
        constexpr std::size_t k_coyoteOffset{36};
        constexpr std::size_t k_landingOffset{37};
        constexpr std::size_t k_leanOffset{38};
        constexpr std::size_t k_sprintRecoverOffset{42};
        constexpr std::size_t k_mantleTicksOffset{43};
        constexpr std::size_t k_mantleHeightOffset{44};
        constexpr std::size_t k_kickTicksOffset{48};
        constexpr std::size_t k_kickCooldownOffset{49};
        constexpr float k_mantleHeightTolerance{0.01f};
        constexpr float k_leanTolerance{0.01f};
        constexpr uint8_t k_maxStance{static_cast<uint8_t>(PawnStance::Slide)};
        constexpr float k_slideTimeTolerance{0.02f};
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

        void placeEntity(Prediction& prediction, world::Entities& entities, const math::Vec3& foot,
            const PawnSettings& settings, bool isTeleport)
        {
            world::Entity* entity = entities.find(prediction.entity);

            if (entity == nullptr)
            {
                return;
            }

            placePawnEntity(*entity, foot, yawOf(prediction), stanceHeight(prediction.motion.stance, settings),
                leanOffset(prediction.motion, settings));

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

            const bool isStanceOff = acked->motion.stance != state.motion.stance
                || acked->motion.isCrouchHeld != state.motion.isCrouchHeld
                || acked->motion.isJumpHeld != state.motion.isJumpHeld
                || acked->motion.jumpBufferTicks != state.motion.jumpBufferTicks
                || acked->motion.coyoteTicks != state.motion.coyoteTicks
                || acked->motion.landingTicks != state.motion.landingTicks
                || std::abs(acked->motion.lean - state.motion.lean) > k_leanTolerance
                || acked->motion.sprintRecoverTicks != state.motion.sprintRecoverTicks
                || acked->motion.mantleTicks != state.motion.mantleTicks
                || acked->motion.isVaulting != state.motion.isVaulting
                || acked->motion.isKickHeld != state.motion.isKickHeld
                || acked->motion.kickTicks != state.motion.kickTicks
                || acked->motion.kickCooldownTicks != state.motion.kickCooldownTicks
                || std::abs(acked->motion.mantleHeight - state.motion.mantleHeight) > k_mantleHeightTolerance
                || std::abs(acked->motion.slideSeconds - state.motion.slideSeconds) > k_slideTimeTolerance
                || std::abs(acked->motion.slideCooldown - state.motion.slideCooldown) > k_slideTimeTolerance;

            return error > k_correctionThreshold || isSpeedOff || isStanceOff
                || acked->motion.isGrounded != state.motion.isGrounded;
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
            matchStanceHeight(physics, prediction.controller, prediction.motion, settings);
            replay(prediction, physics, settings, delta);
            placeEntity(prediction, entities, physics.footPosition(prediction.controller).value_or(state.foot), settings,
                true);
        }
    } //namespace

    std::array<std::byte, k_ownPawnStateBytes> encodeOwnPawnState(const OwnPawnState& state)
    {
        std::array<std::byte, k_ownPawnStateBytes> bytes{};
        bytes[k_flagsOffset] = (state.isAlive ? k_aliveBit : std::byte{0})
            | (state.motion.isGrounded ? k_groundedBit : std::byte{0})
            | (state.motion.isCrouchHeld ? k_crouchHeldBit : std::byte{0})
            | (state.motion.isJumpHeld ? k_jumpHeldBit : std::byte{0})
            | (state.motion.isVaulting ? k_vaultingBit : std::byte{0})
            | (state.motion.isKickHeld ? k_kickHeldBit : std::byte{0});
        bytes[k_lifeOffset] = static_cast<std::byte>(state.life);
        writeFloat(bytes, k_footOffset, state.foot.x);
        writeFloat(bytes, k_footOffset + k_floatBytes, state.foot.y);
        writeFloat(bytes, k_footOffset + 2 * k_floatBytes, state.foot.z);
        writeFloat(bytes, k_verticalSpeedOffset, state.motion.verticalSpeed);
        writeFloat(bytes, k_planarVelocityOffset, state.motion.planarVelocity.x);
        writeFloat(bytes, k_planarVelocityOffset + k_floatBytes, state.motion.planarVelocity.z);
        bytes[k_stanceOffset] = static_cast<std::byte>(state.motion.stance);
        writeFloat(bytes, k_slideSecondsOffset, state.motion.slideSeconds);
        writeFloat(bytes, k_slideCooldownOffset, state.motion.slideCooldown);
        bytes[k_jumpBufferOffset] = static_cast<std::byte>(state.motion.jumpBufferTicks);
        bytes[k_coyoteOffset] = static_cast<std::byte>(state.motion.coyoteTicks);
        bytes[k_landingOffset] = static_cast<std::byte>(state.motion.landingTicks);
        writeFloat(bytes, k_leanOffset, state.motion.lean);
        bytes[k_sprintRecoverOffset] = static_cast<std::byte>(state.motion.sprintRecoverTicks);
        bytes[k_mantleTicksOffset] = static_cast<std::byte>(state.motion.mantleTicks);
        writeFloat(bytes, k_mantleHeightOffset, state.motion.mantleHeight);
        bytes[k_kickTicksOffset] = static_cast<std::byte>(state.motion.kickTicks);
        bytes[k_kickCooldownOffset] = static_cast<std::byte>(state.motion.kickCooldownTicks);

        return bytes;
    }

    std::optional<OwnPawnState> decodeOwnPawnState(std::span<const std::byte> bytes)
    {
        if (bytes.size() != k_ownPawnStateBytes || static_cast<uint8_t>(bytes[k_stanceOffset]) > k_maxStance)
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
                    readFloat(bytes, k_planarVelocityOffset + k_floatBytes)},
                .stance = static_cast<PawnStance>(bytes[k_stanceOffset]),
                .isCrouchHeld = (bytes[k_flagsOffset] & k_crouchHeldBit) != std::byte{0},
                .slideSeconds = readFloat(bytes, k_slideSecondsOffset),
                .slideCooldown = readFloat(bytes, k_slideCooldownOffset),
                .isJumpHeld = (bytes[k_flagsOffset] & k_jumpHeldBit) != std::byte{0},
                .jumpBufferTicks = static_cast<uint8_t>(bytes[k_jumpBufferOffset]),
                .coyoteTicks = static_cast<uint8_t>(bytes[k_coyoteOffset]),
                .landingTicks = static_cast<uint8_t>(bytes[k_landingOffset]),
                .lean = math::clamp(readFloat(bytes, k_leanOffset), -1.0f, 1.0f),
                .sprintRecoverTicks = static_cast<uint8_t>(bytes[k_sprintRecoverOffset]),
                .mantleTicks = static_cast<uint8_t>(bytes[k_mantleTicksOffset]),
                .mantleHeight = readFloat(bytes, k_mantleHeightOffset),
                .isVaulting = (bytes[k_flagsOffset] & k_vaultingBit) != std::byte{0},
                .isKickHeld = (bytes[k_flagsOffset] & k_kickHeldBit) != std::byte{0},
                .kickTicks = static_cast<uint8_t>(bytes[k_kickTicksOffset]),
                .kickCooldownTicks = static_cast<uint8_t>(bytes[k_kickCooldownOffset])}};

        const bool isFinite = std::isfinite(state.foot.x) && std::isfinite(state.foot.y) && std::isfinite(state.foot.z)
            && std::isfinite(state.motion.verticalSpeed) && std::isfinite(state.motion.planarVelocity.x)
            && std::isfinite(state.motion.planarVelocity.z) && std::isfinite(state.motion.slideSeconds)
            && std::isfinite(state.motion.slideCooldown) && std::isfinite(readFloat(bytes, k_leanOffset))
            && std::isfinite(state.motion.mantleHeight);

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
        matchStanceHeight(physics, prediction.controller, prediction.motion, settings);
        replay(prediction, physics, settings, delta);
        const math::Vec3 after = physics.footPosition(prediction.controller).value_or(state->foot);

        prediction.visualOffset = prediction.visualOffset + (before - after);

        if (math::length(prediction.visualOffset) > k_maxSmoothedOffset)
        {
            prediction.visualOffset = {};
            placeEntity(prediction, entities, after, settings, true);
        }
        else
        {
            placeEntity(prediction, entities, after, settings, false);
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
            placeEntity(prediction, entities, *move.foot, settings, false);
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
