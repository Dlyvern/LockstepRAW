#ifndef LOCKSTEP_ASSETS_COOKED_FORMAT_HPP
#define LOCKSTEP_ASSETS_COOKED_FORMAT_HPP

#include "Lockstep/Anim/AnimationClip.hpp"
#include "Lockstep/Anim/Skeleton.hpp"
#include "Lockstep/Assets/Scene.hpp"
#include "Lockstep/Core/Bounds.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/PbrMaterial.hpp"
#include "Lockstep/Render/Resources.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The cooked file formats, shared by the Engine's loader (loadCookedScene) and LockstepCooker's
// writer. Every file starts with a 4-byte magic and k_cookedVersion; a loader rejects other
// versions, so a format change means bumping the version and re-cooking. Little-endian.
//
//   mesh (.mesh):      CookedMeshHeader, PbrVertex[vertexCount], uint32 index[indexCount]
//   skinned mesh:      the same with k_skinnedMeshMagic and SkinnedPbrVertex vertices
//   model (.model):    written by writeCookedModel(): a skinned model's skeleton, clips, bounds
//                      and parts (skinned mesh paths and level material indices)
//   texture (.tex):    CookedTextureHeader, every mip level largest first, tightly packed in
//                      the final GPU format (render::TextureDescription layout)
//   collision (.col):  CookedCollisionHeader, then PhysX's serialized triangle mesh
//   level (scene):     written by writeCookedLevel(): the materials, meshes, instances, nodes and
//                      lights of one scene, and the virtual paths of its other files; versioned
//                      by k_levelVersion
namespace lockstep::assets
{
    inline constexpr uint32_t k_cookedVersion{1};

    // Version of level files, separate from k_cookedVersion so mesh, texture and collision files
    // stay valid when only the level layout changes. Version 2 added SceneNode::parent and
    // SceneInstance::node; version 3 added skinned models and their instances. Older levels
    // still load, without what came later.
    inline constexpr uint32_t k_levelVersion{3};
    inline constexpr uint32_t k_levelVersionWithoutSkins{2};
    inline constexpr uint32_t k_levelVersionWithoutHierarchy{1};
    inline constexpr std::array<char, 4> k_meshMagic{'L', 'M', 'S', 'H'};
    inline constexpr std::array<char, 4> k_textureMagic{'L', 'T', 'E', 'X'};
    inline constexpr std::array<char, 4> k_collisionMagic{'L', 'C', 'O', 'L'};
    inline constexpr std::array<char, 4> k_levelMagic{'L', 'L', 'V', 'L'};
    inline constexpr std::array<char, 4> k_skinnedMeshMagic{'L', 'S', 'K', 'M'};
    inline constexpr std::array<char, 4> k_modelMagic{'L', 'M', 'D', 'L'};

    // CookedMaterial texture slot without a texture.
    inline constexpr uint32_t k_noTexture{0xFFFFFFFFu};

    // Header of a .mesh file (static or skinned, told apart by the magic).
    // bounds: bounds of the vertex positions, as min xyz then max xyz.
    struct CookedMeshHeader
    {
        std::array<char, 4> magic{k_meshMagic};
        uint32_t version{k_cookedVersion};
        uint32_t vertexCount{0};
        uint32_t indexCount{0};
        std::array<float, 6> bounds{};
    };

    // Header of a .tex file.
    // format: a render::TextureFormat value.
    struct CookedTextureHeader
    {
        std::array<char, 4> magic{k_textureMagic};
        uint32_t version{k_cookedVersion};
        uint32_t format{0};
        uint32_t width{0};
        uint32_t height{0};
        uint32_t levels{0};
    };

    // Header of a .col file.
    // triangleCount: triangles in the cooked mesh.
    struct CookedCollisionHeader
    {
        std::array<char, 4> magic{k_collisionMagic};
        uint32_t version{k_cookedVersion};
        uint32_t triangleCount{0};
        uint32_t reserved{0};
    };

