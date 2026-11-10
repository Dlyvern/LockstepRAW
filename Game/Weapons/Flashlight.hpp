#ifndef LOCKSTEP_GAME_WEAPONS_FLASHLIGHT_HPP
#define LOCKSTEP_GAME_WEAPONS_FLASHLIGHT_HPP

#include "Player/PlayerStatus.hpp"

#include <Lockstep/Core/Math.hpp>

#include <array>
#include <optional>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    namespace math = lockstep::math;

    // Look of the weapon flashlights, editable from the dev panel.
    // color:          linear colour (a slightly warm LED).
    // intensity:      brightness of the beam.
    // range:          how far it reaches, metres.
    // innerAngle, outerAngle: half-angles of the hot spot and of the spill, radians.
    // volumetric:     fog scattering of the local player's own beam (low: a beam from the eye
    //                 would wash out the view).
    // partnerVolumetric: fog scattering of the partner's beam, seen from the side.
    // castsShadows:   the beams cast shadows.
    // forwardOffset:  metres in front of the mount the light sits (clear of the weapon's own mesh).
    // flickerSeconds: how long the light stutters when switched on.
    // partnerBack:    metres behind the partner's muzzle its light sits.
    struct FlashlightTuning
    {
        math::Vec3 color{1.0f, 0.93f, 0.82f};
        float intensity{70.0f};
        float range{30.0f};
        float innerAngle{0.14f};
        float outerAngle{0.42f};
        float volumetric{0.25f};
        float partnerVolumetric{0.7f};
        bool castsShadows{true};
        float forwardOffset{0.06f};
        float flickerSeconds{0.32f};
        float partnerBack{0.07f};
    };

    // The flashlights as this machine draws them. Presentation only; the on/off state comes from
    // the local command and the partner's replicated status.
    // tuning:    the look.
    // onSeconds: per player, seconds since its light went on (drives the switch-on flicker).
    // wasOn:     per player, the light was on last frame.
    struct FlashlightState
    {
        FlashlightTuning tuning{};
        std::array<float, k_playerCount> onSeconds{};
        std::array<bool, k_playerCount> wasOn{};
    };

    // Where one player's light shines from this frame.
    // position:  the light's mount in world space.
    // direction: unit direction of the beam.
    struct FlashlightMount
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
    };

    // What the flashlights read this frame.
    // isOn:    per player, its light is on (local command for this machine's player, status for
    //          the partner).
    // mounts:  per player, where its light is, if it is drawn this frame.
    // localIndex: this machine's player.
    // seconds: Player time since the previous frame.
    struct FlashlightFrame
    {
        std::array<bool, k_playerCount> isOn{};
        std::array<std::optional<FlashlightMount>, k_playerCount> mounts{};
        std::size_t localIndex{0};
        float seconds{0.0f};
    };

    // Submits the shining flashlights as shadowed spot lights (call before the level's lights, so
    // the 16-light budget keeps them), with a short stutter after each switch-on.
    // state:    the flashlight state.
    // renderer: the renderer.
    // frame:    on/off states and mounts.
    void presentFlashlights(FlashlightState& state, lockstep::render::Renderer& renderer, const FlashlightFrame& frame);

#if LOCKSTEP_DEV_TOOLS
    // Draws the flashlight controls of the Game dev panel.
    // state: the flashlight state.
    void flashlightDevSection(FlashlightState& state);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_FLASHLIGHT_HPP
