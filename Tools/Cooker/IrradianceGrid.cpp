#include "IrradianceGrid.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace lockstep::cooker
{
    namespace
    {
        constexpr float k_spacingGrowth{1.05f};
        constexpr float k_minSpacing{0.05f};
        constexpr std::size_t k_coefficientsPerTerm{12};
        constexpr std::size_t k_floatsPerProbe{sizeof(render::IrradianceProbe) / sizeof(float)};

        using ProbeFloats = std::array<float, k_floatsPerProbe>;

        void addTerm(float* term, const math::Vec3& radiance, const math::Vec3& direction, float constantWeight,
            float linearWeight)
        {
            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                float* coefficients = term + channel * 4;
                const float value = radiance[channel];
                coefficients[0] += value * constantWeight;
                coefficients[1] += value * linearWeight * direction.x;
                coefficients[2] += value * linearWeight * direction.y;
                coefficients[3] += value * linearWeight * direction.z;
            }
        }

        math::Vec3 evaluateTerm(const float* term, const math::Vec3& normal)
        {
            math::Vec3 result{};

            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                const float* coefficients = term + channel * 4;
                result[channel] = std::max(coefficients[0] + coefficients[1] * normal.x + coefficients[2] * normal.y
                    + coefficients[3] * normal.z, 0.0f);
            }

            return result;
        }

        uint32_t countFor(float extent, float spacing)
        {
            return std::max(2u, static_cast<uint32_t>(std::ceil(extent / spacing - 1.0e-4f)) + 1u);
        }
    } //namespace

    ProbeAccumulator::ProbeAccumulator(uint32_t rayCount)
        : m_constantWeight(1.0f / static_cast<float>(std::max(rayCount, 1u)))
        , m_linearWeight(2.0f / static_cast<float>(std::max(rayCount, 1u)))
    {
    }

    void ProbeAccumulator::add(const math::Vec3& direction, const math::Vec3& local, const math::Vec3& sun,
        const math::Vec3& sky)
    {
        addTerm(m_probe.local, local, direction, m_constantWeight, m_linearWeight);
        addTerm(m_probe.sun, sun, direction, m_constantWeight, m_linearWeight);
        addTerm(m_probe.sky, sky, direction, m_constantWeight, m_linearWeight);
    }

    void ProbeAccumulator::addSky(const math::Vec3& direction)
    {
        m_probe.skyVisibility[0] += m_constantWeight;
        m_probe.skyVisibility[1] += m_linearWeight * direction.x;
        m_probe.skyVisibility[2] += m_linearWeight * direction.y;
        m_probe.skyVisibility[3] += m_linearWeight * direction.z;
    }

    const render::IrradianceProbe& ProbeAccumulator::probe() const noexcept
    {
        return m_probe;
    }

    IrradianceGrid IrradianceGrid::covering(const math::Aabb& bounds, float spacing)
    {
        IrradianceGrid grid;
        float step = std::max(spacing, k_minSpacing);
        const math::Vec3 centre = math::center(bounds);
        const math::Vec3 size = math::size(bounds);

        while (true)
        {
            grid.m_countX = countFor(size.x + step, step);
            grid.m_countY = countFor(size.y + step, step);
            grid.m_countZ = countFor(size.z + step, step);
            const uint64_t total = static_cast<uint64_t>(grid.m_countX) * grid.m_countY * grid.m_countZ;

            if (total <= render::k_maxIrradianceProbes
                && std::max({grid.m_countX, grid.m_countY, grid.m_countZ}) <= render::k_maxIrradianceProbesPerAxis)
            {
                break;
            }

            step *= k_spacingGrowth;
        }

        grid.m_spacing = math::Vec3{step, step, step};
        grid.m_origin = centre - math::Vec3{static_cast<float>(grid.m_countX - 1), static_cast<float>(grid.m_countY - 1),
            static_cast<float>(grid.m_countZ - 1)} * (0.5f * step);
        grid.m_probes.assign(grid.probeCount(), render::IrradianceProbe{});
        grid.m_valid.assign(grid.probeCount(), true);

        return grid;
    }

    uint32_t IrradianceGrid::countX() const noexcept
    {
        return m_countX;
    }

    uint32_t IrradianceGrid::countY() const noexcept
    {
        return m_countY;
    }

    uint32_t IrradianceGrid::countZ() const noexcept
    {
        return m_countZ;
    }

    uint32_t IrradianceGrid::probeCount() const noexcept
    {
        return m_countX * m_countY * m_countZ;
    }

    math::Vec3 IrradianceGrid::spacing() const noexcept
    {
        return m_spacing;
    }

    math::Vec3 IrradianceGrid::probePosition(uint32_t probe) const noexcept
    {
        const uint32_t x = probe % m_countX;
        const uint32_t y = (probe / m_countX) % m_countY;
        const uint32_t z = probe / (m_countX * m_countY);

        return m_origin + math::Vec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)} * m_spacing;
    }

    void IrradianceGrid::setValidity(std::vector<bool> valid)
    {
        m_valid = std::move(valid);
    }

    uint32_t IrradianceGrid::invalidCount() const noexcept
    {
        return static_cast<uint32_t>(std::count(m_valid.begin(), m_valid.end(), false));
    }

    void IrradianceGrid::setProbes(const std::vector<render::IrradianceProbe>& probes)
    {
        m_probes = probes;
    }

    uint32_t IrradianceGrid::fillInvalid()
    {
        std::vector<bool> filled = m_valid;
        std::vector<uint32_t> newlyFilled;

        for (uint32_t probe = 0; probe < probeCount(); ++probe)
        {
            if (!filled[probe])
            {
                m_probes[probe] = render::IrradianceProbe{};
            }
        }

        do
        {
            newlyFilled.clear();
            std::vector<render::IrradianceProbe> updated = m_probes;

            for (uint32_t z = 0; z < m_countZ; ++z)
            {
                for (uint32_t y = 0; y < m_countY; ++y)
                {
                    for (uint32_t x = 0; x < m_countX; ++x)
                    {
                        const uint32_t probe = indexOf(x, y, z);

                        if (filled[probe])
                        {
                            continue;
                        }

                        ProbeFloats sum{};
                        uint32_t neighbours = 0;

                        for (uint32_t nz = z > 0 ? z - 1 : 0; nz <= std::min(z + 1, m_countZ - 1); ++nz)
                        {
                            for (uint32_t ny = y > 0 ? y - 1 : 0; ny <= std::min(y + 1, m_countY - 1); ++ny)
                            {
                                for (uint32_t nx = x > 0 ? x - 1 : 0; nx <= std::min(x + 1, m_countX - 1); ++nx)
                                {
                                    const uint32_t neighbour = indexOf(nx, ny, nz);

                                    if (!filled[neighbour])
                                    {
                                        continue;
                                    }

                                    const auto values = std::bit_cast<ProbeFloats>(m_probes[neighbour]);

                                    for (std::size_t index = 0; index < values.size(); ++index)
                                    {
                                        sum[index] += values[index];
                                    }

                                    ++neighbours;
                                }
                            }
                        }

                        if (neighbours == 0)
                        {
                            continue;
                        }

                        for (float& value : sum)
                        {
                            value /= static_cast<float>(neighbours);
                        }

                        updated[probe] = std::bit_cast<render::IrradianceProbe>(sum);
                        newlyFilled.push_back(probe);
                    }
                }
            }

            m_probes = std::move(updated);

            for (const uint32_t probe : newlyFilled)
            {
                filled[probe] = true;
            }
        }
        while (!newlyFilled.empty());

        return static_cast<uint32_t>(std::count(filled.begin(), filled.end(), false));
    }

    IndirectLight IrradianceGrid::evaluate(const math::Vec3& position, const math::Vec3& normal) const
    {
        const math::Vec3 grid = (position - m_origin) / m_spacing;
        const std::array<uint32_t, 3> counts{m_countX, m_countY, m_countZ};
        std::array<uint32_t, 3> low{};
        std::array<float, 3> fraction{};

        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            const float clamped = std::clamp(grid[axis], 0.0f, static_cast<float>(counts[axis] - 1));
            low[axis] = std::min(static_cast<uint32_t>(clamped), counts[axis] - 2);
            fraction[axis] = clamped - static_cast<float>(low[axis]);
        }

        ProbeFloats blended{};

        for (uint32_t corner = 0; corner < 8; ++corner)
        {
            const uint32_t dx = corner & 1u;
            const uint32_t dy = (corner >> 1) & 1u;
            const uint32_t dz = (corner >> 2) & 1u;
            const float weight = (dx != 0 ? fraction[0] : 1.0f - fraction[0]) * (dy != 0 ? fraction[1] : 1.0f - fraction[1])
                * (dz != 0 ? fraction[2] : 1.0f - fraction[2]);

            if (weight <= 0.0f)
            {
                continue;
            }

            const auto values = std::bit_cast<ProbeFloats>(m_probes[indexOf(low[0] + dx, low[1] + dy, low[2] + dz)]);

            for (std::size_t index = 0; index < 3 * k_coefficientsPerTerm; ++index)
            {
                blended[index] += values[index] * weight;
            }
        }

        return IndirectLight{.local = evaluateTerm(blended.data(), normal),
            .sun = evaluateTerm(blended.data() + k_coefficientsPerTerm, normal),
            .sky = evaluateTerm(blended.data() + 2 * k_coefficientsPerTerm, normal)};
    }

    render::IrradianceVolume IrradianceGrid::volume() const
    {
        return render::IrradianceVolume{.origin = m_origin, .spacing = m_spacing, .countX = m_countX,
            .countY = m_countY, .countZ = m_countZ, .sunTravel = {0.0f, 0.0f, 0.0f}, .probes = m_probes};
    }

    uint32_t IrradianceGrid::indexOf(uint32_t x, uint32_t y, uint32_t z) const noexcept
    {
        return (z * m_countY + y) * m_countX + x;
    }
} //namespace lockstep::cooker
