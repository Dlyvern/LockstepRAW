#ifndef LOCKSTEP_ANIM_MOTION_DATABASE_HPP
#define LOCKSTEP_ANIM_MOTION_DATABASE_HPP

#include "Lockstep/Anim/LocomotionPose.hpp"
#include "Lockstep/Anim/SkinnedModel.hpp"
#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::anim
{
    // Motion matching data: every frame of a set of clips with root motion (the hips travel and
    // turn), described by a feature vector (pose and future trajectory in character space) so the
    // frame that best continues the current pose towards a wanted trajectory can be searched for
    // (see MotionMatching.hpp). Character space: origin on the ground under the root, +Z the
    // root's facing, +X towards its left, Y up (model space conventions).

    // Frames per second clips are sampled at.
    inline constexpr float k_motionSampleRate{30.0f};

    // Future trajectory points per feature vector, at 1/3, 2/3 and 1 second.
    inline constexpr std::size_t k_motionTrajectoryPoints{3};

    // Frames between the frame and each trajectory point.
    inline constexpr std::array<uint32_t, k_motionTrajectoryPoints> k_motionTrajectoryFrames{10, 20, 30};

    // Floats per feature vector: both feet's position and velocity (12), the hips velocity (3), and
    // per trajectory point its ground position (x, z) and facing direction (x, z) (12).
    inline constexpr std::size_t k_motionFeatureCount{27};

    // Where the hips velocity part of a feature vector starts.
    inline constexpr std::size_t k_motionHipVelocityOffset{12};

    // Where the trajectory part of a feature vector starts.
    inline constexpr std::size_t k_motionTrajectoryOffset{15};

    // One clip of a database.
    // clip:      index into the model's clips (with root motion).
    // isLooping: the clip cycles (walks, runs, idles); otherwise it plays once (starts, stops,
    //            turns) and its last frames are not jumped to.
    struct MotionClipSpec
    {
        uint32_t clip{0};
        bool isLooping{true};
    };

    // How much each part of the feature vector counts in a search (after normalisation).
    struct MotionFeatureWeights
    {
        float footPosition{0.75f};
        float footVelocity{0.6f};
        float hipVelocity{1.5f};
        float trajectoryPosition{1.5f};
        float trajectoryDirection{1.5f};
    };

    // A wanted future path in character space: ground positions and facing directions at the
    // k_motionTrajectoryPoints times, and the character's current velocity (it replaces the hips
    // velocity of the playing frame in a search, so a character that really moves leaves an idle).
    struct MotionTrajectory
    {
        std::array<math::Vec3, k_motionTrajectoryPoints> positions{};
        std::array<math::Vec3, k_motionTrajectoryPoints> directions{};
        math::Vec3 velocity{0.0f, 0.0f, 0.0f};
    };

    // The root of a clip at one sample: ground position (model space, y = 0) and facing yaw
    // (radians about +Y, positive turning +Z towards +X).
    struct MotionRoot
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        float yaw{0.0f};
    };

    // One clip of a built database.
    // sourceClip: the clip with root motion it was built from.
    // playClip:   the in-place copy to play (root motion taken out), appended to the model.
    // isLooping:  see MotionClipSpec.
    // duration:   seconds.
    // firstFrame: index of its first frame in the database.
    // frameCount: its frames (one per 1 / k_motionSampleRate seconds, both ends included).
    // roots:      the smoothed root per frame.
    struct MotionDatabaseClip
    {
        uint32_t sourceClip{0};
        uint32_t playClip{0};
        bool isLooping{true};
        float duration{0.0f};
        uint32_t firstFrame{0};
        uint32_t frameCount{0};
        std::vector<MotionRoot> roots;
    };

    // The frames of a set of clips and their normalised feature vectors.
    class MotionDatabase
    {
    public:
        // Builds a database: samples each clip, measures its root (hips ground position and facing,
        // smoothed), appends to the model an in-place copy of each clip (the hips keep only their
        // motion relative to the root) and computes the normalised features of every frame.
        // model:   the model the clips belong to; the in-place copies are appended to its clips.
        // rig:     the model's locomotion rig (hips and feet).
        // clips:   the clips, at least one.
        // weights: how the feature parts count.
        // Returns: the database, or InvalidArgument for no clips, an unknown clip or a clip shorter
        //          than two frames.
        [[nodiscard]] static Result<MotionDatabase> build(SkinnedModel& model, const LocomotionRig& rig,
            std::span<const MotionClipSpec> clips, const MotionFeatureWeights& weights = {});

        // Returns: the clips.
        [[nodiscard]] std::span<const MotionDatabaseClip> clips() const noexcept;

        // Returns: how many frames the database has.
        [[nodiscard]] uint32_t frameCount() const noexcept;

        // frame:   a database frame.
        // Returns: its normalised feature vector (k_motionFeatureCount floats).
        [[nodiscard]] std::span<const float> features(uint32_t frame) const;

        // frame:   a database frame.
        // Returns: true when a search may jump to it (not near the end of a one-shot clip).
        [[nodiscard]] bool isSearchable(uint32_t frame) const;

        // frame:   a database frame.
        // Returns: the clip slot (index into clips()) it belongs to.
        [[nodiscard]] uint32_t clipOf(uint32_t frame) const;

        // clip:    a clip slot.
        // seconds: time in the clip.
        // Returns: the nearest frame.
        [[nodiscard]] uint32_t frameAt(uint32_t clip, float seconds) const;

        // frame:   a database frame.
        // Returns: the root's velocity there in character space, m/s.
        [[nodiscard]] math::Vec3 rootVelocity(uint32_t frame) const;

        // Puts a wanted trajectory into the normalised feature space: its velocity into the hips
        // velocity part, its points into the trajectory part.
        // trajectory: the trajectory, character space.
        // out:        a feature vector (k_motionFeatureCount floats); the foot parts are left as
        //             they are.
        void normalizeTrajectory(const MotionTrajectory& trajectory, std::span<float> out) const;

    private:
        std::vector<MotionDatabaseClip> m_clips;
        std::vector<float> m_features;
        std::vector<uint32_t> m_frameClips;
        std::vector<uint8_t> m_searchable;
        std::array<float, k_motionFeatureCount> m_means{};
        std::array<float, k_motionFeatureCount> m_scales{};
    };
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_MOTION_DATABASE_HPP
