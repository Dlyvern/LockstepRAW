#ifndef LOCKSTEP_GAME_WEATHER_LENS_DROPS_HPP
#define LOCKSTEP_GAME_WEATHER_LENS_DROPS_HPP

#include "Audio/CombatAudio.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/LensRain.hpp>

#include <span>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // How rain gathers on the camera lens, editable from the Weather dev section.
    // enabled:      put drops on the lens at all.
    // rate:         drops landing per second at full rain, looking level.
    // lookUpBoost:  extra rate multiplier when looking straight up into the rain.
    // windBoost:    extra rate multiplier when facing a 10 m/s wind.
    // insideDrying: drying speed multiplier once out of the rain (drops clear faster indoors).
    // exposureSeconds: seconds the lens takes to count as in or out of the rain after moving.
    struct LensDropTuning
    {
        bool enabled{true};
        float rate{12.0f};
        float lookUpBoost{3.5f};
        float windBoost{0.6f};
        float insideDrying{3.0f};
        float exposureSeconds{0.4f};
    };

    // The rain on this machine's camera lens (an Engine LensRain driven by the weather).
    // drops:    the simulated drops.
    // exposure: 0 sheltered (indoors or under a roof) to 1 in the open rain, eased.
    // isOpen:   the last shelter test found open sky.
    struct LensDrops
    {
        lockstep::render::LensRain drops{};
        float exposure{0.0f};
        bool isOpen{false};
    };

    // What the lens rain reads each frame.
    // rain:      how hard it rains, 0 to 1.
    // eye:       camera position.
    // forward:   unit view direction.
    // wind:      the wind velocity in m/s.
    // physics:   the physics world (roof test), or null.
    // interiors: the level's interior boxes (always sheltered).
    // seconds:   Player time since the previous frame (the lens is the player's).
    // aspectRatio: screen width over height.
    struct LensDropFrame
    {
        float rain{0.0f};
        lockstep::math::Vec3 eye{0.0f, 0.0f, 0.0f};
        lockstep::math::Vec3 forward{0.0f, 0.0f, -1.0f};
        lockstep::math::Vec3 wind{0.0f, 0.0f, 0.0f};
        const lockstep::physics::PhysicsWorld* physics{nullptr};
        std::span<const InteriorBox> interiors{};
        float seconds{0.0f};
        float aspectRatio{16.0f / 9.0f};
    };

    // Lands, runs and dries the lens drops and submits them to the renderer: drops land only out
    // in the rain (not inside interior boxes or under a roof), more when looking up or into the
    // wind; once sheltered they stop landing and dry faster.
    // lens:     the lens state.
    // tuning:   rates.
    // frame:    this frame's view and weather.
    // renderer: the renderer to submit the drops to.
    void updateLensDrops(LensDrops& lens, const LensDropTuning& tuning, const LensDropFrame& frame,
        lockstep::render::Renderer& renderer);
} //namespace game

#endif //LOCKSTEP_GAME_WEATHER_LENS_DROPS_HPP
