#ifndef LOCKSTEP_ASSETS_COOKED_IRRADIANCE_HPP
#define LOCKSTEP_ASSETS_COOKED_IRRADIANCE_HPP

#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/IrradianceVolume.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

// The baked irradiance file "<level folder>/irradiance", written by LockstepCooker --gi next to the
// level file and read by loadCookedScene(). Little-endian:
//
//   char[4]  magic "LIRR"
//   uint32   k_irradianceVersion
//   float[3] origin, float[3] spacing
//   uint32   countX, countY, countZ
//   float[3] sunTravel
//   then countX * countY * countZ probes (x fastest, then y, then z), each the 40 floats of
//   render::IrradianceProbe in declaration order: local[12], sun[12], sky[12], skyVisibility[4]
namespace lockstep::assets
{
    // Version of irradiance files; a loader rejects others.
    inline constexpr uint32_t k_irradianceVersion{1};

    // First four bytes of an irradiance file.
    inline constexpr std::array<char, 4> k_irradianceMagic{'L', 'I', 'R', 'R'};

    // Name of the irradiance file inside a level's folder.
    inline constexpr std::string_view k_irradianceFileName{"irradiance"};

    static_assert(sizeof(render::IrradianceProbe) == 40 * sizeof(float));

    // Serializes an irradiance volume (equal volumes give equal bytes).
    // volume:  the volume; its probe count must match its counts.
    // Returns: the file contents.
    [[nodiscard]] std::vector<std::byte> writeCookedIrradiance(const render::IrradianceVolume& volume);

    // Parses an irradiance file.
    // bytes:   the file contents.
    // path:    the file's virtual path, for messages.
    // Returns: the volume, or AssetLoadFailed for another format or version, counts out of range,
    //          a wrong size or a value that is not finite.
    [[nodiscard]] Result<render::IrradianceVolume> readCookedIrradiance(std::span<const std::byte> bytes,
        std::string_view path);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_COOKED_IRRADIANCE_HPP
