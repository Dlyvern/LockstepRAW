#ifndef LOCKSTEP_GAME_CHARACTERS_CHARACTER_MOTION_HPP
#define LOCKSTEP_GAME_CHARACTERS_CHARACTER_MOTION_HPP

#include "Characters/CharacterModel.hpp"
#include "Characters/LocomotionSignals.hpp"

#include <Lockstep/Anim/DrivenPose.hpp>
#include <Lockstep/Anim/LocomotionPose.hpp>
#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <array>
#include <span>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    // One character's pose corrections and the ground last probed under its feet. Presentation
    // only, never replicated.
    // pose:     the Engine state (smoothed ground, pelvis, locked feet).
    // ground:   what each foot probe found.
    // probedAt: where each probe was cast (a probe is cast again once its foot moved).
    // hasProbes: ground and probedAt are valid.
    struct CharacterMotion
    {
        lockstep::anim::LocomotionPoseState pose{};
        std::array<lockstep::anim::FootGround, lockstep::anim::k_footProbeCount> ground{};
        std::array<math::Vec3, lockstep::anim::k_footProbeCount> probedAt{};
        bool hasProbes{false};
    };

    // What poseCharacter() needs this frame.
    // placement:    the world transform the model is drawn with.
    // offsets:      rotations added on top of the animated pose (hit reactions, kick).
    // signals:      the character's locomotion signals, or null (a standing placeholder: only
    //               foot IK).
    // yaw:          its facing (with signals).
    // isGrounded:   it stands on the ground (foot IK fades out otherwise).
    // physics:      the world the ground is probed in, or null (no foot IK).
    // deltaSeconds: presentation time since the last frame.
    struct CharacterPoseFrame
    {
        math::Transform placement{};
        std::span<const lockstep::anim::JointRotationOffset> offsets{};
        const LocomotionSignals* signals{nullptr};
        float yaw{0.0f};
        bool isGrounded{true};
        const lockstep::physics::PhysicsWorld* physics{nullptr};
        float deltaSeconds{0.0f};
    };

    // Animates a character and fits the pose to its movement and the ground: orientation and
    // stride warping, turn steps, lean and foot IK (see LocomotionPose.hpp), as the tuning
    // allows. Replaces SkinnedInstance::update() for characters with a locomotion rig; without
    // one it is a plain update.
    // instance:  the character's animated instance.
    // character: its model.
    // motion:    its correction state.
    // tuning:    the locomotion tuning.
    // frame:     this frame's placement, offsets, signals and ground.
    void poseCharacter(lockstep::anim::SkinnedInstance& instance, const CharacterModel& character,
        CharacterMotion& motion, const LocomotionTuning& tuning, const CharacterPoseFrame& frame);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_CHARACTER_MOTION_HPP
