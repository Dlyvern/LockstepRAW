#ifndef LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_SIGHTS_HPP
#define LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_SIGHTS_HPP

#include "Characters/ViewModel.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>

#include <optional>

namespace game
{
    namespace math = lockstep::math;

    // Points on the drawn first-person weapon in world space, where they appear on screen. The
    // viewmodel is drawn with its own field of view, so its points are moved sideways and up to
    // where the world camera shows that pixel at the same depth; world-space effects placed there
    // (flashlight, muzzle flash, casings) line up with the drawn weapon at any zoom.
    // muzzle:     the muzzle; its -z points along the barrel and +y towards the weapon's top.
    // forward:    unit direction of the barrel.
    // rearSight:  the rear aperture.
    // frontSight: the front post.
    // rail:       the mount of the weapon light, under the barrel behind the muzzle.
    struct ViewModelSockets
    {
        math::Transform muzzle{};
        math::Vec3 forward{0.0f, 0.0f, -1.0f};
        math::Vec3 rearSight{0.0f, 0.0f, 0.0f};
        math::Vec3 frontSight{0.0f, 0.0f, 0.0f};
        math::Vec3 rail{0.0f, 0.0f, 0.0f};
    };

    // Returns: the vertical field of view the viewmodel is drawn with, radians.
    [[nodiscard]] float viewModelFov();

    // The weapon's points as last drawn (drawViewModel() of this frame).
    // state:    the viewmodel state.
    // worldFov: the world camera's vertical field of view this frame, radians.
    // Returns:  the points, or nothing while the viewmodel is hidden.
    [[nodiscard]] std::optional<ViewModelSockets> viewModelSockets(const ViewModelState& state, float worldFov);

    // Where a shot leaves towards: from the hip the look direction (where the crosshair is
    // drawn), aiming down sights the ray from the eye through the front post as drawn (sway,
    // breathing, weapon kick, the camera's head motion), or the axis of the mounted optic
    // (ViewModelOpticAim::axis, where its reticle points), blended by the aim-in.
    // state:    the viewmodel state.
    // worldFov: the world camera's vertical field of view this frame, radians.
    // Returns:  unit world direction, or nothing while the viewmodel is hidden.
    [[nodiscard]] std::optional<math::Vec3> viewModelShotDirection(const ViewModelState& state, float worldFov);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_VIEW_MODEL_SIGHTS_HPP
