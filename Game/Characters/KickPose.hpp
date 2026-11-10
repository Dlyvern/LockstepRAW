#ifndef LOCKSTEP_GAME_CHARACTERS_KICK_POSE_HPP
#define LOCKSTEP_GAME_CHARACTERS_KICK_POSE_HPP

#include "Npcs/HitReactions.hpp"

#include <Lockstep/Anim/DrivenPose.hpp>

#include <vector>

namespace game
{
    // The procedural front kick of a player character (there is no kick clip): the right thigh
    // swings up while the knee chambers, the shin snaps out at the strike, holds and comes back;
    // the upper body leans back against it. Rotations are added on top of the playing animation
    // (SkinnedInstance::update() offsets). Editable from the dev panel.
    // seconds:       length of the whole kick (KickSettings::seconds).
    // chamberSeconds: time to lift the knee.
    // strikeSeconds: time from the start to the full extension (KickSettings::impactSeconds).
    // holdSeconds:   time the leg stays out.
    // thighLift:     forward swing of the thigh at the strike, radians.
    // chamberKnee:   knee bend while chambered, radians.
    // strikeKnee:    knee bend at the strike, radians.
    // spineLean:     backward lean of the upper body at the strike, radians.
    // thighInward:   turn of the leg towards the body's centre line at the strike, radians (the
    //                foot lands in the middle of the first-person view).
    // forwardSign:   1 when the model faces +Z in model space (Mixamo), -1 for -Z.
    struct KickPoseTuning
    {
        float seconds{0.5f};
        float chamberSeconds{0.08f};
        float strikeSeconds{0.17f};
        float holdSeconds{0.07f};
        float thighLift{2.05f};
        float chamberKnee{1.7f};
        float strikeKnee{0.05f};
        float spineLean{0.15f};
        float thighInward{0.3f};
        float forwardSign{1.0f};
    };

    // Adds the kick's rotations for one moment of it.
    // offsets: the rotations handed to SkinnedInstance::update(), appended to.
    // rig:     the character's rig (its right leg and spine joints).
    // tuning:  the kick's look.
    // seconds: time since the kick started; outside 0..tuning.seconds nothing is added.
    // Returns: how far the leg is out, 0 (down) to 1 (struck).
    float appendKickOffsets(std::vector<lockstep::anim::JointRotationOffset>& offsets, const ReactionRig& rig,
        const KickPoseTuning& tuning, float seconds);

#if LOCKSTEP_DEV_TOOLS
    // Draws the kick pose controls of the Game dev panel.
    // tuning: the kick's look.
    void kickPoseDevSection(KickPoseTuning& tuning);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_KICK_POSE_HPP
