#ifndef LOCKSTEP_GAME_INTRO_WORDMARK_HPP
#define LOCKSTEP_GAME_INTRO_WORDMARK_HPP

#include <Lockstep/Core/Math.hpp>

#include <cstdint>
#include <vector>

namespace game
{
    namespace math = lockstep::math;

    // How the "VELA" wordmark the intro dust gathers into is sampled. There is no official Vela
    // logo file, so the wordmark is a clean geometric sans built from straight strokes.
    // height:    letter height in metres.
    // tracking:  gap between letters in metres.
    // thickness: stroke width in metres; points scatter across it.
    // depth:     points scatter this far in front of and behind the wordmark plane, in metres.
    // points:    how many points to place, spread over the strokes by length.
    // seed:      seed of the scatter.
    struct WordmarkSampling
    {
        float height{0.62f};
        float tracking{0.2f};
        float thickness{0.055f};
        float depth{0.03f};
        uint32_t points{1800};
        uint32_t seed{11};
    };

    // Samples points on the "VELA" wordmark, centred on the origin in the XY plane (+Y up, read
    // from +Z).
    // sampling: size, stroke width and point count.
    // Returns:  the points, `sampling.points` of them.
    [[nodiscard]] std::vector<math::Vec3> sampleVelaWordmark(const WordmarkSampling& sampling);
} //namespace game

#endif //LOCKSTEP_GAME_INTRO_WORDMARK_HPP
