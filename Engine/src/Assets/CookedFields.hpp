#ifndef LOCKSTEP_ASSETS_COOKED_FIELDS_HPP
#define LOCKSTEP_ASSETS_COOKED_FIELDS_HPP

#include "Lockstep/Core/Bounds.hpp"
#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Transform.hpp"

#include "BinaryStream.hpp"

// Field-by-field encoding of the math types cooked files store (level and model files), so equal
// values always give equal bytes.
namespace lockstep::assets
{
    // Writes x, y, z.
    // writer: the stream.
    // vector: the value.
    inline void writeVec3(BinaryWriter& writer, const math::Vec3& vector)
    {
        writer.write(vector.x);
        writer.write(vector.y);
        writer.write(vector.z);
    }

    // Reads what writeVec3() wrote.
    // reader: the stream.
    // vector: receives the value.
    inline void readVec3(BinaryReader& reader, math::Vec3& vector)
    {
        reader.read(vector.x);
        reader.read(vector.y);
        reader.read(vector.z);
    }

    // Writes min then max.
    // writer: the stream.
    // bounds: the value.
    inline void writeBounds(BinaryWriter& writer, const math::Aabb& bounds)
    {
        writeVec3(writer, bounds.min);
        writeVec3(writer, bounds.max);
    }

    // Reads what writeBounds() wrote.
    // reader: the stream.
    // bounds: receives the value.
    inline void readBounds(BinaryReader& reader, math::Aabb& bounds)
    {
        readVec3(reader, bounds.min);
        readVec3(reader, bounds.max);
    }

    // Writes position, rotation xyzw, scale.
    // writer:    the stream.
    // transform: the value.
    inline void writeTransform(BinaryWriter& writer, const math::Transform& transform)
    {
        writeVec3(writer, transform.position);
        writer.write(transform.rotation.x);
        writer.write(transform.rotation.y);
        writer.write(transform.rotation.z);
        writer.write(transform.rotation.w);
        writeVec3(writer, transform.scale);
    }

    // Reads what writeTransform() wrote.
    // reader:    the stream.
    // transform: receives the value.
    inline void readTransform(BinaryReader& reader, math::Transform& transform)
    {
        readVec3(reader, transform.position);
        reader.read(transform.rotation.x);
        reader.read(transform.rotation.y);
        reader.read(transform.rotation.z);
        reader.read(transform.rotation.w);
        readVec3(reader, transform.scale);
    }

    // Writes the 16 floats, column by column.
    // writer: the stream.
    // matrix: the value.
    inline void writeMatrix(BinaryWriter& writer, const math::Mat4& matrix)
    {
        for (std::size_t index = 0; index < 16; ++index)
        {
            writer.write(matrix.data()[index]);
        }
    }

    // Reads what writeMatrix() wrote.
    // reader: the stream.
    // matrix: receives the value.
    inline void readMatrix(BinaryReader& reader, math::Mat4& matrix)
    {
        for (std::size_t index = 0; index < 16; ++index)
        {
            reader.read(matrix.data()[index]);
        }
    }
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_COOKED_FIELDS_HPP
