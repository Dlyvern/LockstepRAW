#ifndef LOCKSTEP_CORE_SPRING_HPP
#define LOCKSTEP_CORE_SPRING_HPP

#include "Lockstep/Core/Math.hpp"

namespace lockstep::math
{
    // Moves a value towards 0 like a critically damped spring (no overshoot), exactly for any
    // step. The half-life is the time the value takes to fall to about half when it starts still.
    // Inertialization offsets decay this way.
    // value:        the value, changed in place.
    // velocity:     its rate of change per second, changed in place.
    // halflife:     seconds; 0 or less snaps both to 0.
    // deltaSeconds: the step.
    void decaySpring(float& value, float& velocity, float halflife, float deltaSeconds) noexcept;

    // decaySpring() for each component of a vector.
    // value:        the value, changed in place.
    // velocity:     its rate of change per second, changed in place.
    // halflife:     seconds; 0 or less snaps both to 0.
    // deltaSeconds: the step.
    void decaySpring(Vec3& value, Vec3& velocity, float halflife, float deltaSeconds) noexcept;

    // Moves a value towards a goal like a critically damped spring (no overshoot), exactly for
    // any step.
    // value:        the value, changed in place.
    // velocity:     its rate of change per second, changed in place.
    // goal:         where it settles.
    // halflife:     seconds; 0 or less snaps to the goal.
    // deltaSeconds: the step.
    void criticalSpring(float& value, float& velocity, float goal, float halflife, float deltaSeconds) noexcept;

    // Moves a value towards a goal like a damped spring, which may overshoot and wobble (lean,
    // sway). Stable for any step: long steps are split.
    // value:        the value, changed in place.
    // velocity:     its rate of change per second, changed in place.
    // goal:         where it settles.
    // frequency:    undamped oscillation frequency, Hz.
    // dampingRatio: 1 is critically damped, below 1 overshoots.
    // deltaSeconds: the step.
    void dampedSpring(float& value, float& velocity, float goal, float frequency, float dampingRatio,
        float deltaSeconds) noexcept;
} //namespace lockstep::math

#endif //LOCKSTEP_CORE_SPRING_HPP
