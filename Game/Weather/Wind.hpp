#ifndef LOCKSTEP_GAME_WEATHER_WIND_HPP
#define LOCKSTEP_GAME_WEATHER_WIND_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/RenderSettings.hpp>

#include <cstdint>

namespace game
{
    // How the level's wind behaves, editable from the Weather dev section.
    // enabled:     blow at all (off: still air, foliage and litter rest).
    // clearSpeed:  mean wind speed in m/s in clear weather.
    // stormSpeed:  mean wind speed in m/s in full rain.
    // gustiness:   how much gusts add and take away, 0 (steady) to 1 (strong gusts).
    // heading:     compass direction the wind blows towards, in degrees (0: -Z, 90: +X).
    // wander:      degrees the heading drifts either way over time.
    // cloudSpeed:  cloud drift speed as a multiple of the ground wind (wind is faster aloft).
    struct WindTuning
    {
        bool enabled{true};
        float clearSpeed{2.5f};
        float stormSpeed{9.0f};
        float gustiness{0.6f};
        float heading{70.0f};
        float wander{25.0f};
        float cloudSpeed{4.0f};
    };

    // The wind on this machine: presentation only, never replicated, animated with World time.
    // speed:      current speed in m/s, gusts included.
    // direction:  unit horizontal direction it blows towards (x, z).
    // gust:       current gust level, 0 to 1, easing towards gustTarget.
    // gustTarget: the gust level being approached.
    // gustSeconds: World time until a new gust target is picked.
    // swaySeconds: foliage animation clock (WindSettings::time).
    // headingSeconds: clock of the heading's slow wander.
    // cloudOffset: how far the clouds have drifted, in metres (CloudSettings::offset).
    // random:     presentation-only random state.
    struct Wind
    {
        float speed{0.0f};
        lockstep::math::Vec2 direction{1.0f, 0.0f};
        float gust{0.5f};
        float gustTarget{0.5f};
        float gustSeconds{0.0f};
        double swaySeconds{0.0};
        double headingSeconds{0.0};
        lockstep::math::Vec2 cloudOffset{0.0f, 0.0f};
        uint32_t random{0x68E31DA4u};
    };

    // Advances the wind: gusts, the heading's wander and the cloud drift.
    // wind:         the wind state.
    // tuning:       how it behaves.
    // rain:         how hard it rains, 0 to 1 (blends calm towards storm speed).
    // worldSeconds: World time since the previous frame.
    void updateWind(Wind& wind, const WindTuning& tuning, float rain, float worldSeconds);

    // wind:    the wind state.
    // Returns: the wind's velocity in m/s (horizontal).
    [[nodiscard]] lockstep::math::Vec3 windVelocity(const Wind& wind);

    // Writes the wind into the render settings: foliage sway and the clouds' drift.
    // wind:     the wind state.
    // settings: the render settings.
    void applyWind(const Wind& wind, lockstep::render::RenderSettings& settings);
} //namespace game

#endif //LOCKSTEP_GAME_WEATHER_WIND_HPP
