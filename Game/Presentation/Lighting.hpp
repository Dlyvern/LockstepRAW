#ifndef LOCKSTEP_GAME_PRESENTATION_LIGHTING_HPP
#define LOCKSTEP_GAME_PRESENTATION_LIGHTING_HPP

#include "Presentation/MaterialShowcase.hpp"

#include <Lockstep/Platform/Input.hpp>

#include <optional>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // Presentation-only lighting state of this machine. Never replicated.
    // seconds:        presentation time.
    // showcase:       the PBR test objects, created on first use.
    // showcaseFailed: creating the showcase failed once; it is not retried.
    struct LightingState
    {
        float seconds{0.0f};
        std::optional<MaterialShowcase> showcase;
        bool showcaseFailed{false};
    };

    // Sets the arena's look on the renderer once: a clear afternoon sun and sky lighting the
    // arena, image-based lighting, no fog, filmic tonemapping. The dev tools can change it
    // afterwards.
    // renderer: the renderer.
    void applyArenaLook(lockstep::render::Renderer& renderer);

    // Advances the lighting clock.
    // state:        the lighting state.
    // input:        this frame's input.
    // frameSeconds: wall time since the previous frame.
    void updateLighting(LightingState& state, const lockstep::platform::Input& input, float frameSeconds);

    // Submits the arena's lights (the sun comes from the renderer's sky; this adds a few warm lamps)
    // and draws the PBR material showcase along the north wall, creating it on the first call.
    // renderer: the renderer.
    // state:    the lighting state.
    void submitArenaLights(lockstep::render::Renderer& renderer, LightingState& state);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_LIGHTING_HPP
