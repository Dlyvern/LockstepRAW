#ifndef LOCKSTEP_RENDER_ENVIRONMENT_RESOURCES_HPP
#define LOCKSTEP_RENDER_ENVIRONMENT_RESOURCES_HPP

#include "Lockstep/Core/Result.hpp"

#include <Vela/Graphics/Texture.hpp>

#include <vector>

namespace vela::core
{
    class Context;
} //namespace vela::core

namespace lockstep::render
{
    // Renderer-owned textures every PBR material and the lit pass rely on.
    // whiteSrgb:   1x1 white, sRGB; stands in for a missing base colour or emissive texture.
    // whiteLinear: 1x1 white, linear; stands in for a missing metallic-roughness or occlusion map.
    // flatNormal:  1x1 tangent-space +Z; stands in for a missing normal map.
    // brdfLut:     split-sum environment BRDF table (x: n.v, y: roughness; r scale, g bias on F0),
    //              computed on the CPU at startup.
    // surfaceDetail: the micro-detail layers, indexed by SurfaceDetail minus one (k_surfaceDetailKinds
    //              of them; see SurfaceTextures.hpp), repeating, mipmapped, anisotropic.
    // lightProfiles: the emission profile atlas (LightProfiles.hpp), repeating in u, clamped in v.
    // blackFloat:  one RGBA16F texel of zero (irradiance volume bindings while none is set).
    // lensDirt:    the lens dirt bloom lights up (see SurfaceTextures.hpp), clamped, mipmapped.
    struct EnvironmentResources
    {
        vela::graphics::Texture whiteSrgb;
        vela::graphics::Texture whiteLinear;
        vela::graphics::Texture flatNormal;
        vela::graphics::Texture brdfLut;
        std::vector<vela::graphics::Texture> surfaceDetail;
        vela::graphics::Texture lensDirt;
        vela::graphics::Texture lightProfiles;
        vela::graphics::Texture blackFloat;

        // Creates the textures (the procedural ones are generated on worker threads).
        // context: the GPU context.
        // Returns: the resources, or an error if an upload failed.
        [[nodiscard]] static Result<EnvironmentResources> create(vela::core::Context& context);
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_ENVIRONMENT_RESOURCES_HPP
