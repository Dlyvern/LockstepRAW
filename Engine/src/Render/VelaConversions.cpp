#include "VelaConversions.hpp"

#include <algorithm>

namespace lockstep::render
{
    namespace
    {
        constexpr float k_materialAnisotropy{16.0f};
    } //namespace

    math::Mat4 toViewMatrix(const Camera& camera)
    {
        return math::lookAt(camera.position, camera.target, camera.up);
    }

    math::Mat4 toProjectionMatrix(const Camera& camera, float aspectRatio)
    {
        math::Mat4 projection = math::perspective(camera.verticalFov, aspectRatio, camera.nearPlane, camera.farPlane);

        projection[1][1] *= -1.0f;

        return projection;
    }

    vela::graphics::TextureFormat toVelaFormat(TextureFormat format)
    {
        switch (format)
        {
            case TextureFormat::Rgba8Srgb:
            {
                return vela::graphics::TextureFormat::RGBA8Srgb;
            }
            case TextureFormat::Rgba8Unorm:
            {
                return vela::graphics::TextureFormat::RGBA8Unorm;
            }
            case TextureFormat::Bc1Srgb:
            {
                return vela::graphics::TextureFormat::BC1Srgb;
            }
            case TextureFormat::Bc1Unorm:
            {
                return vela::graphics::TextureFormat::BC1Unorm;
            }
            case TextureFormat::Bc3Srgb:
            {
                return vela::graphics::TextureFormat::BC3Srgb;
            }
            case TextureFormat::Bc3Unorm:
            {
                return vela::graphics::TextureFormat::BC3Unorm;
            }
            case TextureFormat::Bc7Srgb:
            {
                return vela::graphics::TextureFormat::BC7Srgb;
            }
            case TextureFormat::Bc7Unorm:
            {
                return vela::graphics::TextureFormat::BC7Unorm;
            }
            case TextureFormat::Bc4Unorm:
            {
                return vela::graphics::TextureFormat::BC4Unorm;
            }
            case TextureFormat::Bc5Unorm:
            {
                return vela::graphics::TextureFormat::BC5Unorm;
            }
        }

        return vela::graphics::TextureFormat::RGBA8Srgb;
    }

    vela::graphics::SamplerDescription toVelaSampler(TextureFilter filter)
    {
        const vela::graphics::Filter velaFilter = filter == TextureFilter::Nearest
            ? vela::graphics::Filter::Nearest
            : vela::graphics::Filter::Linear;

        vela::graphics::SamplerDescription sampler{};
        sampler.minFilter = velaFilter;
        sampler.magFilter = velaFilter;
        sampler.anisotropy = filter == TextureFilter::Linear ? k_materialAnisotropy : 1.0f;

        return sampler;
    }

    vela::core::VSync toVelaVSync(PresentMode mode, std::span<const vela::core::VSync> supported)
    {
        switch (mode)
        {
            case PresentMode::Uncapped:
            {
                return vela::core::VSync::Off;
            }
            case PresentMode::LowLatency:
            {
                const bool hasLatestReady = std::find(supported.begin(), supported.end(),
                    vela::core::VSync::LatestReady) != supported.end();

                return hasLatestReady ? vela::core::VSync::LatestReady : vela::core::VSync::Fast;
            }
            case PresentMode::VSync:
            {
                break;
            }
        }

        return vela::core::VSync::On;
    }
} //namespace lockstep::render
