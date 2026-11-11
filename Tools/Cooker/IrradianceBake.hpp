#ifndef LOCKSTEP_COOKER_IRRADIANCE_BAKE_HPP
#define LOCKSTEP_COOKER_IRRADIANCE_BAKE_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Render/IrradianceVolume.hpp>

#include "SceneSource.hpp"
#include "TextureCook.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace lockstep::cooker
{
    // Settings of the irradiance probe bake (--gi and friends).
    // spacing: wanted distance between probes in metres (--gi-spacing); grown when the volume would
    //          get too many probes.
    // sun:     sun azimuth and elevation in degrees (--gi-sun; azimuth 0 towards -Z, 90 towards +X),
    //          or nothing for a bake without sun.
    // rays:    rays traced from every probe (--gi-rays).
    // bounces: light bounces gathered (--gi-bounces); 1 is light that bounced once.
    struct IrradianceBakeOptions
    {
        float spacing{1.0f};
        std::optional<math::Vec2> sun{};
        uint32_t rays{256};
        uint32_t bounces{3};
    };

    // Bakes a level's indirect light into an irradiance volume (see render::IrradianceVolume): a
    // probe grid over the union of the Zone_Interior_* boxes (else the scene bounds), padded by half
    // a spacing. Every probe traces the same spherical Fibonacci ray set turned by a hash of its
    // index; hits carry the light their surface reflects (per-material albedo, direct light from
    // the static lights, the sun and a uniform sky with shadow rays, emissive for the local term),
    // and further bounces are gathered by evaluating the previous pass's probes at the hits.
    // Probes that see more than a quarter back faces are inside geometry and are filled from their
    // neighbours. Prints timing and probe counts. Deterministic for any thread count.
    // scene:   the source scene.
    // means:   per scene texture, its mean colour and alpha coverage (see TextureMean).
    // options: spacing, sun, rays and bounces.
    // threads: worker threads; 0 picks the hardware thread count.
    // Returns: the volume, or InvalidArgument if the scene has no geometry to bake against.
    [[nodiscard]] Result<render::IrradianceVolume> bakeIrradiance(const SourceScene& scene,
        std::span<const TextureMean> means, const IrradianceBakeOptions& options, uint32_t threads);

    // Unit direction sunlight travels in for a sun position, as the Engine's sky model defines it.
    // azimuth:   radians; 0 puts the sun towards -Z, pi/2 towards +X.
    // elevation: radians above the horizon.
    // Returns:   the direction from the sun.
    [[nodiscard]] math::Vec3 sunTravelDirection(float azimuth, float elevation);
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_IRRADIANCE_BAKE_HPP
