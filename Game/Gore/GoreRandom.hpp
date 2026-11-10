#ifndef LOCKSTEP_GAME_GORE_GORE_RANDOM_HPP
#define LOCKSTEP_GAME_GORE_GORE_RANDOM_HPP

#include <cstdint>

namespace game
{
    // Small xorshift generator for the look of gore (droplet spread, splatter spin). Seeded from
    // a GoreEvent so both machines draw an event the same way. Not for game rules.
    struct GoreRandom
    {
        uint32_t state{0x9E3779B9u};

        // Returns: the next value in [0, 1).
        float next()
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return static_cast<float>(state >> 8) * (1.0f / 16777216.0f);
        }

        // low, high: the range.
        // Returns:   the next value in [low, high).
        float range(float low, float high)
        {
            return low + (high - low) * next();
        }
    };

    // seed:    any number (0 included).
    // Returns: a generator started from it.
    [[nodiscard]] inline GoreRandom goreRandom(uint32_t seed)
    {
        GoreRandom random{.state = (seed ^ 0xA511E9B3u) * 2654435761u + 1u};
        random.next();
        return random;
    }
} //namespace game

#endif //LOCKSTEP_GAME_GORE_GORE_RANDOM_HPP
