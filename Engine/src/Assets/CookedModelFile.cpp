#include "CookedFormat.hpp"

#include "BinaryStream.hpp"
#include "CookedFields.hpp"

#include <algorithm>

namespace lockstep::assets
{
    namespace
    {
        constexpr std::size_t k_minimumJointBytes{4 + 4 + 40 + 64};
        constexpr std::size_t k_minimumClipBytes{4 + 4 + 4};
        constexpr std::size_t k_minimumChannelBytes{4 + 1 + 1 + 4 + 4};
        constexpr std::size_t k_minimumPartBytes{4 + 4};
        constexpr std::size_t k_keyTimeBytes{sizeof(float)};
        constexpr std::size_t k_keyValueBytes{4 * sizeof(float)};
        constexpr std::size_t k_minimumLimbBytes{1 + 4 + 4};
        constexpr std::size_t k_hullPointBytes{3 * sizeof(float) + 4 + 4 * sizeof(float)};

        Error modelError(std::string_view path, std::string_view what)
        {
            return Error{ErrorCode::AssetLoadFailed, "model '" + std::string(path) + "': " + std::string(what)};
        }

        void writeChannel(BinaryWriter& writer, const anim::AnimationChannel& channel)
        {
            writer.write(channel.joint);
            writer.write(static_cast<uint8_t>(channel.target));
            writer.write(static_cast<uint8_t>(channel.interpolation));
            writer.write(static_cast<uint32_t>(channel.times.size()));
            writer.writeBytes(std::as_bytes(std::span<const float>(channel.times)));
            writer.write(static_cast<uint32_t>(channel.values.size()));

            for (const math::Vec4& value : channel.values)
            {
                writer.write(value.x);
                writer.write(value.y);
                writer.write(value.z);
                writer.write(value.w);
            }
        }

        bool validLimbs(const CookedModel& model, std::size_t jointCount)
        {
            const auto validLimb = [](anim::Limb limb)
            {
                return static_cast<std::size_t>(limb) < anim::k_limbCount || limb == anim::Limb::None;
            };

            for (const CookedSkinnedPart& part : model.parts)
            {
                if (!validLimb(part.limb) || part.role > anim::LimbPartRole::PieceCap)
                {
                    return false;
                }
            }

            return std::all_of(model.limbs.begin(), model.limbs.end(), [&](const anim::LimbInfo& limb)
            {
                return static_cast<std::size_t>(limb.limb) < anim::k_limbCount && limb.rootJoint < jointCount
                    && std::all_of(limb.hull.begin(), limb.hull.end(), [jointCount](const anim::LimbHullPoint& point)
                    {
                        return std::all_of(point.joints.begin(), point.joints.end(),
                            [jointCount](uint8_t joint) { return joint < jointCount; });
                    });
            });
        }

        void readChannel(BinaryReader& reader, anim::AnimationChannel& channel)
        {
            uint8_t target = 0;
            uint8_t interpolation = 0;
            uint32_t count = 0;
            reader.read(channel.joint);
            reader.read(target);
            reader.read(interpolation);
            channel.target = static_cast<anim::ChannelTarget>(target);
            channel.interpolation = static_cast<anim::Interpolation>(interpolation);

            if (reader.readCount(count, k_keyTimeBytes))
            {
                channel.times.resize(count);
                reader.readBytes(std::as_writable_bytes(std::span<float>(channel.times)));
            }

            if (reader.readCount(count, k_keyValueBytes))
            {
                channel.values.resize(count);

                for (math::Vec4& value : channel.values)
                {
                    reader.read(value.x);
                    reader.read(value.y);
                    reader.read(value.z);
                    reader.read(value.w);
                }
            }
        }

        void writeLimb(BinaryWriter& writer, const anim::LimbInfo& limb)
        {
            writer.write(static_cast<uint8_t>(limb.limb));
            writer.write(limb.rootJoint);
            writer.write(static_cast<uint32_t>(limb.hull.size()));

            for (const anim::LimbHullPoint& point : limb.hull)
            {
                writeVec3(writer, point.position);

                for (const uint8_t joint : point.joints)
                {
                    writer.write(joint);
                }

                for (const float weight : point.weights)
                {
                    writer.write(weight);
                }
            }
        }

        void readHullPoint(BinaryReader& reader, anim::LimbHullPoint& point)
        {
            readVec3(reader, point.position);

            for (uint8_t& joint : point.joints)
            {
                reader.read(joint);
            }

            for (float& weight : point.weights)
            {
                reader.read(weight);
            }
        }

        template<typename T, typename ReadOne>
        void readArray(BinaryReader& reader, std::vector<T>& items, std::size_t minimumBytes, ReadOne readOne)
        {
            uint32_t count = 0;

            if (!reader.readCount(count, minimumBytes))
            {
                return;
            }

            items.resize(count);

            for (T& item : items)
            {
                readOne(reader, item);
            }
        }

        bool validChannel(const anim::AnimationChannel& channel, std::size_t jointCount)
        {
            const bool isCubic = channel.interpolation == anim::Interpolation::CubicSpline;
            const std::size_t stride = isCubic ? 3 : 1;

            return channel.joint < jointCount && channel.target <= anim::ChannelTarget::Scale
                && channel.interpolation <= anim::Interpolation::CubicSpline && !channel.times.empty()
                && channel.values.size() == channel.times.size() * stride
                && std::is_sorted(channel.times.begin(), channel.times.end());
        }