    static_assert(sizeof(CookedMeshHeader) == 40);
    static_assert(sizeof(CookedTextureHeader) == 24);
    static_assert(sizeof(CookedCollisionHeader) == 16);

    // A material of a cooked level: the PBR factors, and texture slots as indices into
    // CookedLevel::textures (k_noTexture for none).
    // factors: everything but the texture handles, which are ignored.
    struct CookedMaterial
    {
        render::PbrMaterialDescription factors{};
        uint32_t baseColor{k_noTexture};
        uint32_t normal{k_noTexture};
        uint32_t metallicRoughness{k_noTexture};
        uint32_t occlusion{k_noTexture};
        uint32_t emissive{k_noTexture};
    };

    // A mesh of a cooked level.
    // path:           virtual path of its .mesh file.
    // material:       index into CookedLevel::materials.
    // alphaMode:      the material's alpha mode.
    // localBounds:    bounds in mesh space.
    // triangleCount:  triangles drawn.
    // collisionShape: index into CookedLevel::collisionShapes, or k_noCollisionShape.
    struct CookedMesh
    {
        std::string path;
        uint32_t material{0};
        render::AlphaMode alphaMode{render::AlphaMode::Opaque};
        math::Aabb localBounds{};
        uint32_t triangleCount{0};
        uint32_t collisionShape{k_noCollisionShape};
    };

    // One part of a cooked skinned model.
    // path:         virtual path of its skinned .mesh file.
    // material:     index into CookedLevel::materials (created as a skinned material).
    // materialName: the source material's name. Stored after the parts in a trailing section of
    //               the .model file (files cooked without it load with empty names).
    struct CookedSkinnedPart
    {
        std::string path;
        uint32_t material{0};
        std::string materialName;
    };

    // The contents of a .model file: an anim::SkinnedModel without GPU handles.
    struct CookedModel
    {
        std::string name;
        anim::Skeleton skeleton;
        std::vector<anim::AnimationClip> clips;
        std::vector<float> jointRadii;
        math::Aabb bindBounds{};
        std::vector<CookedSkinnedPart> parts;
    };

    // The contents of a level file.
    // textures:        virtual paths of the .tex files.
    // materials:       materials.
    // meshes:          meshes.
    // collisionShapes: virtual paths of the .col files.
    // instances:       placed meshes (SceneInstance::mesh indexes `meshes`).
    // nodes:           named nodes, markers included.
    // lights:          authored lights.
    // bounds:          world bounds of all instances.
    // models:          virtual paths of the skinned models' .model files.
    // skinnedInstances: placed skinned models (SceneSkinnedInstance::model indexes `models`).
    struct CookedLevel
    {
        std::vector<std::string> textures;
        std::vector<CookedMaterial> materials;
        std::vector<CookedMesh> meshes;
        std::vector<std::string> collisionShapes;
        std::vector<SceneInstance> instances;
        std::vector<SceneNode> nodes;
        std::vector<SceneLight> lights;
        math::Aabb bounds{};
        std::vector<std::string> models;
        std::vector<SceneSkinnedInstance> skinnedInstances;
    };

    // A skinned .mesh file's contents.
    struct CookedSkinnedMeshData
    {
        std::vector<render::SkinnedPbrVertex> vertices;
        std::vector<uint32_t> indices;
        math::Aabb bounds{};
    };

    // A .mesh file's contents.
    struct CookedMeshData
    {
        std::vector<render::PbrVertex> vertices;
        std::vector<uint32_t> indices;
        math::Aabb bounds{};
    };

    // A .tex file's header and where its level data starts.
    // header: the header, validated.
    // pixels: every mip level, as render::TextureDescription::pixels wants them.
    struct CookedTextureView
    {
        CookedTextureHeader header{};
        std::span<const std::byte> pixels;
    };

