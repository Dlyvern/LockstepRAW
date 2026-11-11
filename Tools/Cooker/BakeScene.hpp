#ifndef LOCKSTEP_COOKER_BAKE_SCENE_HPP
#define LOCKSTEP_COOKER_BAKE_SCENE_HPP

#include <Lockstep/Core/Bounds.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>

#include "RayTracer.hpp"
#include "SceneSource.hpp"
#include "TextureCook.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lockstep::cooker
{
    // How a surface (one source material) reflects and emits light in the lighting bake, as
    // per-material constants (no texture lookups).
    // albedo:      diffuse colour: base colour factor times the base colour texture's mean, times
    //              one minus the mean metalness.
    // emissive:    emitted radiance: emissive factor times the emissive texture's mean.
    // doubleSided: back faces are lit like front faces (with the normal flipped).
    struct BakeSurface
    {
        math::Vec3 albedo{0.0f, 0.0f, 0.0f};
        math::Vec3 emissive{0.0f, 0.0f, 0.0f};
        bool doubleSided{false};
    };

    // A point of a level light as the bake uses it, in renderer units (see SceneLights.hpp).
    // position:  where it is.
    // radiance:  colour times renderer intensity.
    // range:     where its falloff window ends.
    // direction: spot only: unit direction it shines in.
    // cosOuter / cosInner: spot only: cosines of the cone's outer and inner half-angles.
    // isSpot:    a spot light (else a point light).
    struct BakeLight
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Vec3 radiance{0.0f, 0.0f, 0.0f};
        float range{1.0f};
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
        float cosOuter{-1.0f};
        float cosInner{-1.0f};
        bool isSpot{false};
    };

    // Direct light arriving at a surface point, divided by pi (the radiance a white Lambert surface
    // there reflects).
    // local: from the level's lights, RGB.
    // sun:   from a sun of radiance 1 (n . toSun when unshadowed).
    // sky:   from a uniform sky of radiance 1 (the visible share of the cosine-weighted hemisphere).
    struct DirectLight
    {
        math::Vec3 local{0.0f, 0.0f, 0.0f};
        float sun{0.0f};
        float sky{0.0f};
    };

    // What gathering a scene for the bake kept and dropped.
    struct BakeSceneReport
    {
        uint32_t instances{0};
        uint32_t skippedInstances{0};
        uint32_t lights{0};
        uint32_t tubeLights{0};
        uint32_t skippedLights{0};
    };

    // A level reduced to what the irradiance bake needs: the static opaque and masked geometry in a
    // ray tracer, per-material reflectance, the static lights and the sun. Thread-safe to query.
    class BakeScene
    {
    public:
        // Gathers the scene. Skipped: alpha-blended materials, instances under nodes named
        // Interact_*, Breakable_Glass*, Toy_*, Decal_*, Npc_* or Spawn_*, skinned models, and
        // lights whose names contain _Flicker, _Siren, _Pulse or _NoBounce (animated lights). A light
        // named *_Tube* is a line along its local +X, as long as its world X scale, taken as four
        // points sharing its intensity.
        // scene:     the source scene.
        // means:     per scene texture, its mean colour and alpha coverage.
        // sunTravel: unit direction sunlight travels in, or nothing for no sun.
        // Returns:   the bake scene, or InvalidArgument if no geometry is left.
        [[nodiscard]] static Result<BakeScene> create(const SourceScene& scene, std::span<const TextureMean> means,
            std::optional<math::Vec3> sunTravel);

        // Computes the direct light at a surface point, with shadow rays to every light in range,
        // to the sun and over the hemisphere for the sky.
        // position: the point.
        // normal:   unit normal of the lit side.
        // seed:     per-point value picking the sky rays (same seed, same result).
        // Returns:  the light.
        [[nodiscard]] DirectLight directLight(const math::Vec3& position, const math::Vec3& normal, uint64_t seed) const;

        // Returns: the ray tracer over the scene's geometry.
        [[nodiscard]] const RayTracer& tracer() const noexcept;

        // surface: a RayHit::surface.
        // Returns: its reflectance.
        [[nodiscard]] const BakeSurface& surface(uint32_t surface) const noexcept;

        // Returns: what was kept and dropped.
        [[nodiscard]] const BakeSceneReport& report() const noexcept;

    private:
        explicit BakeScene(RayTracer tracer)
            : m_tracer(std::move(tracer))
        {
        }

        void buildLightGrid();

        [[nodiscard]] std::span<const uint32_t> lightsNear(const math::Vec3& position) const;

        RayTracer m_tracer;
        std::vector<BakeSurface> m_surfaces;
        std::vector<BakeLight> m_lights;
        std::optional<math::Vec3> m_toSun;
        BakeSceneReport m_report{};
        math::Aabb m_gridBounds{};
        math::Vec3i m_gridSize{0, 0, 0};
        float m_cellSize{1.0f};
        std::vector<uint32_t> m_cellStart;
        std::vector<uint32_t> m_cellLights;
    };
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_BAKE_SCENE_HPP
