#ifndef LOCKSTEP_GAME_PRESENTATION_SPRINGS_HPP
#define LOCKSTEP_GAME_PRESENTATION_SPRINGS_HPP

#include <algorithm>
#include <cmath>

namespace game
{
    // Largest sub-step stepSpring() takes, in seconds, so stiff springs stay stable at low frame
    // rates.
    inline constexpr float k_maxSpringStep{1.0f / 240.0f};

    // Advances a damped spring pulling `position` back to zero (presentation motion: sway,
    // recoil, landing dip). Works for floats and vectors.
    // position:  the spring's displacement, updated.
    // rate:      its velocity, updated.
    // stiffness: pull towards zero, 1/s^2.
    // damping:   velocity damping, 1/s.
    // seconds:   time to advance.
    template<typename T>
    void stepSpring(T& position, T& rate, float stiffness, float damping, float seconds)
    {
        const int steps = std::max(1, static_cast<int>(std::ceil(seconds / k_maxSpringStep)));
        const float step = seconds / static_cast<float>(steps);

        for (int index = 0; index < steps; ++index)
        {
            rate = rate + (position * -stiffness - rate * damping) * step;
            position = position + rate * step;
        }
    }
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_SPRINGS_HPP
