#ifndef LOCKSTEP_GAME_PRESENTATION_LIGHTING_HPP
#define LOCKSTEP_GAME_PRESENTATION_LIGHTING_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Platform/Input.hpp>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    namespace math = lockstep::math;

    // Presentation-only lighting state of this machine. Never replicated: the local flashlight
    // switch only changes what this player sees (the other player's flashlight is always drawn on).
    // isFlashlightOn: the local player's flashlight.
    // seconds:        presentation time, drives flickering fixtures.
    struct LightingState
    {
        bool isFlashlightOn{true};
        float seconds{0.0f};
    };

    // Sets the arena's look on the renderer once: dark ambient, black fog, filmic tonemapping.
    // The dev tools can change it afterwards.
    // renderer: the renderer.
    void applyArenaLook(lockstep::render::Renderer& renderer);

    // Advances the lighting clock and toggles the flashlight on F.
    // state:        the lighting state.
    // input:        this frame's input.
    // frameSeconds: wall time since the previous frame.
    void updateLighting(LightingState& state, const lockstep::platform::Input& input, float frameSeconds);

    // Submits the arena's fixed lights: a faint skylight and a few failing ceiling fixtures.
    // renderer: the renderer.
    // state:    the lighting state (for flicker).
    void submitArenaLights(lockstep::render::Renderer& renderer, const LightingState& state);

    // Submits the local player's flashlight, held at the hip so its shadows show. It is the one
    // light that casts the shadow map. Does nothing while the flashlight is off.
    // renderer:  the renderer.
    // state:     the lighting state.
    // eye:       the camera position.
    // direction: unit look direction.
    void submitLocalFlashlight(lockstep::render::Renderer& renderer, const LightingState& state, const math::Vec3& eye,
        const math::Vec3& direction);

    // Submits another player's flashlight, pointing where their pawn faces.
    // renderer: the renderer.
    // pawn:     the pawn's (interpolated) transform.
    void submitRemoteFlashlight(lockstep::render::Renderer& renderer, const math::Transform& pawn);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_LIGHTING_HPP
