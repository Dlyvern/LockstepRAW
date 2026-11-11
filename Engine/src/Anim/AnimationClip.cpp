#include "Lockstep/Anim/AnimationClip.hpp"

#include <algorithm>
#include <cctype>

namespace lockstep::anim
{
    namespace
    {
        constexpr std::size_t k_cubicStride{3};

        math::Quat toQuat(const math::Vec4& value)
        {
            return math::Quat{value.x, value.y, value.z, value.w};
        }

        math::Vec4 toVec4(const math::Quat& value)
        {
            return math::Vec4{value.x, value.y, value.z, value.w};
        }

        math::Vec4 keyValue(const AnimationChannel& channel, std::size_t key)
        {
            return channel.interpolation == Interpolation::CubicSpline ? channel.values[key * k_cubicStride + 1]
                : channel.values[key];
        }

        math::Vec4 hermite(const AnimationChannel& channel, std::size_t key, float span, float u)
        {
            const math::Vec4 start = keyValue(channel, key);
            const math::Vec4 end = keyValue(channel, key + 1);
            const math::Vec4 outTangent = channel.values[key * k_cubicStride + 2] * span;
            const math::Vec4 inTangent = channel.values[(key + 1) * k_cubicStride] * span;
            const float u2 = u * u;
            const float u3 = u2 * u;

            return start * (2.0f * u3 - 3.0f * u2 + 1.0f) + outTangent * (u3 - 2.0f * u2 + u)
                + end * (-2.0f * u3 + 3.0f * u2) + inTangent * (u3 - u2);
        }

        math::Vec4 sampleChannel(const AnimationChannel& channel, float seconds)
        {
            const std::vector<float>& times = channel.times;
            const std::size_t last = times.size() - 1;

            if (seconds <= times.front())
            {
                return keyValue(channel, 0);
            }

            if (seconds >= times[last])
            {
                return keyValue(channel, last);
            }

            const std::size_t key = static_cast<std::size_t>(
                std::upper_bound(times.begin(), times.end(), seconds) - times.begin()) - 1;
            const float span = times[key + 1] - times[key];
            const float u = span > 0.0f ? (seconds - times[key]) / span : 0.0f;
            const bool isRotation = channel.target == ChannelTarget::Rotation;

            switch (channel.interpolation)
            {
                case Interpolation::Step:
                {
                    return keyValue(channel, key);
                }
                case Interpolation::Linear:
                {
                    if (isRotation)
                    {
                        return toVec4(math::slerp(toQuat(keyValue(channel, key)), toQuat(keyValue(channel, key + 1)), u));
                    }

                    return math::lerp(keyValue(channel, key), keyValue(channel, key + 1), u);
                }
                case Interpolation::CubicSpline:
                {
                    const math::Vec4 value = hermite(channel, key, span, u);
                    return isRotation ? toVec4(math::normalize(toQuat(value))) : value;
                }
            }

            return keyValue(channel, key);
        }

        bool isUsable(const AnimationChannel& channel)
        {
            const std::size_t stride = channel.interpolation == Interpolation::CubicSpline ? k_cubicStride : 1;

            return !channel.times.empty() && channel.values.size() == channel.times.size() * stride;
        }

        bool equalsIgnoringCase(std::string_view left, std::string_view right)
        {
            return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
                [](char a, char b)
                {
                    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                });
        }
    } //namespace

    std::optional<math::Quat> sampleJointRotation(const AnimationClip& clip, uint32_t joint, float seconds)
    {
        for (const AnimationChannel& channel : clip.channels)
        {
            if (channel.joint == joint && channel.target == ChannelTarget::Rotation && isUsable(channel))
            {
                return math::normalize(toQuat(sampleChannel(channel, seconds)));
            }
        }

        return std::nullopt;
    }

    void sampleClip(const AnimationClip& clip, float seconds, Pose& pose)
    {
        for (const AnimationChannel& channel : clip.channels)
        {
            if (channel.joint >= pose.joints.size() || !isUsable(channel))
            {
                continue;
            }

            const math::Vec4 value = sampleChannel(channel, seconds);
            math::Transform& joint = pose.joints[channel.joint];

            switch (channel.target)
            {
                case ChannelTarget::Translation:
                {
                    joint.position = math::Vec3{value.x, value.y, value.z};
                    break;
                }
                case ChannelTarget::Rotation:
                {
                    joint.rotation = toQuat(value);
                    break;
                }
                case ChannelTarget::Scale:
                {
                    joint.scale = math::Vec3{value.x, value.y, value.z};
                    break;
                }
            }
        }
    }

    void blendPoses(const Pose& from, const Pose& to, float weight, Pose& result)
    {
        const float t = std::clamp(weight, 0.0f, 1.0f);
        const std::size_t count = std::min(from.joints.size(), to.joints.size());
        result.joints.resize(count);

        for (std::size_t index = 0; index < count; ++index)
        {
            result.joints[index] = math::interpolate(from.joints[index], to.joints[index], t);
        }
    }

    std::optional<uint32_t> findClip(std::span<const AnimationClip> clips, std::string_view name)
    {
        for (std::size_t index = 0; index < clips.size(); ++index)
        {
            if (equalsIgnoringCase(clips[index].name, name))
            {
                return static_cast<uint32_t>(index);
            }
        }

        return std::nullopt;
    }
} //namespace lockstep::anim
