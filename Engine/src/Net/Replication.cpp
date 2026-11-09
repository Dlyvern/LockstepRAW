#include "Lockstep/Net/Replication.hpp"

#include "Lockstep/Net/Session.hpp"

#include "SessionState.hpp"
#include "SnapshotBuffer.hpp"
#include "SnapshotFormat.hpp"
#include "WireFormat.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace lockstep::net
{
    struct ReplicationState
    {
        struct ArchetypeSlot
        {
            world::EntityId entity{};
            uint16_t archetype{0};
        };

        ReplicationState(Session& session, world::Entities& entities, world::TimeDomains& timeDomains)
            : session(session)
            , entities(entities)
            , timeDomains(timeDomains)
            , worldScale{.scale = timeDomains.scale(world::TimeDomain::World), .tick = 0}
        {
        }

        Session& session;
        world::Entities& entities;
        world::TimeDomains& timeDomains;
        ReplicatedTimeScale worldScale{};
        float interpolationDelay{k_defaultInterpolationDelayTicks};
        std::vector<ArchetypeSlot> archetypes;
        std::vector<std::byte> localInput;
        bool hasLocalInput{false};
        std::optional<InputMessage> remoteInput;
        SnapshotBuffer snapshots;
        std::vector<MirroredEntity> mirrored;
        ReplicationStats counters{};
        bool sendFailureReported{false};
        bool spawnFailureReported{false};
    };

    namespace
    {
        void log(const ReplicationState& state, const std::string& message)
        {
            std::clog << "Net (" << toString(state.session.role()) << "): " << message << '\n';
        }

        std::string describe(world::EntityId id)
        {
            return std::to_string(id.index) + ":" + std::to_string(id.generation);
        }

        bool isConnectedAs(const Session& session, Role role)
        {
            return session.role() == role && session.state() == ConnectionState::Connected;
        }

        void reportSend(ReplicationState& state, const Status& sent, const char* what)
        {
            if (sent)
            {
                state.sendFailureReported = false;
                return;
            }

            const bool isReportedBySession = sent.error().code == ErrorCode::NetworkFailure;

            if (!state.sendFailureReported && !isReportedBySession)
            {
                state.sendFailureReported = true;
                log(state, std::string("sending ") + what + " failed: " + sent.error().message);
            }
        }

        void setWorldScale(ReplicationState& state, ReplicatedTimeScale scale)
        {
            if (scale.scale == state.worldScale.scale)
            {
                return;
            }

            state.worldScale = scale;
            log(state, "World scale x" + std::to_string(scale.scale) + " from host tick " + std::to_string(scale.tick));
        }

        uint16_t archetypeOf(const ReplicationState& state, world::EntityId entity)
        {
            if (entity.index >= state.archetypes.size() || state.archetypes[entity.index].entity != entity)
            {
                return 0;
            }

            return state.archetypes[entity.index].archetype;
        }

        Snapshot captureSnapshot(const ReplicationState& state, uint64_t tick)
        {
            Snapshot snapshot{.tick = tick, .worldScale = state.worldScale, .entities = {}};
            const std::span<const world::Entity> entities = state.entities.all();
            const std::span<const world::EntityId> ids = state.entities.ids();

            for (size_t index = 0; index < entities.size(); ++index)
            {
                const world::Entity& entity = entities[index];

                if (entity.isClientOnly())
                {
                    continue;
                }

                snapshot.entities.push_back(EntityState{
                    .hostEntity = ids[index],
                    .archetype = archetypeOf(state, ids[index]),
                    .domain = entity.domain(),
                    .owner = entity.owner(),
                    .transform = entity.transform()});
            }

            return snapshot;
        }

        void sendSnapshot(ReplicationState& state, uint64_t tick)
        {
            const std::vector<std::byte> message = encodeSnapshot(captureSnapshot(state, tick));
            const Status sent = sendEngineMessage(sessionState(state.session), message, Delivery::Unreliable);
            reportSend(state, sent, "a snapshot");

            if (sent)
            {
                ++state.counters.snapshotsSent;
            }
        }

        void sendInput(ReplicationState& state, uint64_t tick)
        {
            const std::vector<std::byte> message = encodeInput(tick, state.localInput);
            const Status sent = sendEngineMessage(sessionState(state.session), message, Delivery::Unreliable);
            reportSend(state, sent, "input");

            if (sent)
            {
                ++state.counters.inputsSent;
            }
        }

        std::optional<MirroredEntity> spawnMirror(ReplicationState& state, const EntityState& source)
        {
            auto created = state.entities.create({
                .transform = source.transform,
                .domain = source.domain,
                .owner = source.owner,
                .clientOnly = false});

            if (!created)
            {
                if (!state.spawnFailureReported)
                {
                    state.spawnFailureReported = true;
                    log(state, "could not mirror host entity " + describe(source.hostEntity) + ": " + created.error().message);
                }

                return std::nullopt;
            }

            log(state, "mirrored host entity " + describe(source.hostEntity) + " (archetype "
                + std::to_string(source.archetype) + ") as " + describe(created.value()));

            return MirroredEntity{.entity = created.value(), .hostEntity = source.hostEntity, .archetype = source.archetype};
        }

        void despawnMirror(ReplicationState& state, const MirroredEntity& mirror)
        {
            state.entities.destroy(mirror.entity);
            log(state, "host entity " + describe(mirror.hostEntity) + " is gone, destroyed " + describe(mirror.entity));
        }

        std::optional<MirroredEntity> updateMirror(ReplicationState& state, MirroredEntity mirror, const EntityState& source)
        {
            world::Entity* entity = state.entities.find(mirror.entity);

            if (entity == nullptr)
            {
                return spawnMirror(state, source);
            }

            entity->transform() = source.transform;
            mirror.archetype = source.archetype;

            return mirror;
        }

        void applyMirror(ReplicationState& state, const std::vector<EntityState>& sampled)
        {
            std::vector<MirroredEntity> next;
            next.reserve(sampled.size());
            auto current = state.mirrored.begin();

            for (const EntityState& source : sampled)
            {
                const uint64_t key = entityKey(source.hostEntity);

                for (; current != state.mirrored.end() && entityKey(current->hostEntity) < key; ++current)
                {
                    despawnMirror(state, *current);
                }

                const bool isKnown = current != state.mirrored.end() && entityKey(current->hostEntity) == key;
                const std::optional<MirroredEntity> mirror = isKnown ? updateMirror(state, *current, source)
                                                                     : spawnMirror(state, source);

                if (isKnown)
                {
                    ++current;
                }

                if (mirror)
                {
                    next.push_back(*mirror);
                }
            }

            for (; current != state.mirrored.end(); ++current)
            {
                despawnMirror(state, *current);
            }

            state.mirrored = std::move(next);
        }

        void clearMirror(ReplicationState& state)
        {
            for (const MirroredEntity& mirror : state.mirrored)
            {
                state.entities.destroy(mirror.entity);
            }

            if (!state.mirrored.empty())
            {
                log(state, "connection ended, destroyed " + std::to_string(state.mirrored.size()) + " mirrored entities");
            }

            state.mirrored.clear();
            state.snapshots.clear();
        }

        void receiveSnapshots(ReplicationState& state)
        {
            for (const std::vector<std::byte>& message : sessionState(state.session).engineMessages)
            {
                if (messageKind(message) != MessageKind::Snapshot)
                {
                    continue;
                }

                std::optional<Snapshot> snapshot = decodeSnapshot(message);

                if (snapshot)
                {
                    ++state.counters.snapshotsReceived;
                    state.snapshots.insert(std::move(*snapshot));
                }
            }

            state.snapshots.advanceClock(state.interpolationDelay);
            std::optional<SampledWorld> world = state.snapshots.sample();

            if (!world)
            {
                return;
            }

            applyMirror(state, world->entities);
            setWorldScale(state, world->worldScale);
            state.timeDomains.setScale(world::TimeDomain::World, world->worldScale.scale);
        }

        void receiveInputs(ReplicationState& state)
        {
            for (const std::vector<std::byte>& message : sessionState(state.session).engineMessages)
            {
                if (messageKind(message) != MessageKind::Input)
                {
                    continue;
                }

                std::optional<InputMessage> input = decodeInput(message);

                if (!input)
                {
                    continue;
                }

                ++state.counters.inputsReceived;

                if (!state.remoteInput || input->tick > state.remoteInput->tick)
                {
                    state.remoteInput = std::move(input);
                }
            }
        }
    } //namespace

    Replication::Replication(Session& session, world::Entities& entities, world::TimeDomains& timeDomains)
        : m_state(std::make_unique<ReplicationState>(session, entities, timeDomains))
    {
    }

    Replication::~Replication() = default;

    Status Replication::setArchetype(world::EntityId entity, uint16_t archetype)
    {
        if (!m_state->entities.isAlive(entity))
        {
            return Error{ErrorCode::InvalidHandle, "Replication::setArchetype: the entity is not alive"};
        }

        if (entity.index >= m_state->archetypes.size())
        {
            m_state->archetypes.resize(static_cast<size_t>(entity.index) + 1);
        }

        m_state->archetypes[entity.index] = {.entity = entity, .archetype = archetype};

        return {};
    }

    Status Replication::setLocalInput(std::span<const std::byte> input)
    {
        if (input.size() > k_maxInputBytes)
        {
            return Error{ErrorCode::InvalidArgument, "Replication::setLocalInput: input larger than k_maxInputBytes"};
        }

        m_state->localInput.assign(input.begin(), input.end());
        m_state->hasLocalInput = true;

        return {};
    }

    std::optional<RemoteInput> Replication::remoteInput() const
    {
        if (!m_state->remoteInput)
        {
            return std::nullopt;
        }

        return RemoteInput{.tick = m_state->remoteInput->tick, .bytes = m_state->remoteInput->bytes};
    }

    std::span<const MirroredEntity> Replication::mirrored() const
    {
        return m_state->mirrored;
    }

    ReplicatedTimeScale Replication::worldTimeScale() const
    {
        return m_state->worldScale;
    }

    void Replication::setInterpolationDelay(float ticks)
    {
        m_state->interpolationDelay = std::clamp(ticks, 0.0f, k_maxInterpolationDelayTicks);
    }

    float Replication::interpolationDelay() const
    {
        return m_state->interpolationDelay;
    }

    ReplicationStats Replication::stats() const
    {
        ReplicationStats stats = m_state->counters;
        stats.bufferedSnapshots = m_state->snapshots.size();
        stats.newestSnapshotTick = m_state->snapshots.newestTick();
        stats.renderTick = m_state->snapshots.renderTick();
        stats.mirroredEntities = m_state->mirrored.size();

        return stats;
    }

    void Replication::sendTick(uint64_t tick)
    {
        ReplicationState& state = *m_state;

        if (state.session.role() != Role::Client)
        {
            setWorldScale(state, {.scale = state.timeDomains.scale(world::TimeDomain::World), .tick = tick + 1});
        }

        if (isConnectedAs(state.session, Role::Host))
        {
            sendSnapshot(state, tick);
        }
        else if (isConnectedAs(state.session, Role::Client) && state.hasLocalInput)
        {
            sendInput(state, tick);
        }

        state.localInput.clear();
        state.hasLocalInput = false;
    }

    void Replication::receiveTick()
    {
        ReplicationState& state = *m_state;

        if (isConnectedAs(state.session, Role::Client))
        {
            receiveSnapshots(state);
        }
        else if (!state.mirrored.empty() || state.snapshots.size() > 0)
        {
            clearMirror(state);
        }

        if (isConnectedAs(state.session, Role::Host))
        {
            receiveInputs(state);
        }
        else
        {
            state.remoteInput.reset();
        }

        if (state.session.state() != ConnectionState::Connected)
        {
            state.counters = ReplicationStats{};
        }
    }
} //namespace lockstep::net
