#ifndef LOCKSTEP_GAME_PLAYER_PREDICTION_HPP
#define LOCKSTEP_GAME_PLAYER_PREDICTION_HPP

#include "Player/Pawns.hpp"
#include "Player/PlayerCommand.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>
#include <Lockstep/World/Entities.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>

namespace lockstep::net
{
    struct InputAck;
} //namespace lockstep::net

namespace game
{
    // Bytes an encoded OwnPawnState takes.
    inline constexpr std::size_t k_ownPawnStateBytes{50};

    // Smallest gap between the predicted and the host's foot position, in metres, that counts as
    // a misprediction and makes the client rewind and replay.
    inline constexpr float k_correctionThreshold{0.01f};

    // The host's state of the client's pawn after the newest client input it applied, sent with
    // every snapshot so the client can check its prediction.
    // isAlive: the pawn exists; the other fields are meaningless otherwise.
    // life:    the pawn's spawn counter (Pawn::life).
    // foot:    its controller's foot position.
    // motion:  its velocity, grounding, stance, slide and jump timers.
    struct OwnPawnState
    {
        bool isAlive{false};
        uint8_t life{0};
        math::Vec3 foot{0.0f, 0.0f, 0.0f};
        PawnMotion motion{};
    };

    // One local command the client applied to its predicted pawn, kept until the host
    // acknowledges it.
    // tick:    the client tick it belongs to (the Replication input tick).
    // command: the command as the host decodes it.
    // foot:    the predicted foot position after it; nothing if the pawn did not exist.
    // motion:  the predicted motion after it.
    struct PredictedMove
    {
        uint64_t tick{0};
        PlayerCommand command{};
        std::optional<math::Vec3> foot;
        PawnMotion motion{};
    };

    // The client's prediction of its own pawn: a client-only entity with its own controller in
    // the client's copy of the level, moved by local commands at once and reconciled with the
    // host's state when acknowledgements arrive.
    // entity:       the client-only pawn entity; null while the pawn is dead or not spawned.
    // controller:   its controller; null with the entity.
    // life:         the host pawn life it was created for.
    // motion:       velocity and grounding after the newest move.
    // history:      unacknowledged moves, oldest first.
    // ackedTick:    the newest input tick the host acknowledged.
    // visualOffset: what is still shown of the last corrections, added to the drawn position
    //               and faded out every tick so corrections do not snap.
    // corrections:  rewinds made, for the dev panel.
    // lastError:    the gap that caused the newest rewind, in metres.
    // lastReplayed: moves replayed by the newest rewind.
    struct Prediction
    {
        lockstep::world::EntityId entity{};
        lockstep::physics::ControllerId controller{};
        uint8_t life{0};
        PawnMotion motion{};
        std::deque<PredictedMove> history;
        std::optional<uint64_t> ackedTick;
        math::Vec3 visualOffset{0.0f, 0.0f, 0.0f};
        uint32_t corrections{0};
        float lastError{0.0f};
        std::size_t lastReplayed{0};
    };

    // Packs the host's state of the client's pawn.
    // state:   the state.
    // Returns: the bytes for Replication::setClientState().
    [[nodiscard]] std::array<std::byte, k_ownPawnStateBytes> encodeOwnPawnState(const OwnPawnState& state);

    // Unpacks the host's state of the client's pawn.
    // bytes:   InputAck::clientState.
    // Returns: the state, or nothing if `bytes` is not a valid state.
    [[nodiscard]] std::optional<OwnPawnState> decodeOwnPawnState(std::span<const std::byte> bytes);

    // Client: checks the prediction against the host's state in an acknowledgement. Creates the
    // predicted pawn when the host's is alive (or respawned) and removes it when it died. If the
    // acknowledged move was predicted more than k_correctionThreshold off, moves the controller
    // to the host's state and replays the unacknowledged moves, keeping the visible jump in
    // visualOffset. Drops acknowledged moves.
    // prediction: the prediction.
    // entities:   the entity store, for the client-only pawn entity.
    // physics:    the client's physics world.
    // ack:        the acknowledgement from Replication::inputAck().
    // settings:   movement settings.
    // delta:      the Player domain's delta for one tick, in seconds.
    // Returns:    the misprediction in metres when it rewound, or nothing.
    [[nodiscard]] std::optional<float> reconcilePrediction(Prediction& prediction, lockstep::world::Entities& entities,
        lockstep::physics::PhysicsWorld& physics, const lockstep::net::InputAck& ack, const PawnSettings& settings,
        float delta);

    // Client: applies this tick's command to the predicted pawn (when it exists) and remembers it
    // for replays. Fades the visual offset.
    // prediction: the prediction.
    // entities:   the entity store.
    // physics:    the client's physics world.
    // tick:       this tick, the same number the input is sent with.
    // command:    the command as the host will decode it.
    // settings:   movement settings.
    // delta:      the Player domain's delta for this tick, in seconds.
    void predictPawn(Prediction& prediction, lockstep::world::Entities& entities,
        lockstep::physics::PhysicsWorld& physics, uint64_t tick, const PlayerCommand& command,
        const PawnSettings& settings, float delta);

    // Client: removes the predicted pawn and forgets the history (e.g. on disconnect).
    // prediction: the prediction.
    // entities:   the entity store.
    // physics:    the client's physics world.
    void clearPrediction(Prediction& prediction, lockstep::world::Entities& entities,
        lockstep::physics::PhysicsWorld& physics);

    // prediction: the prediction.
    // entities:   the entity store.
    // alpha:      FrameInfo::alpha.
    // Returns:    where to show the predicted pawn this frame (interpolated plus the visual
    //             offset), or nothing while it does not exist.
    [[nodiscard]] std::optional<math::Transform> predictedTransform(const Prediction& prediction,
        const lockstep::world::Entities& entities, float alpha);
} //namespace game

#endif //LOCKSTEP_GAME_PLAYER_PREDICTION_HPP