        bool validModel(const CookedModel& model)
        {
            const std::size_t jointCount = model.skeleton.joints.size();

            if (jointCount == 0 || jointCount > anim::k_maxJoints || model.jointRadii.size() != jointCount)
            {
                return false;
            }

            for (std::size_t index = 0; index < jointCount; ++index)
            {
                const uint32_t parent = model.skeleton.joints[index].parent;

                if (parent != anim::k_noJoint && parent >= index)
                {
                    return false;
                }
            }

            if (!validLimbs(model, jointCount))
            {
                return false;
            }

            return std::all_of(model.clips.begin(), model.clips.end(), [jointCount](const anim::AnimationClip& clip)
            {
                return std::all_of(clip.channels.begin(), clip.channels.end(),
                    [jointCount](const anim::AnimationChannel& channel) { return validChannel(channel, jointCount); });
            });
        }
    } //namespace

    std::vector<std::byte> writeCookedModel(const CookedModel& model)
    {
        BinaryWriter writer;
        writer.write(k_modelMagic);
        writer.write(k_cookedVersion);
        writer.writeString(model.name);
        writeMatrix(writer, model.skeleton.rootMatrix);
        writer.write(static_cast<uint32_t>(model.skeleton.joints.size()));

        for (const anim::Joint& joint : model.skeleton.joints)
        {
            writer.writeString(joint.name);
            writer.write(joint.parent);
            writeTransform(writer, joint.rest);
            writeMatrix(writer, joint.inverseBind);
        }

        writer.write(static_cast<uint32_t>(model.jointRadii.size()));
        writer.writeBytes(std::as_bytes(std::span<const float>(model.jointRadii)));
        writeBounds(writer, model.bindBounds);
        writer.write(static_cast<uint32_t>(model.clips.size()));

        for (const anim::AnimationClip& clip : model.clips)
        {
            writer.writeString(clip.name);
            writer.write(clip.duration);
            writer.write(static_cast<uint32_t>(clip.channels.size()));

            for (const anim::AnimationChannel& channel : clip.channels)
            {
                writeChannel(writer, channel);
            }
        }

        writer.write(static_cast<uint32_t>(model.parts.size()));

        for (const CookedSkinnedPart& part : model.parts)
        {
            writer.writeString(part.path);
            writer.write(part.material);
        }

        for (const CookedSkinnedPart& part : model.parts)
        {
            writer.writeString(part.materialName);
        }

        for (const CookedSkinnedPart& part : model.parts)
        {
            writer.write(static_cast<uint8_t>(part.limb));
            writer.write(static_cast<uint8_t>(part.role));
        }

        writer.write(static_cast<uint32_t>(model.limbs.size()));

        for (const anim::LimbInfo& limb : model.limbs)
        {
            writeLimb(writer, limb);
        }

        return writer.take();
    }

    Result<CookedModel> readCookedModel(std::span<const std::byte> bytes, std::string_view path)
    {
        BinaryReader reader(bytes);
        std::array<char, 4> magic{};
        uint32_t version = 0;

        if (!reader.read(magic) || magic != k_modelMagic || !reader.read(version))
        {
            return modelError(path, "not a cooked model");
        }

        if (version != k_cookedVersion)
        {
            return modelError(path, "format version " + std::to_string(version) + " is not supported; re-cook it");
        }

        CookedModel model{};
        reader.readString(model.name);
        readMatrix(reader, model.skeleton.rootMatrix);
        readArray(reader, model.skeleton.joints, k_minimumJointBytes, [](BinaryReader& source, anim::Joint& joint)
        {
            source.readString(joint.name);
            source.read(joint.parent);
            readTransform(source, joint.rest);
            readMatrix(source, joint.inverseBind);
        });

        uint32_t radiusCount = 0;

        if (reader.readCount(radiusCount, sizeof(float)))
        {
            model.jointRadii.resize(radiusCount);
            reader.readBytes(std::as_writable_bytes(std::span<float>(model.jointRadii)));
        }

        readBounds(reader, model.bindBounds);
        readArray(reader, model.clips, k_minimumClipBytes, [](BinaryReader& source, anim::AnimationClip& clip)
        {
            source.readString(clip.name);
            source.read(clip.duration);
            readArray(source, clip.channels, k_minimumChannelBytes, readChannel);
        });
        readArray(reader, model.parts, k_minimumPartBytes, [](BinaryReader& source, CookedSkinnedPart& part)
        {
            source.readString(part.path);
            source.read(part.material);
        });

        if (reader.ok() && !reader.atEnd())
        {
            for (CookedSkinnedPart& part : model.parts)
            {
                reader.readString(part.materialName);
            }
        }

        if (reader.ok() && !reader.atEnd())
        {
            for (CookedSkinnedPart& part : model.parts)
            {
                uint8_t limb = 0;
                uint8_t role = 0;
                reader.read(limb);
                reader.read(role);
                part.limb = static_cast<anim::Limb>(limb);
                part.role = static_cast<anim::LimbPartRole>(role);
            }

            readArray(reader, model.limbs, k_minimumLimbBytes, [](BinaryReader& source, anim::LimbInfo& limb)
            {
                uint8_t value = 0;
                source.read(value);
                limb.limb = static_cast<anim::Limb>(value);
                source.read(limb.rootJoint);
                readArray(source, limb.hull, k_hullPointBytes, readHullPoint);
            });
        }

        if (!reader.ok() || !reader.atEnd())
        {
            return modelError(path, "the file is truncated or damaged");
        }

        if (!validModel(model))
        {
            return modelError(path, "the skeleton or a clip is damaged");
        }

        return model;
    }
} //namespace lockstep::assets
