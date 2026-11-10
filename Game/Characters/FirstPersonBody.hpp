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
    //               its own).
    // LegsOnly:     thighs and shins.
    // Hidden:       nothing (the old behaviour).
    enum class FirstPersonBodyMode : uint8_t
    {
        TorsoAndLegs = 0,
        LegsOnly,
        Hidden
    };

    // Feel of the first-person body, editable from the dev panel.
    // mode:         which parts are drawn.
    // backOffset:   how far behind the eye the body is drawn looking straight ahead, metres, so
    //               the camera never sits inside the chest.
    // lookDownBack: extra distance back at a full look down, metres (the view then shows the
    //               legs in front instead of the vest from inside).
    // crouchBack:   extra distance back while crouched or sliding, metres.
    struct FirstPersonBodyTuning
    {
        FirstPersonBodyMode mode{FirstPersonBodyMode::TorsoAndLegs};
        float backOffset{0.16f};
        float lookDownBack{0.14f};
        float crouchBack{0.08f};
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
    // hasViewModel:       the viewmodel draws its own arms and weapon (the body's are hidden).
    struct FirstPersonBodyView
    {
        float lookYaw{0.0f};
        float lookPitch{0.0f};
        bool hasViewModel{false};
    };

    // masks:   a model's first-person masks.
    // tuning:  the first-person body settings.
    // view:    this frame's look.
    // Returns: the parts the first-person camera draws (0 draws nothing).
    [[nodiscard]] lockstep::anim::PartMask firstPersonBodyParts(const FirstPersonBodyMasks& masks,
        const FirstPersonBodyTuning& tuning, const FirstPersonBodyView& view);

    // Moves the body's placement back behind the eye for the first-person camera.
    // placement:  where the body is drawn for everyone else (and in mirrors).
    // tuning:     the first-person body settings.
    // view:       this frame's look.
    // crouch:     0 standing to 1 crouched or sliding.
    // Returns:    the placement for the first-person camera.
    [[nodiscard]] math::Transform firstPersonBodyPlacement(const math::Transform& placement,
        const FirstPersonBodyTuning& tuning, const FirstPersonBodyView& view, float crouch);

#if LOCKSTEP_DEV_TOOLS
    // Draws the first-person body controls of the Game dev panel.
    // tuning: the settings, edited.
    void firstPersonBodyDevSection(FirstPersonBodyTuning& tuning);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_FIRST_PERSON_BODY_HPP
