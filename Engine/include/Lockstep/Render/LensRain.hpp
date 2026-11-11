#ifndef LOCKSTEP_RENDER_LENS_RAIN_HPP
#define LOCKSTEP_RENDER_LENS_RAIN_HPP

#include "Lockstep/Core/Math.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::render
{
    class Renderer;

    // Most drops a LensRain keeps at once.
    inline constexpr uint32_t k_maxLensDroplets{320};

    // One drop on the lens as the renderer draws it.
    // position: centre in screen uv (0..1, y down).
    // radius:   radius in screen heights.
    // stretch:  vertical elongation of a running drop, 1 for round.
    // opacity:  0 (gone) to 1.
    struct LensDroplet
    {
        math::Vec2 position{0.5f, 0.5f};
        float radius{0.01f};
        float stretch{1.0f};
        float opacity{1.0f};
    };

    // What drives the drops for one update.
    // seconds:     time to simulate (the caller picks the clock; negative counts as 0).
    // landRate:    drops landing per second (0 when the lens is out of the rain).
    // gravity:     how strongly drops run down the screen, 0 (lens facing up) to 1 (level view).
    // evaporation: how fast drops dry, 1 natural (a small drop lasts a few seconds), higher dries
    //              faster (e.g. after going inside).
    // aspectRatio: screen width over height (drops stay round).
    // drift:       screen-space push in uv per second (e.g. from the wind), added to running drops.
    struct LensRainFrame
    {
        float seconds{0.0f};
        float landRate{0.0f};
        float gravity{1.0f};
        float evaporation{1.0f};
        float aspectRatio{16.0f / 9.0f};
        math::Vec2 drift{0.0f, 0.0f};
    };

    // Counters for the dev tools.
    // drops:    drops on the lens.
    // running:  drops currently running down.
    // coverage: rough share of the screen covered, 0 to 1.
    struct LensRainStats
    {
        uint32_t drops{0};
        uint32_t running{0};
        float coverage{0.0f};
    };

    // Rain drops on a camera lens, simulated on the CPU: drops land at random, bead up, merge when
    // they touch, run down once heavy enough (stick-slip, leaving a trail of small beads), shrink as
    // they dry and fade out. Presentation only; Game owns one, updates it every frame and submits
    // it (see LensRainSettings for how it is drawn).
    class LensRain
    {
    public:
        // Advances the drops.
        // frame: rates and time for this update.
        void update(const LensRainFrame& frame);

        // Starts a wipe: a band sweeping down the lens over `seconds` that removes the drops it
        // passes.
        // seconds: duration of the sweep (at least one frame).
        void wipe(float seconds);

        // Removes every drop at once (a level switch).
        void clear();

        // Hands the drops to the renderer for its next render().
        // renderer: the renderer.
        void submit(Renderer& renderer) const;

        // Returns: the drops, for drawing.
        [[nodiscard]] std::span<const LensDroplet> droplets() const;

        // Returns: counters for the dev tools.
        [[nodiscard]] LensRainStats stats() const;

    private:
        // One drop's simulation state.
        // mass:      water volume, which sets the radius.
        // speed:     downward speed in screen heights per second while running.
        // stuck:     seconds it stays put before it may run again (stick-slip).
        // trail:     screen heights run since it left its last trail bead.
        struct Drop
        {
            math::Vec2 position{0.5f, 0.5f};
            float mass{0.0f};
            float speed{0.0f};
            float stuck{0.0f};
            float trail{0.0f};
            float opacity{0.0f};
        };

        float random();
        void land(const LensRainFrame& frame);
        void move(Drop& drop, const LensRainFrame& frame);
        void merge(float aspectRatio);
        void sweep(float seconds);

        std::vector<Drop> m_drops;
        std::vector<LensDroplet> m_view;
        float m_landCarry{0.0f};
        float m_wipeSeconds{0.0f};
        float m_wipeDuration{0.0f};
        uint32_t m_random{0x9E3779B9u};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_LENS_RAIN_HPP
