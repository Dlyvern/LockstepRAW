#ifndef LOCKSTEP_GAME_CHARACTERS_FIRST_PERSON_BODY_HPP
#define LOCKSTEP_GAME_CHARACTERS_FIRST_PERSON_BODY_HPP

#include "Characters/GearLooks.hpp"

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Render/Views.hpp>

#include <optional>

namespace game
{
    namespace math = lockstep::math;

    // Render layer of the local player's own body as the first-person camera sees it (full-body
    // awareness: looking down shows the legs and torso moving with the animation). Mirrors don't
    // draw it; they show the whole body on k_mirrorOnlyLayer instead.
    inline constexpr lockstep::render::RenderLayers k_firstPersonBodyLayer{1u << 2};

    // Which parts of the body the first-person camera sees.
    // TorsoAndLegs: everything but the head and head gear (and the arms while the viewmodel shows
    //               its own). Looking down shows the cut neck and arms; for testing only.
    // LegsOnly:     thighs and shins (hips down), kept out of view above the hips.
    // Hidden:       nothing (the old behaviour).
    enum class FirstPersonBodyMode : uint8_t
    {
        TorsoAndLegs = 0,
        LegsOnly,
        Hidden
    };

    // Feel of the first-person body, editable from the dev panel. The legs are moved back just as
    // far as needed to keep the open top of the thighs (the cut at the hips) below the bottom
    // edge of the view, which depends on the look pitch and how far the hips are below the eye.
    // mode:         which parts are drawn.
    // minBack:      least distance the body sits behind where the pawn stands, metres.
    // hipRadius:    how far the thigh tops reach in front of the hip joint, metres.
    // margin:       extra room kept between the thigh tops and the view's edge, metres.
    // viewHalfAngle: half the vertical field of view assumed, radians (a little more than the
    //               camera's, so field of view kicks never reveal the cut).
    // kickForward:  how far the body may come forward with a kick at full extension, metres (so
    //               the kicking leg shows at the bottom of the view; never past the cut limit).
    struct FirstPersonBodyTuning
    {
        FirstPersonBodyMode mode{FirstPersonBodyMode::LegsOnly};
        float minBack{0.05f};
        float hipRadius{0.16f};
        float margin{0.08f};
        float viewHalfAngle{0.62f};
        float kickForward{0.15f};
    };

    // The parts of one character model the first-person camera may show. Needs a model cooked
    // with limbs (LockstepCooker --limbs); without them nothing can be hidden selectively.
    // withArms:    body without head and head gear.
    // withoutArms: the same without the arms (the viewmodel shows its own arms and weapon).
    // legs:        thighs and shins.
    struct FirstPersonBodyMasks
    {
        lockstep::anim::PartMask withArms{0};
        lockstep::anim::PartMask withoutArms{0};
        lockstep::anim::PartMask legs{0};
    };

    // Finds the first-person parts of a character model.
    // model:   the character model.
    // gear:    its head gear masks (findGearMasks()).
    // Returns: the masks, or nothing for a model cooked without limbs.
    [[nodiscard]] std::optional<FirstPersonBodyMasks> findFirstPersonBodyMasks(
        const lockstep::anim::SkinnedModel& model, const GearMasks& gear);

    // What the first-person camera shows of the local body this frame.
    // lookYaw, lookPitch: the camera's look, radians.
    // eye:                where the camera is, world (the pawn's eye position).
    // hasViewModel:       the viewmodel draws its own arms and weapon (the body's are hidden).
    struct FirstPersonBodyView
    {
        float lookYaw{0.0f};
        float lookPitch{0.0f};
        math::Vec3 eye{0.0f, 0.0f, 0.0f};
        bool hasViewModel{false};
    };

    // masks:   a model's first-person masks.
    // tuning:  the first-person body settings.
    // view:    this frame's look.
    // Returns: the parts the first-person camera draws (0 draws nothing).
    [[nodiscard]] lockstep::anim::PartMask firstPersonBodyParts(const FirstPersonBodyMasks& masks,
        const FirstPersonBodyTuning& tuning, const FirstPersonBodyView& view);

    // Moves the body's placement back behind the eye for the first-person camera, far enough that
    // the cut at the hips stays below the view (see FirstPersonBodyTuning).
    // placement:  where the body is drawn for everyone else (and in mirrors).
    // tuning:     the first-person body settings.
    // view:       this frame's look and eye.
    // hips:       the hip joint's world position with `placement`.
    // kick:       how far a kick's leg is out, 0 to 1 (appendKickOffsets()).
    // Returns:    the placement for the first-person camera.
    [[nodiscard]] math::Transform firstPersonBodyPlacement(const math::Transform& placement,
        const FirstPersonBodyTuning& tuning, const FirstPersonBodyView& view, const math::Vec3& hips, float kick);

#if LOCKSTEP_DEV_TOOLS
    // Draws the first-person body controls of the Game dev panel.
    // tuning: the settings, edited.
    void firstPersonBodyDevSection(FirstPersonBodyTuning& tuning);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_FIRST_PERSON_BODY_HPP
