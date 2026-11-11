#ifndef LOCKSTEP_RENDER_PBR_MATERIAL_HPP
#define LOCKSTEP_RENDER_PBR_MATERIAL_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Render/Resources.hpp"

#include <cstdint>

// The contract between asset import (Engine/Assets) and rendering (Engine/Render): glTF 2.0's
// metallic-roughness material model and a vertex layout with tangents. Assets fills these;
// the Renderer implements them (Renderer::createMesh / createMaterial overloads taking these
// types). Field meanings follow the glTF 2.0 specification exactly, so an imported material
// looks the same as in Blender or any glTF viewer. Two Engine extensions glTF can't express sit on
// top (a height map for parallax and a micro-detail layer); only cooked levels fill them, and their
// defaults leave a material exactly as glTF describes it.
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

    // One vertex of a skinned PBR mesh: a PbrVertex plus up to four joint influences (glTF
    // JOINTS_0 / WEIGHTS_0).
    // position, normal, uv, tangent: as in PbrVertex, in bind pose.
    // joints:  indices into the skeleton's joints, stored as floats (exact below 2^24).
    // weights: influence of each joint; they sum to 1.
    struct SkinnedPbrVertex
    {
        float position[3];
        float normal[3];
        float uv[2];
        float tangent[4];
        float joints[4];
        float weights[4];
    };

    // Which vertex layout a mesh, and the materials drawing it, use.
    // Static:  PbrVertex; drawn with Renderer::submit().
    // Skinned: SkinnedPbrVertex; drawn with Renderer::submitSkinned() and a joint palette.
    enum class MeshKind : uint8_t
    {
        Static = 0,
        Skinned
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

    // A generic tiling micro-detail layer the Renderer provides (procedural textures, about half a
    // metre per repeat), blended over a lit material near the camera so close surfaces keep fine
    // structure instead of going blurry: normal, roughness and slight albedo variation.
    // None:    no detail layer.
    // Mineral: grain and small pits (concrete, plaster, brick, stone, asphalt).
    // Paint:   fine orange peel and light scuffs (painted surfaces, plastic, linoleum).
    // Metal:   brushed streaks and scratches.
    // Fibrous: weave and fibre grain (fabric, wood, cardboard).
    enum class SurfaceDetail : uint8_t
    {
        None = 0,
        Mineral,
        Paint,
        Metal,
        Fibrous
    };

    // Number of SurfaceDetail layers besides None.
    inline constexpr uint32_t k_surfaceDetailKinds{4};

    // How a lit material scatters light, on top of the metallic-roughness model (an Engine
    // extension; Game picks it per material, e.g. by material or node name).
    // Standard: plain metallic-roughness.
    // Skin:     subsurface scattering approximated per light: light wraps past the terminator and
    //           shadows bleed red, thin silhouettes glow when back-lit. Applies only to pixels whose
    //           base colour looks like skin (warm, bright, clearly redder than green), so gear, hair
    //           and eyes sharing a face texture stay standard.
    // Fabric:   an added sheen lobe (Charlie distribution): soft rim highlights of cloth fibres at
    //           grazing angles (uniforms, curtains, sofas, mattresses).
    // Screen:   an emissive display (monitor, TV, laptop, vending machine panel): the emission gets
    //           scanlines, a slow rolling band and a faint shimmer, and may scroll
    //           (SurfaceEffectSettings::screens). Dim or flicker it per instance with the draw's
    //           emissive scale.
    enum class ShadingModel : uint8_t
    {
        Standard = 0,
        Skin,
        Fabric,
        Screen
    };

    // A material's shading model and its parameters (see ShadingModel).
    // model:          the shading model.
    // strength:       skin: scatter amount (1 typical, 0 to 2); fabric: sheen intensity (1 typical,
    //                 0 to 2).
    // sheenRoughness: fabric: width of the sheen lobe, 0.2 (tight satin rim) to 1 (fuzzy cotton).
    //                 screen: `strength` is how strongly the screen effects show (0 to 2).
    // scroll:         screen: how fast the emissive image scrolls up, in UV units per second (0 for
    //                 still content; the screen's UVs should span the whole texture).
    // terminal:       screen: show an Engine-generated terminal image (rows of dim text, a title
    //                 bar, a blinking cursor) tinted by the emissive texture, for screens whose
    //                 texture is plain.
    // decalRelief:    alpha-blended decals drawn with Renderer::submitDecal(): height of the relief
    //                 the base colour alpha describes, in UV units (positive raised, e.g. 0.01 for a
    //                 blood film's rim; negative dented, e.g. -0.03 for a bullet hole). The relief
    //                 shades the decal and darkens the uncovered surface below where it faces away
    //                 from the lights, and its normal and roughness reach the G-buffer. 0 is off.
    struct MaterialShading
    {
        ShadingModel model{ShadingModel::Standard};
        float strength{1.0f};
        float sheenRoughness{0.6f};
        float decalRelief{0.0f};
        float scroll{0.0f};
        bool terminal{false};
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
    // Engine extensions (not glTF):
    // height / heightScale:          linear height map, R from 0 (deepest) to 1 (top), for parallax
    //                                occlusion mapping; heightScale is the depth from 1 to 0 in UV units
    //                                (1 is one texture repeat); 0 or no texture disables parallax.
    // detail / detailScale:          the micro-detail layer and how often it repeats per UV unit.
    // shading:                       the shading model (Standard by default; see MaterialShading).
    //                                Not cooked; Game sets it (Renderer::setMaterialShading()).
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

        TextureHandle height{};
        float heightScale{0.0f};

        SurfaceDetail detail{SurfaceDetail::None};
        float detailScale{1.0f};

        MaterialShading shading{};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_PBR_MATERIAL_HPP
