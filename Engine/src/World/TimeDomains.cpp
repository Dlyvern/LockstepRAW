#include "Lockstep/World/TimeDomains.hpp"

#include <algorithm>
#include <cassert>

namespace lockstep::world
{
    namespace
    {
        std::size_t toIndex(TimeDomain domain)
        {
            return static_cast<std::size_t>(domain);
        }

        bool isValid(TimeDomain domain)
        {
            return domain < TimeDomain::Count;
        }
    } //namespace

    void TimeDomains::setScale(TimeDomain domain, float scale)
    {
        assert(domain != TimeDomain::Player && "TimeDomains: the Player domain always runs at scale 1");
        assert(isValid(domain) && "TimeDomains: invalid domain");

        if (domain == TimeDomain::Player || !isValid(domain))
        {
            return;
        }

        m_scales[toIndex(domain)] = std::max(scale, 0.0f);
    }

    float TimeDomains::scale(TimeDomain domain) const
    {
        return isValid(domain) ? m_scales[toIndex(domain)] : 0.0f;
    }

    float TimeDomains::delta(TimeDomain domain) const
    {
        return isValid(domain) ? m_deltas[toIndex(domain)] : 0.0f;
    }

    double TimeDomains::elapsed(TimeDomain domain) const
    {
        return isValid(domain) ? m_elapsed[toIndex(domain)] : 0.0;
    }

    void TimeDomains::advance(float tickSeconds)
    {
        for (std::size_t index = 0; index < k_domainCount; ++index)
        {
            m_deltas[index] = tickSeconds * m_scales[index];
            m_elapsed[index] += m_deltas[index];
        }
    }
} //namespace lockstep::world
