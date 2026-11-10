#ifndef LOCKSTEP_GAME_LIGHTS_LIGHT_PATTERNS_HPP
#define LOCKSTEP_GAME_LIGHTS_LIGHT_PATTERNS_HPP

#include <Lockstep/Core/Math.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace game
{
    namespace math = lockstep::math;

    // How an authored level light animates, picked by a tag in its object name.
    // None:        steady.
    // Flicker:     "_Flicker", a failing fluorescent tube: mostly on, short dropouts and stutter
    //              bursts, now and then off for a second or two.
    // FlickerHard: "_FlickerHard", a dying tube: mostly off, with stutter bursts and short spells on.
    // SirenRed, SirenBlue: "_SirenRed" / "_SirenBlue", one colour of a police lightbar: double
    //              flashes, red in the first half of the cycle and blue in the second.
    // Pulse:       "_Pulse", slow breathing (alarm lights).
    enum class LightPattern : uint8_t
    {
        None = 0,
        Flicker,
        FlickerHard,
        SirenRed,
        SirenBlue,
        Pulse
    };

    // Pattern timing, editable from the dev panel. All times are World-domain seconds.
    // flickerWindow:      length of one flicker episode (steady, burst or off spell).
    // flickerSlot:        length of one on/off step inside a stutter.
    // flickerDropout:     chance per slot that a steady tube drops out for one slot.
    // flickerBurstChance: chance per episode of a stutter burst.
    // flickerOffChance:   chance per episode of a long off spell.
    // flickerOffLevel:    brightness of a tube that is off (residual glow), 0-1.
    // sirenCycle:         seconds of one red + blue lightbar cycle.
    // sirenFlash:         seconds one flash lasts.
    // sirenGap:           seconds between the two flashes of a double flash.
    // sirenRateJitter:    each car's cycle is this fraction faster or slower, so cars drift apart.
    // pulsePeriod:        seconds of one breath.
    // pulseLow:           brightness at the bottom of a breath, 0-1.
    struct LightPatternTuning
    {
        float flickerWindow{3.0f};
        float flickerSlot{0.035f};
        float flickerDropout{0.006f};
        float flickerBurstChance{0.18f};
        float flickerOffChance{0.07f};
        float flickerOffLevel{0.03f};
        float sirenCycle{0.72f};
        float sirenFlash{0.065f};
        float sirenGap{0.05f};
        float sirenRateJitter{0.08f};
        float pulsePeriod{2.6f};
        float pulseLow{0.15f};
    };

    // Reads the pattern tag in a light's object name. "_FlickerHard" wins over "_Flicker"; other
    // tags (such as "_Shadow") may appear in the same name.
    // name:    the light's object name.
    // Returns: the pattern, None when the name has no tag.
    [[nodiscard]] LightPattern lightPatternFromName(std::string_view name);

    // Short display name of a pattern, for the dev panel.
    // pattern: the pattern.
    // Returns: its name.
    [[nodiscard]] const char* lightPatternName(LightPattern pattern);

    // A stable 32-bit seed from a light's name and its index in the scene.
    // name:    the light's object name.
    // index:   its index in Scene::lights.
    // Returns: the seed.
    [[nodiscard]] uint32_t lightSeed(std::string_view name, uint32_t index);

    // Brightness of a light at a point in time. Deterministic: the same seed and time give the same
    // value on every machine and every call.
    // pattern: the pattern (None gives 1).
    // seed:    the light's seed (for sirens, the car's seed, shared by its lights).
    // seconds: World-domain seconds since the level's lights started animating.
    // tuning:  the timing.
    // Returns: brightness multiplier, 0-1.
    [[nodiscard]] float lightPatternLevel(LightPattern pattern, uint32_t seed, double seconds,
        const LightPatternTuning& tuning);

    // Whether a pattern replaces the authored colour, and with what.
    // pattern: the pattern.
    // Returns: linear RGB for sirens; nothing for other patterns (they keep the authored colour).
    [[nodiscard]] std::optional<math::Vec3> lightPatternColor(LightPattern pattern);
} //namespace game

#endif //LOCKSTEP_GAME_LIGHTS_LIGHT_PATTERNS_HPP
