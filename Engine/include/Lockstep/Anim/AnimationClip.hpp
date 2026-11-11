#ifndef LOCKSTEP_ANIM_ANIMATION_CLIP_HPP
#define LOCKSTEP_ANIM_ANIMATION_CLIP_HPP

#include "Lockstep/Anim/Skeleton.hpp"
#include "Lockstep/Core/Math.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::anim
{
    // Which part of a joint's local transform a channel animates.
    enum class ChannelTarget : uint8_t
    {
        Translation = 0,
        Rotation,
        Scale
    };

    // How a channel moves between keys (glTF sampler interpolation).
    // Step:        holds each key's value until the next key.
    // Linear:      linear for translation and scale, spherical for rotation.
    // CubicSpline: Hermite spline through keys with in and out tangents.
    enum class Interpolation : uint8_t
    {
        Step = 0,
        Linear,
        CubicSpline
    };

    // Keys for one transform part of one joint.
    // joint:         index into Skeleton::joints.
    // target:        translation, rotation or scale.
    // interpolation: how values between keys are found.
    // times:         key times in seconds, increasing.
    // values:        per key xyz (translation, scale) or xyzw (rotation quaternion). For
    //                CubicSpline three per key: in-tangent, value, out-tangent.
    struct AnimationChannel
    {
        uint32_t joint{0};
        ChannelTarget target{ChannelTarget::Translation};
        Interpolation interpolation{Interpolation::Linear};
        std::vector<float> times;
        std::vector<math::Vec4> values;
    };

    // One animation of a skeleton (a glTF animation's channels that target its joints).
    // name:     the authored name.
    // duration: seconds from 0 to the last key.
    // channels: the animated joint parts; joints without a channel keep their pose value.
    struct AnimationClip
    {
        std::string name;
        float duration{0.0f};
        std::vector<AnimationChannel> channels;
    };

    // Samples a clip into a pose. Times outside the keys hold the first or last key.
    // clip:    the clip.
    // seconds: time in the clip.
    // pose:    pose of the clip's skeleton; only the animated parts are written, so start from
    //          resetToRest() for a full pose.
    void sampleClip(const AnimationClip& clip, float seconds, Pose& pose);

    // Samples one joint's rotation from a clip.
    // clip:    the clip.
    // joint:   index into the skeleton.
    // seconds: time in the clip.
    // Returns: the joint's local rotation, or nothing when the clip doesn't animate it.
    [[nodiscard]] std::optional<math::Quat> sampleJointRotation(const AnimationClip& clip, uint32_t joint,
        float seconds);

    // Blends two poses of the same skeleton: translation and scale linearly, rotation
    // spherically along the shortest arc.
    // from:   the pose at weight 0.
    // to:     the pose at weight 1.
    // weight: blend factor, clamped to 0..1.
    // result: receives the blend; may be `from` or `to`.
    void blendPoses(const Pose& from, const Pose& to, float weight, Pose& result);

    // Finds a clip by name, case-insensitively.
    // clips:   the clips to search.
    // name:    the name.
    // Returns: the clip's index, or nothing.
    [[nodiscard]] std::optional<uint32_t> findClip(std::span<const AnimationClip> clips, std::string_view name);
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_ANIMATION_CLIP_HPP
