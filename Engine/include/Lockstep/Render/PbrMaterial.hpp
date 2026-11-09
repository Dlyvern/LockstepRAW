#ifndef LOCKSTEP_RENDER_PBR_MATERIAL_HPP
#define LOCKSTEP_RENDER_PBR_MATERIAL_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Render/Resources.hpp"

#include <cstdint>

// The contract between asset import (Engine/Assets) and rendering (Engine/Render): glTF 2.0's
// metallic-roughness material model and a vertex layout with tangents. Assets fills these;
// the Renderer implements them (Renderer::createMesh / createMaterial overloads taking these
// types). Field meanings follow the glTF 2.0 specification exactly, so an imported material
// looks the same as in Blender or any glTF viewer.
namespace lockstep::render
{
    // One vertex of a PBR mesh.
    // position: object-space position.
    // normal:   object-space unit normal.
    // uv:       texture coordinate (glTF TEXCOORD_0); (0,0) is the top-left of the image.
    // tangent:  object-space unit tangent in xyz; w is +1 or -1, the bitangent's handedness
    //           (bitangent = cross(normal, tangent.xyz) * w), as in glTF TANGENT.
    struct PbrVertex
    {
        float position[3];
        float normal[3];
        float uv[2];
        float tangent[4];
    };

    // How a material's alpha is used (glTF alphaMode).
    // Opaque: alpha is ignored.
    // Mask:   pixels with alpha below `alphaCutoff` are discarded; the rest are opaque (foliage).
    // Blend:  alpha-blended over what is behind it, drawn after opaque geometry.
    enum class AlphaMode : uint8_t
    {
        Opaque = 0,
        Mask,
        Blend
    };

    // A glTF 2.0 metallic-roughness material. Any texture may be left invalid; the factor alone
    // is used then. Textures are sampled with TEXCOORD_0.
    // baseColor / baseColorFactor:   albedo; the texture is sRGB, multiplied by the linear factor.
    //                                Alpha comes from the texture's A times the factor's w.
    // normal / normalScale:          tangent-space normal map (linear); xy are scaled by normalScale.
    // metallicRoughness:             linear texture; G is roughness, B is metallic.
    // metallicFactor / roughnessFactor: multiplied with the texture's B / G channel.
    // occlusion / occlusionStrength: linear texture, ambient occlusion in R; 0 disables, 1 full.
    // emissive / emissiveFactor:     the texture is sRGB, multiplied by the linear factor
    //                                (emissive strength already folded into the factor).
    // alphaMode / alphaCutoff:       see AlphaMode; the cutoff only matters for Mask.
    // doubleSided:                   true disables back-face culling and lights back faces.
    struct PbrMaterialDescription
    {
        TextureHandle baseColor{};
        math::Vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};

        TextureHandle normal{};
        float normalScale{1.0f};

        TextureHandle metallicRoughness{};
        float metallicFactor{1.0f};
        float roughnessFactor{1.0f};

        TextureHandle occlusion{};
        float occlusionStrength{1.0f};

        TextureHandle emissive{};
        math::Vec3 emissiveFactor{0.0f, 0.0f, 0.0f};

        AlphaMode alphaMode{AlphaMode::Opaque};
        float alphaCutoff{0.5f};
        bool doubleSided{false};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_PBR_MATERIAL_HPP
