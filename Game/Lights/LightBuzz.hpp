#ifndef LOCKSTEP_GAME_LIGHTS_LIGHT_BUZZ_HPP
#define LOCKSTEP_GAME_LIGHTS_LIGHT_BUZZ_HPP

#include <Lockstep/Audio/AudioTypes.hpp>
#include <Lockstep/Core/Math.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lockstep::assets
{
    class FileSystem;
} //namespace lockstep::assets

namespace lockstep::audio
{
    class AudioSystem;
} //namespace lockstep::audio

namespace game
{
    namespace math = lockstep::math;

    // Levels and distances of the fluorescent sounds, editable from the dev panel.
    // buzzVolume:      the hum loop of a lit tube.
    // offBuzz:         fraction of the hum left while the tube is dark (the ballast still hums).
    // crackleVolume:   the crackle when a tube switches on or off.
    // crackleCooldown: World seconds a tube waits before it may crackle again.
    // hearDistance:    metres from the listener within which tubes buzz.
    // maxVoices:       most tubes buzzing at once (the nearest).
    struct LightBuzzTuning
    {
        float buzzVolume{0.3f};
        float offBuzz{0.15f};
        float crackleVolume{0.3f};
        float crackleCooldown{0.14f};
        float hearDistance{12.0f};
        int maxVoices{3};
    };

    // One flickering tube as the buzz sees it this frame.
    // key:      stable id of the tube (its index in Scene::lights).
    // position: where it is.
    // level:    its brightness this frame, 0-1.
    // toggled:  it switched between on and off since the previous frame.
    struct BuzzSource
    {
        uint32_t key{0};
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        float level{1.0f};
        bool toggled{false};
    };

    // A hum loop playing for one tube.
    struct BuzzVoice
    {
        uint32_t key{0};
        lockstep::audio::VoiceHandle voice{};
    };

    // The fluorescent buzz: a quiet 3D hum loop on the nearest flickering tubes and crackles when
    // they switch. Presentation only; plays on the World bus, so bullet-time slows and muffles it.
    // buzz, crackles: the loaded sounds (`sounds/ambience/fluorescent_buzz`,
    //                 `sounds/ambience/fluorescent_crackle_<n>`); missing ones stay silent.
    // isLoaded:       the sounds were looked for.
    // tuning:         levels and distances.
    // voices:         hum loops playing.
    // lastCrackle:    per source index, World seconds of its last crackle.
    // random:         presentation-only random state (crackle choice and pitch).
    struct LightBuzz
    {
        std::optional<lockstep::audio::SoundHandle> buzz{};
        std::vector<lockstep::audio::SoundHandle> crackles{};
        bool isLoaded{false};
        LightBuzzTuning tuning{};
        std::vector<BuzzVoice> voices{};
        std::vector<double> lastCrackle{};
        uint32_t random{0x68E31DA4u};
    };

    // Loads the buzz and crackle sounds, logging what is missing. Does nothing once loaded.
    // buzz:  the buzz state.
    // audio: the audio system.
    // files: the mounted content.
    void loadLightBuzz(LightBuzz& buzz, lockstep::audio::AudioSystem& audio, const lockstep::assets::FileSystem& files);

    // Keeps hum loops on the nearest audible tubes and plays crackles for tubes that switched.
    // buzz:     the buzz state.
    // audio:    the audio system.
    // sources:  the level's flickering tubes, in the same order every frame.
    // listener: the listener position.
    // seconds:  World seconds since the level's lights started animating.
    void updateLightBuzz(LightBuzz& buzz, lockstep::audio::AudioSystem& audio, std::span<const BuzzSource> sources,
        const math::Vec3& listener, double seconds);

    // Stops every hum loop and forgets crackle times (level change, animation turned off).
    // buzz:  the buzz state.
    // audio: the audio system.
    void stopLightBuzz(LightBuzz& buzz, lockstep::audio::AudioSystem& audio);

#if LOCKSTEP_DEV_TOOLS
    // Draws the buzz tuning inside the light animation dev section.
    // buzz: the buzz state.
    void lightBuzzDevSection(LightBuzz& buzz);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_LIGHTS_LIGHT_BUZZ_HPP
