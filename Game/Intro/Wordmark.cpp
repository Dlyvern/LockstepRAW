#include "Intro/Wordmark.hpp"

#include "Gore/GoreRandom.hpp"

#include <array>

namespace game
{
    namespace
    {
        struct Stroke
        {
            math::Vec2 from;
            math::Vec2 to;
        };

        struct Letter
        {
            float width{0.0f};
            std::vector<Stroke> strokes;
        };

        constexpr float k_letterWidth{0.74f};
        constexpr float k_narrowWidth{0.6f};
        constexpr float k_middleBarLength{0.86f};
        constexpr float k_crossBarHeight{0.34f};
        constexpr float k_crossBarInset{0.2f};

        std::array<Letter, 4> velaLetters()
        {
            const float w = k_letterWidth;
            const float n = k_narrowWidth;
            const float barLeft = w * 0.5f * k_crossBarHeight + k_crossBarInset * 0.15f;

            return {{
                {w, {{{0.0f, 1.0f}, {w * 0.5f, 0.0f}}, {{w * 0.5f, 0.0f}, {w, 1.0f}}}},
                {n, {{{0.0f, 0.0f}, {0.0f, 1.0f}}, {{0.0f, 1.0f}, {n, 1.0f}}, {{0.0f, 0.5f}, {n * k_middleBarLength, 0.5f}},
                        {{0.0f, 0.0f}, {n, 0.0f}}}},
                {n, {{{0.0f, 1.0f}, {0.0f, 0.0f}}, {{0.0f, 0.0f}, {n * 0.95f, 0.0f}}}},
                {w, {{{0.0f, 0.0f}, {w * 0.5f, 1.0f}}, {{w * 0.5f, 1.0f}, {w, 0.0f}},
                        {{barLeft, k_crossBarHeight}, {w - barLeft, k_crossBarHeight}}}},
            }};
        }

        float strokeLength(const Stroke& stroke, float height)
        {
            return math::length(stroke.to - stroke.from) * height;
        }
    } //namespace

    std::vector<math::Vec3> sampleVelaWordmark(const WordmarkSampling& sampling)
    {
        const std::array<Letter, 4> letters = velaLetters();
        const float height = sampling.height;
        float totalWidth = -sampling.tracking;
        float totalLength = 0.0f;

        for (const Letter& letter : letters)
        {
            totalWidth += letter.width * height + sampling.tracking;

            for (const Stroke& stroke : letter.strokes)
            {
                totalLength += strokeLength(stroke, height);
            }
        }

        std::vector<Stroke> placed;
        float left = -totalWidth * 0.5f;

        for (const Letter& letter : letters)
        {
            for (const Stroke& stroke : letter.strokes)
            {
                const math::Vec2 origin{left, -height * 0.5f};
                placed.push_back({origin + stroke.from * height, origin + stroke.to * height});
            }

            left += letter.width * height + sampling.tracking;
        }

        GoreRandom random = goreRandom(sampling.seed);
        std::vector<math::Vec3> points;
        points.reserve(sampling.points);

        while (points.size() < sampling.points)
        {
            float pick = random.next() * totalLength;
            const Stroke* chosen = &placed.back();

            for (const Stroke& stroke : placed)
            {
                const float length = math::length(stroke.to - stroke.from);

                if (pick < length)
                {
                    chosen = &stroke;
                    break;
                }

                pick -= length;
            }

            const math::Vec2 along = chosen->to - chosen->from;
            const math::Vec2 side = math::normalize(math::Vec2{-along.y, along.x});
            const float t = random.next();
            const float across = (random.next() + random.next() - 1.0f) * sampling.thickness * 0.5f;
            const math::Vec2 point = chosen->from + along * t + side * across;
            points.push_back({point.x, point.y, random.range(-sampling.depth, sampling.depth)});
        }

        return points;
    }
} //namespace game
