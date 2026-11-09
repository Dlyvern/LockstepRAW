#include "Lockstep/Render/Resources.hpp"

#include "VelaConversions.hpp"

#include <algorithm>

namespace lockstep::render
{
    std::size_t textureLevelBytes(TextureFormat format, uint32_t width, uint32_t height) noexcept
    {
        return vela::graphics::imageSizeBytes(toVelaFormat(format), width, height);
    }

    uint32_t fullMipLevelCount(uint32_t width, uint32_t height) noexcept
    {
        uint32_t levels = 1;
        uint32_t largest = std::max(width, height);

        while (largest > 1)
        {
            largest /= 2;
            ++levels;
        }

        return levels;
    }
} //namespace lockstep::render
