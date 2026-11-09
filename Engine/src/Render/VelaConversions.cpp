#include "VelaConversions.hpp"

namespace lockstep::render
{
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

        return sampler;
    }
} //namespace lockstep::render
