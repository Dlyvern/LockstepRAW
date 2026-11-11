#ifndef LOCKSTEP_COOKER_RAY_TRACER_HPP
#define LOCKSTEP_COOKER_RAY_TRACER_HPP

#include <Lockstep/Core/Bounds.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lockstep::cooker
{
    // How rays treat the triangles of one surface (one material).
    // coverage:      share of the surface that is solid (alpha-masked foliage); a ray passes through
    //                a triangle with probability 1 - coverage, decided by a hash of the ray's seed and
    //                the triangle, so the same ray always gives the same answer.
    // blocksShadows: false lets occlusion tests pass through (light fixtures' emissive shades).
    struct RaySurface
    {
        float coverage{1.0f};
        bool blocksShadows{true};
    };

    // A world-space triangle and the surface it belongs to.
    // corners: the three corners; counter-clockwise seen from the front.
    // surface: index into the surfaces given to RayTracer::create().
    struct RayTriangle
    {
        math::Vec3 corners[3];
        uint32_t surface{0};
    };

    // A ray segment.
    // origin, direction: start and unit direction.
    // maxDistance:       hits beyond this distance are ignored.
    // seed:              per-ray value for the coverage hash (see RaySurface::coverage).
    struct Ray
    {
        math::Vec3 origin{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 0.0f, 1.0f};
        float maxDistance{1.0e30f};
        uint64_t seed{0};
    };

    // Where a ray hit.
    // distance:   along the ray.
    // surface:    the triangle's surface.
    // normal:     the triangle's unit geometric normal, facing its front.
    // isBackFace: the ray hit the triangle's back (its normal points along the ray).
    struct RayHit
    {
        float distance{0.0f};
        uint32_t surface{0};
        math::Vec3 normal{0.0f, 1.0f, 0.0f};
        bool isBackFace{false};
    };

    // Casts rays against a fixed set of triangles through a bounding volume hierarchy (binned SAH,
    // built single-threaded so the same triangles always give the same tree). Thread-safe to query.
    class RayTracer
    {
    public:
        // Builds the hierarchy.
        // triangles: the triangles; degenerate ones are dropped.
        // surfaces:  per surface index, how rays treat it.
        // Returns:   the tracer, or InvalidArgument if no triangle is left or one names a surface out
        //            of range.
        [[nodiscard]] static Result<RayTracer> create(std::span<const RayTriangle> triangles,
            std::vector<RaySurface> surfaces);

        // Finds the nearest hit along a ray (front or back face).
        // ray:     the ray.
        // Returns: the hit, or nothing if the ray leaves without hitting anything.
        [[nodiscard]] std::optional<RayHit> closestHit(const Ray& ray) const;

        // Tells whether anything that blocks shadows lies on a ray segment.
        // ray:     the segment.
        // Returns: true at the first such hit.
        [[nodiscard]] bool isOccluded(const Ray& ray) const;

        // Returns: triangles in the hierarchy.
        [[nodiscard]] std::size_t triangleCount() const noexcept;

        // Returns: bounds of every triangle.
        [[nodiscard]] math::Aabb bounds() const noexcept;

    private:
        struct Node
        {
            math::Aabb bounds{};
            uint32_t first{0};
            uint32_t count{0};
        };

        struct PackedTriangle
        {
            math::Vec3 corner{0.0f, 0.0f, 0.0f};
            math::Vec3 edge1{0.0f, 0.0f, 0.0f};
            math::Vec3 edge2{0.0f, 0.0f, 0.0f};
            uint32_t surface{0};
            uint32_t id{0};
        };

        template<bool k_anyHit>
        bool traverse(const Ray& ray, RayHit* hit) const;

        RayTracer() = default;

        std::vector<Node> m_nodes;
        std::vector<PackedTriangle> m_triangles;
        std::vector<RaySurface> m_surfaces;
    };
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_RAY_TRACER_HPP
