#include "SnapshotFormat.hpp"

#include "WireFormat.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace lockstep::net
{
    namespace
    {
        constexpr size_t k_entityStateBytes{52};
        constexpr uint8_t k_ownerCount{static_cast<uint8_t>(world::EntityOwner::Player2) + 1};
        constexpr double k_viewTickScale{256.0};
        constexpr uint8_t k_hasValue{1};

        void writeVec3(ByteWriter& writer, const math::Vec3& vector)
        {
            writer.writeF32(vector.x);
            writer.writeF32(vector.y);
            writer.writeF32(vector.z);
        }

        math::Vec3 readVec3(ByteReader& reader)
        {
            const float x = reader.readF32();
            const float y = reader.readF32();
            const float z = reader.readF32();

            return math::Vec3{x, y, z};
        }

        math::Quat readQuat(ByteReader& reader)
        {
            const float x = reader.readF32();
            const float y = reader.readF32();
            const float z = reader.readF32();
            const float w = reader.readF32();

            return math::Quat{x, y, z, w};
        }

        bool isFinite(const math::Transform& transform)
        {
            const std::array<float, 10> values{transform.position.x, transform.position.y, transform.position.z,
                transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w,
                transform.scale.x, transform.scale.y, transform.scale.z};

            return std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); });
        }

        std::optional<EntityState> readEntityState(ByteReader& reader)
        {
            EntityState state{};
            state.hostEntity.index = reader.readU32();
            state.hostEntity.generation = reader.readU32();
            state.archetype = reader.readU16();
            const uint8_t domain = reader.readU8();
            const uint8_t owner = reader.readU8();
            state.transform.position = readVec3(reader);
            state.transform.rotation = readQuat(reader);
            state.transform.scale = readVec3(reader);

            const bool isValid = reader.isValid() && !state.hostEntity.isNull()
                && domain < static_cast<uint8_t>(world::TimeDomain::Count) && owner < k_ownerCount
                && isFinite(state.transform);

            if (!isValid)
            {
                return std::nullopt;
            }

            state.domain = static_cast<world::TimeDomain>(domain);
            state.owner = static_cast<world::EntityOwner>(owner);

            return state;
        }
    } //namespace

    std::vector<std::byte> encodeSnapshot(const Snapshot& snapshot)
    {
        ByteWriter writer(MessageKind::Snapshot);
        writer.writeU64(snapshot.tick);
        writer.writeF32(snapshot.worldScale.scale);
        writer.writeU64(snapshot.worldScale.tick);
        writer.writeU8(snapshot.ackInputTick ? k_hasValue : 0);
        writer.writeU64(snapshot.ackInputTick.value_or(0));
        writer.writeU8(static_cast<uint8_t>(snapshot.clientState.size()));
        writer.writeBytes(snapshot.clientState);
        writer.writeU32(static_cast<uint32_t>(snapshot.entities.size()));

        for (const EntityState& entity : snapshot.entities)
        {
            writer.writeU32(entity.hostEntity.index);
            writer.writeU32(entity.hostEntity.generation);
            writer.writeU16(entity.archetype);
            writer.writeU8(static_cast<uint8_t>(entity.domain));
            writer.writeU8(static_cast<uint8_t>(entity.owner));
            writeVec3(writer, entity.transform.position);
            writer.writeF32(entity.transform.rotation.x);
            writer.writeF32(entity.transform.rotation.y);
            writer.writeF32(entity.transform.rotation.z);
            writer.writeF32(entity.transform.rotation.w);
            writeVec3(writer, entity.transform.scale);
        }

        return writer.take();
    }

    std::optional<Snapshot> decodeSnapshot(std::span<const std::byte> message)
    {
        ByteReader reader(message);
        Snapshot snapshot{};
        snapshot.tick = reader.readU64();
        snapshot.worldScale.scale = reader.readF32();
        snapshot.worldScale.tick = reader.readU64();
        const uint8_t hasAck = reader.readU8();
        const uint64_t ackInputTick = reader.readU64();
        const uint8_t clientStateSize = reader.readU8();

        if (clientStateSize > k_maxClientStateBytes)
        {
            return std::nullopt;
        }

        const std::span<const std::byte> clientState = reader.readBytes(clientStateSize);
        snapshot.clientState.assign(clientState.begin(), clientState.end());

        if (hasAck == k_hasValue)
        {
            snapshot.ackInputTick = ackInputTick;
        }

        const uint32_t count = reader.readU32();

        if (!reader.isValid() || !std::isfinite(snapshot.worldScale.scale) || snapshot.worldScale.scale < 0.0f
            || reader.remaining() != static_cast<size_t>(count) * k_entityStateBytes)
        {
            return std::nullopt;
        }

        snapshot.entities.reserve(count);

        for (uint32_t index = 0; index < count; ++index)
        {
            std::optional<EntityState> entity = readEntityState(reader);

            if (!entity)
            {
                return std::nullopt;
            }

            snapshot.entities.push_back(*entity);
        }

        std::sort(snapshot.entities.begin(), snapshot.entities.end(), [](const EntityState& first, const EntityState& second)
        {
            return entityKey(first.hostEntity) < entityKey(second.hostEntity);
        });

        return snapshot;
    }

    std::vector<std::byte> encodeInputs(std::span<const InputMessage> inputs)
    {
        ByteWriter writer(MessageKind::Input);
        writer.writeU8(static_cast<uint8_t>(inputs.size()));

        for (const InputMessage& input : inputs)
        {
            writer.writeU64(input.tick);
            writer.writeU8(input.viewTick ? k_hasValue : 0);
            writer.writeU64(static_cast<uint64_t>(std::max(input.viewTick.value_or(0.0), 0.0) * k_viewTickScale));
            writer.writeU8(static_cast<uint8_t>(input.bytes.size()));
            writer.writeBytes(input.bytes);
        }

        return writer.take();
    }

    std::optional<std::vector<InputMessage>> decodeInputs(std::span<const std::byte> message)
    {
        ByteReader reader(message);
        const uint8_t count = reader.readU8();

        if (!reader.isValid() || count == 0 || count > k_maxInputsPerMessage)
        {
            return std::nullopt;
        }

        std::vector<InputMessage> inputs;
        inputs.reserve(count);

        for (uint8_t index = 0; index < count; ++index)
        {
            InputMessage input{};
            input.tick = reader.readU64();
            const uint8_t hasViewTick = reader.readU8();
            const uint64_t viewTick = reader.readU64();
            const uint8_t size = reader.readU8();

            const bool isOrdered = inputs.empty() || input.tick > inputs.back().tick;

            if (!reader.isValid() || size > k_maxInputBytes || !isOrdered)
            {
                return std::nullopt;
            }

            if (hasViewTick == k_hasValue)
            {
                input.viewTick = static_cast<double>(viewTick) / k_viewTickScale;
            }

            const std::span<const std::byte> bytes = reader.readBytes(size);
            input.bytes.assign(bytes.begin(), bytes.end());
            inputs.push_back(std::move(input));
        }

        if (!reader.isValid() || reader.remaining() != 0)
        {
            return std::nullopt;
        }

        return inputs;
    }
} //namespace lockstep::net
