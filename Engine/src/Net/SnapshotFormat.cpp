#include "SnapshotFormat.hpp"

#include "WireFormat.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace lockstep::net
{
    namespace
    {
        constexpr size_t k_entityStateBytes{52};
        constexpr uint8_t k_ownerCount{static_cast<uint8_t>(world::EntityOwner::Player2) + 1};

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

    std::vector<std::byte> encodeInput(uint64_t tick, std::span<const std::byte> input)
    {
        ByteWriter writer(MessageKind::Input);
        writer.writeU64(tick);
        writer.writeBytes(input);

        return writer.take();
    }

    std::optional<InputMessage> decodeInput(std::span<const std::byte> message)
    {
        ByteReader reader(message);
        InputMessage input{};
        input.tick = reader.readU64();

        if (!reader.isValid() || reader.remaining() > k_maxInputBytes)
        {
            return std::nullopt;
        }

        const std::span<const std::byte> bytes = reader.readBytes(reader.remaining());
        input.bytes.assign(bytes.begin(), bytes.end());

        return input;
    }
} //namespace lockstep::net
