#ifndef LOCKSTEP_COOKER_IRRADIANCE_GRID_HPP
#define LOCKSTEP_COOKER_IRRADIANCE_GRID_HPP

#include <Lockstep/Core/Bounds.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/IrradianceVolume.hpp>

#include <cstdint>
#include <vector>

namespace lockstep::cooker
{
    // Indirect light arriving at a surface, divided by pi, per term of render::IrradianceProbe.
    struct IndirectLight
    {
        math::Vec3 local{0.0f, 0.0f, 0.0f};
        math::Vec3 sun{0.0f, 0.0f, 0.0f};
        math::Vec3 sky{0.0f, 0.0f, 0.0f};
    };

    // Sums the radiance a probe's rays bring back into cosine-convolved order-1 spherical harmonics
    // (the render::IrradianceProbe layout). Rays must cover the sphere uniformly.
    class ProbeAccumulator
    {
    public:
        // rayCount: rays that will be added (misses included), each weighing 4 pi / rayCount.
        explicit ProbeAccumulator(uint32_t rayCount);

        // Adds a ray that hit a surface.
        // direction: unit direction of the ray from the probe.
        // local, sun, sky: radiance it brings back per term.
        void add(const math::Vec3& direction, const math::Vec3& local, const math::Vec3& sun, const math::Vec3& sky);

        // Adds a ray that left the level (sky radiance 1 for IrradianceProbe::skyVisibility).
        // direction: unit direction of the ray from the probe.
        void addSky(const math::Vec3& direction);

        // Returns: the probe.
        [[nodiscard]] const render::IrradianceProbe& probe() const noexcept;

    private:
        float m_constantWeight{0.0f};
        float m_linearWeight{0.0f};
        render::IrradianceProbe m_probe{};
    };

    // A probe grid during the bake: layout, probe data, which probes are valid (not inside
    // geometry), and trilinear evaluation for the next bounce.
    class IrradianceGrid
    {
    public:
        // Lays out a grid covering a box, centred on it with the box padded by half a spacing on
        // every side. The spacing grows (by 5 % steps) until the grid fits
        // render::k_maxIrradianceProbes and k_maxIrradianceProbesPerAxis. Every probe starts valid
        // and zero.
        // bounds:  the box to cover.
        // spacing: wanted spacing in metres.
        // Returns: the grid.
        [[nodiscard]] static IrradianceGrid covering(const math::Aabb& bounds, float spacing);

        // Returns: probes along x, y or z.
        [[nodiscard]] uint32_t countX() const noexcept;
        [[nodiscard]] uint32_t countY() const noexcept;
        [[nodiscard]] uint32_t countZ() const noexcept;

        // Returns: all probes.
        [[nodiscard]] uint32_t probeCount() const noexcept;

        // Returns: the distance between neighbouring probes.
        [[nodiscard]] math::Vec3 spacing() const noexcept;

        // probe:   probe index (x fastest, then y, then z).
        // Returns: its world position.
        [[nodiscard]] math::Vec3 probePosition(uint32_t probe) const noexcept;

        // Sets which probes hold usable data.
        // valid: one flag per probe.
        void setValidity(std::vector<bool> valid);

        // Returns: probes marked invalid by setValidity().
        [[nodiscard]] uint32_t invalidCount() const noexcept;

        // Replaces the probe data.
        // probes: one per probe.
        void setProbes(const std::vector<render::IrradianceProbe>& probes);

        // Overwrites every invalid probe with the mean of its valid neighbours (all 26), repeating
        // with the newly filled ones until nothing changes. Probes no valid probe reaches are zeroed.
        // The validity flags are kept.
        // Returns: probes left zeroed.
        uint32_t fillInvalid();

        // Evaluates the grid with trilinear filtering (clamped at the edges).
        // position: where.
        // normal:   unit normal of the receiving surface.
        // Returns:  the indirect light per term.
        [[nodiscard]] IndirectLight evaluate(const math::Vec3& position, const math::Vec3& normal) const;

        // Returns: the grid as a volume (sunTravel left zero).
        [[nodiscard]] render::IrradianceVolume volume() const;

    private:
        IrradianceGrid() = default;

        [[nodiscard]] uint32_t indexOf(uint32_t x, uint32_t y, uint32_t z) const noexcept;

        math::Vec3 m_origin{0.0f, 0.0f, 0.0f};
        math::Vec3 m_spacing{1.0f, 1.0f, 1.0f};
        uint32_t m_countX{2};
        uint32_t m_countY{2};
        uint32_t m_countZ{2};
        std::vector<render::IrradianceProbe> m_probes;
        std::vector<bool> m_valid;
    };
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_IRRADIANCE_GRID_HPP