    // Serializes a level field by field (no padding bytes, so equal levels give equal bytes).
    // level:   the level.
    // Returns: the file contents.
    [[nodiscard]] std::vector<std::byte> writeCookedLevel(const CookedLevel& level);

    // Parses a level file.
    // bytes:   the file contents.
    // path:    the file's virtual path, for messages.
    // Returns: the level, or AssetLoadFailed for another format, another version or damaged data
    //          (indices out of range included).
    [[nodiscard]] Result<CookedLevel> readCookedLevel(std::span<const std::byte> bytes, std::string_view path);

    // vertices: mesh vertices.
    // indices:  triangle list.
    // bounds:   bounds of the positions.
    // Returns:  the .mesh file contents.
    [[nodiscard]] std::vector<std::byte> writeCookedMesh(std::span<const render::PbrVertex> vertices,
        std::span<const uint32_t> indices, const math::Aabb& bounds);

    // Parses a .mesh file.
    // bytes:   the file contents.
    // path:    the file's virtual path, for messages.
    // Returns: the mesh, or AssetLoadFailed for another format or version, a wrong size or an
    //          index out of range.
    [[nodiscard]] Result<CookedMeshData> readCookedMesh(std::span<const std::byte> bytes, std::string_view path);

    // vertices: skinned mesh vertices.
    // indices:  triangle list.
    // bounds:   bounds of the bind-pose positions.
    // Returns:  the skinned .mesh file contents.
    [[nodiscard]] std::vector<std::byte> writeCookedSkinnedMesh(std::span<const render::SkinnedPbrVertex> vertices,
        std::span<const uint32_t> indices, const math::Aabb& bounds);

    // Parses a skinned .mesh file.
    // bytes:   the file contents.
    // path:    the file's virtual path, for messages.
    // Returns: the mesh, or AssetLoadFailed for another format or version, a wrong size, an index
    //          out of range or a joint index of k_maxJoints or more.
    [[nodiscard]] Result<CookedSkinnedMeshData> readCookedSkinnedMesh(std::span<const std::byte> bytes,
        std::string_view path);

    // Serializes a skinned model field by field (equal models give equal bytes).
    // model:   the model.
    // Returns: the .model file contents.
    [[nodiscard]] std::vector<std::byte> writeCookedModel(const CookedModel& model);

    // Parses a .model file.
    // bytes:   the file contents.
    // path:    the file's virtual path, for messages.
    // Returns: the model, or AssetLoadFailed for another format or version or damaged data (a
    //          joint parent not before its child, a channel out of range or with mismatched keys).
    [[nodiscard]] Result<CookedModel> readCookedModel(std::span<const std::byte> bytes, std::string_view path);

    // header:  format, size and level count (magic and version are set here).
    // pixels:  every level, tightly packed.
    // Returns: the .tex file contents.
    [[nodiscard]] std::vector<std::byte> writeCookedTexture(CookedTextureHeader header,
        std::span<const std::byte> pixels);

    // Parses a .tex file without copying its pixels.
    // bytes:   the file contents; the view points into them.
    // path:    the file's virtual path, for messages.
    // Returns: the view, or AssetLoadFailed for another format or version, an unknown pixel
    //          format, too many levels or a size that doesn't match.
    [[nodiscard]] Result<CookedTextureView> readCookedTexture(std::span<const std::byte> bytes, std::string_view path);

    // triangleCount: triangles in the mesh.
    // cooked:        PhysX's serialized triangle mesh.
    // Returns:       the .col file contents.
    [[nodiscard]] std::vector<std::byte> writeCookedCollision(uint32_t triangleCount, std::span<const std::byte> cooked);

    // Strips a .col file's header.
    // bytes:   the file contents.
    // path:    the file's virtual path, for messages.
    // Returns: PhysX's serialized mesh, or AssetLoadFailed for another format or version.
    [[nodiscard]] Result<std::vector<std::byte>> readCookedCollision(std::vector<std::byte> bytes, std::string_view path);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_COOKED_FORMAT_HPP
