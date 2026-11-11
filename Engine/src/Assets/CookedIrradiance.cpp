#include "CookedIrradiance.hpp"

#include "BinaryStream.hpp"
#include "CookedFields.hpp"

#include <bit>
#include <cmath>
#include <string>

namespace lockstep::assets
{
    namespace
    {
        Error irradianceError(std::string_view path, std::string_view what)
        {
            return Error{ErrorCode::AssetLoadFailed, "irradiance '" + std::string(path) + "': " + std::string(what)};
        }

        bool isFinite(const math::Vec3& vector)
        {
            return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
        }

        bool isValidCount(uint32_t count)
        {
            return count >= 2 && count <= render::k_maxIrradianceProbesPerAxis;
        }

        bool isFinite(const render::IrradianceProbe& probe)
        {
            using ProbeFloats = std::array<float, sizeof(render::IrradianceProbe) / sizeof(float)>;

            for (const float value : std::bit_cast<ProbeFloats>(probe))
            {
                if (!std::isfinite(value))
                {
                    return false;
                }
            }

            return true;
        }
    } //namespace

    std::vector<std::byte> writeCookedIrradiance(const render::IrradianceVolume& volume)
    {
        BinaryWriter writer;
        writer.write(k_irradianceMagic);
        writer.write(k_irradianceVersion);
        writeVec3(writer, volume.origin);
        writeVec3(writer, volume.spacing);
        writer.write(volume.countX);
        writer.write(volume.countY);
        writer.write(volume.countZ);
        writeVec3(writer, volume.sunTravel);
        writer.writeBytes(std::as_bytes(std::span(volume.probes)));

        return writer.take();
    }

    Result<render::IrradianceVolume> readCookedIrradiance(std::span<const std::byte> bytes, std::string_view path)
    {
        BinaryReader reader(bytes);
        std::array<char, 4> magic{};
        uint32_t version = 0;
        render::IrradianceVolume volume{};
        reader.read(magic);
        reader.read(version);

        if (!reader.ok() || magic != k_irradianceMagic)
        {
            return irradianceError(path, "not an irradiance file");
        }

        if (version != k_irradianceVersion)
        {
            return irradianceError(path, "version " + std::to_string(version) + ", expected "
                + std::to_string(k_irradianceVersion));
        }

        readVec3(reader, volume.origin);
        readVec3(reader, volume.spacing);
        reader.read(volume.countX);
        reader.read(volume.countY);
        reader.read(volume.countZ);
        readVec3(reader, volume.sunTravel);

        if (!reader.ok() || !isValidCount(volume.countX) || !isValidCount(volume.countY) || !isValidCount(volume.countZ))
        {
            return irradianceError(path, "damaged header");
        }

        const uint64_t count = static_cast<uint64_t>(volume.countX) * volume.countY * volume.countZ;

        if (count > render::k_maxIrradianceProbes || !isFinite(volume.origin) || !isFinite(volume.spacing)
            || !isFinite(volume.sunTravel) || !(volume.spacing.x > 0.0f) || !(volume.spacing.y > 0.0f)
            || !(volume.spacing.z > 0.0f))
        {
            return irradianceError(path, "damaged header");
        }

        volume.probes.resize(static_cast<std::size_t>(count));
        reader.readBytes(std::as_writable_bytes(std::span(volume.probes)));

        if (!reader.ok() || !reader.atEnd())
        {
            return irradianceError(path, "wrong size");
        }

        for (const render::IrradianceProbe& probe : volume.probes)
        {
            if (!isFinite(probe))
            {
                return irradianceError(path, "a probe value is not finite");
            }
        }

        return volume;
    }
} //namespace lockstep::assets
