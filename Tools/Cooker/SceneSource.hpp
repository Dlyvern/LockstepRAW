#ifndef LOCKSTEP_COOKER_SCENE_SOURCE_HPP
#define LOCKSTEP_COOKER_SCENE_SOURCE_HPP

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Core/Result.hpp>

#include "Assets/CookedFormat.hpp"
#include "Assets/GltfDocument.hpp"
#include "Assets/GltfMeshes.hpp"
#include "Assets/GltfSkins.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::cooker
{
    // What a texture holds, which decides its cooked format.
    // Colour:    base colour or emissive: sRGB, Bc7Srgb.
    // Normal:    tangent-space normal map: xy in Bc5Unorm, z reconstructed by the shader.
    // Data:      metallic-roughness (occlusion too when it shares the image): Bc7Unorm.
    // Occlusion: occlusion in its own image (R only): Bc4Unorm.
    // Height:    height map for parallax (R only, stretched to the full 0-1 range): Bc4Unorm.
    enum class TextureUsage : uint8_t
    {
        Colour = 0,
        Normal,
        Data,
        Occlusion,
        Height
    };

    // One texture to cook: a glTF image, or an image file beside the source, read for one usage.
    // image:       the glTF image, or null when `file` is set.
    // file:        an image file on disk (height maps found by applySurfaceRules()); empty for glTF images.
    // alphaCutoff: cutoff of the alpha-masked material using it as base colour, or negative when no
    //              masked material does (its mips then keep their alpha coverage, see cookTexture()).
    struct SourceTexture
    {
        const cgltf_image* image{nullptr};
        std::filesystem::path file{};
        TextureUsage usage{TextureUsage::Colour};
        float alphaCutoff{-1.0f};
    };

    // One mesh to cook: a glTF primitive's geometry and its material.
    // geometry:  PBR vertices, indices and bounds.
    // material:  index into SourceScene::materials.
    // alphaMode: the material's alpha mode.
    struct SourceMesh
    {
        assets::PrimitiveGeometry geometry;
        uint32_t material{0};
        render::AlphaMode alphaMode{render::AlphaMode::Opaque};
    };

    // One skinned model to cook: a glTF skin with its primitives, and their materials.
    // model:     skeleton, clips, bounds and skinned geometry.
    // materials: per part, index into SourceScene::materials.
    struct SourceSkinnedModel
    {
        assets::GltfSkinnedModel model;
        std::vector<uint32_t> materials;
    };

    // A glTF scene read into CPU memory, laid out like a cooked level: every mesh, material and
    // texture in first-use order, so the same file always gives the same order.
    // document:  the parsed glTF (its images are read later).
    // directory: the virtual folder of the document, for image URIs.
    // textures:  textures to cook.
    // materials: materials, their texture slots indexing `textures`.
    // materialNames: the source name of each material (empty when unnamed).
    // meshes:    meshes to cook.
    // instances: placed meshes.
    // nodes:     named nodes (markers included).
    // lights:    KHR_lights_punctual lights.
    // skinnedModels: skinned models, one per glTF skin; each is placed once (SceneSkinnedInstance
    //            with the model's transform and node).
    // bounds:    world bounds of all instances, skinned ones in bind pose included.
    // warnings:  what the import skipped or approximated.
    struct SourceScene
    {
        assets::GltfDocument document;
        std::string directory;
        std::vector<SourceTexture> textures;
        std::vector<assets::CookedMaterial> materials;
        std::vector<std::string> materialNames;
        std::vector<SourceMesh> meshes;
        std::vector<assets::SceneInstance> instances;
        std::vector<assets::SceneNode> nodes;
        std::vector<assets::SceneLight> lights;
        std::vector<SourceSkinnedModel> skinnedModels;
        math::Aabb bounds{};
        std::vector<std::string> warnings;
    };

    // Reads a glTF file's node tree, meshes and materials, the same way importGltf() does.
    // files: the file system holding the source.
    // path:  virtual path of the .gltf or .glb file.
    // scale: uniform scale applied around the origin.
    // Returns: the scene, or an error if the file can't be read or has no drawable triangles
    //          (static or skinned).
    [[nodiscard]] Result<SourceScene> readSourceScene(const assets::FileSystem& files, std::string_view path, float scale);
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_SCENE_SOURCE_HPP
