#ifndef LOCKSTEP_WORLD_TIME_DOMAINS_HPP
#define LOCKSTEP_WORLD_TIME_DOMAINS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace lockstep::world
{
    // The clocks simulation code runs on. Each has its own time scale.
    // Player: the players' own simulation. Always scale 1, so network prediction never sees
    //         scaled time.
    // World:  everything else in the world. Game may slow it down or speed it up.
    enum class TimeDomain : uint8_t
    {
        Player,
        World,
        Count
    };

    // Per-domain time scale and simulated time. The run loop calls advance() once before every
    // fixed tick; simulation code then reads its domain's delta() for that tick. A scale set
    // during a tick takes effect from the next tick on, so every tick sees one consistent delta.
    class TimeDomains
    {
    public:
        // Sets a domain's time scale.
        // domain: the domain. Player is locked to 1: setting it asserts and is ignored.
        // scale:  1 is real time, 0 freezes the domain. Negative values are clamped to 0.
        void setScale(TimeDomain domain, float scale);

        // domain:  the domain.
        // Returns: the domain's time scale, as set (it applies from the next tick).
        [[nodiscard]] float scale(TimeDomain domain) const;

        // domain:  the domain.
        // Returns: seconds the domain advances during the current tick: the tick length times the
        //          scale that was in effect when the tick started.
        [[nodiscard]] float delta(TimeDomain domain) const;

        // domain:  the domain.
        // Returns: total simulated seconds of the domain, including the current tick.
        [[nodiscard]] double elapsed(TimeDomain domain) const;

        // Starts a new tick: computes each domain's delta from the tick length and its scale and
        // adds it to the domain's elapsed time. Called by the run loop; Game does not call it.
        // tickSeconds: the fixed tick length.
        void advance(float tickSeconds);

    private:
        static constexpr std::size_t k_domainCount{static_cast<std::size_t>(TimeDomain::Count)};

        std::array<float, k_domainCount> m_scales{1.0f, 1.0f};
        std::array<float, k_domainCount> m_deltas{};
        std::array<double, k_domainCount> m_elapsed{};
    };
} //namespace lockstep::world

#endif //LOCKSTEP_WORLD_TIME_DOMAINS_HPP
