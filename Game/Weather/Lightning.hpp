#ifndef LOCKSTEP_GAME_WEATHER_LIGHTNING_HPP
#define LOCKSTEP_GAME_WEATHER_LIGHTNING_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/RenderSettings.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace game
{
    // How often and how hard lightning strikes while it rains, editable from the Weather dev section.
    // enabled:      strike at all.
    // minRain:      rain amount below which there is no lightning.
    // minSeconds, maxSeconds: World time between strikes at full rain (scaled up in lighter rain).
    // minDistance, maxDistance: metres from the camera a strike lands at.
    // brightness:   multiplier on the flash.
    // bolts:        draw the bolt in the sky.
    struct LightningTuning
    {
        bool enabled{true};
        float minRain{0.3f};
        float minSeconds{9.0f};
        float maxSeconds{35.0f};
        float minDistance{900.0f};
        float maxDistance{9000.0f};
        float brightness{1.0f};
        bool bolts{true};
    };

    // Thunder of a strike, due now.
    // direction: unit direction from the camera towards the strike.
    // distance:  metres to the strike (pitch and character of the roll).
    // loudness:  0 to about 1.2 (near strikes crack).
    struct ThunderCue
    {
        lockstep::math::Vec3 direction{0.0f, 0.0f, -1.0f};
        float distance{3000.0f};
        float loudness{1.0f};
    };

    // Lightning on this machine: presentation only, never replicated, animated with World time (so
    // bullet-time slows a flash). A strike is a few return strokes within half a second; its thunder
    // arrives after the distance over the speed of sound.
    // untilStrike: World time to the next strike.
    // age:         World time since the current strike began (negative: none).
    // strokeTimes: when each return stroke of the current strike fires, from its start.
    // strokePeaks: each stroke's brightness.
    // strokes:     strokes in the current strike.
    // direction:   unit direction from the camera towards the current strike's origin in the sky.
    // distance:    metres to it.
    // boltSeed:    shape of its bolt.
    // cloudBase:   metres above the camera of the cloud base at the last update (strike origin).
    // flash:       brightness this frame (LightningSettings::flash).
    // pending:     thunder still on its way: World seconds until due, and the cue.
    // due:         thunder that arrived this frame (replaced every update).
    // random:      presentation-only random state.
    struct Lightning
    {
        float untilStrike{6.0f};
        float age{-1.0f};
        std::array<float, 4> strokeTimes{};
        std::array<float, 4> strokePeaks{};
        uint32_t strokes{0};
        lockstep::math::Vec3 direction{0.0f, 1.0f, 0.0f};
        float distance{3000.0f};
        float boltSeed{0.0f};
        float cloudBase{1000.0f};
        float flash{0.0f};
        std::vector<std::pair<float, ThunderCue>> pending{};
        std::vector<ThunderCue> due{};
        uint32_t random{0x1B56C4E9u};
    };

    // Advances lightning: schedules strikes while it rains, animates the flash and releases the
    // thunder that has arrived (into `lightning.due`).
    // lightning:    the lightning state.
    // tuning:       how it behaves.
    // rain:         how hard it rains, 0 to 1.
    // cloudBase:    metres above the camera of the cloud base (where strikes come from).
    // worldSeconds: World time since the previous frame.
    void updateLightning(Lightning& lightning, const LightningTuning& tuning, float rain, float cloudBase,
        float worldSeconds);

    // Starts a strike now (dev tools), whatever the weather.
    // lightning: the lightning state.
    // tuning:    distances.
    void strikeLightning(Lightning& lightning, const LightningTuning& tuning);

    // Writes the flash into the render settings (LightningSettings).
    // lightning: the lightning state.
    // tuning:    brightness and bolts.
    // settings:  the render settings.
    void applyLightning(const Lightning& lightning, const LightningTuning& tuning,
        lockstep::render::RenderSettings& settings);
} //namespace game

#endif //LOCKSTEP_GAME_WEATHER_LIGHTNING_HPP
