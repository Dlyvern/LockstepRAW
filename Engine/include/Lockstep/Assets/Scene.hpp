#ifndef LOCKSTEP_ASSETS_SCENE_HPP
#define LOCKSTEP_ASSETS_SCENE_HPP

#include "Lockstep/Core/Bounds.hpp"
#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Core/Transform.hpp"
#include "Lockstep/Render/PbrMaterial.hpp"
#include "Lockstep/Render/Resources.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace lockstep::assets
{
    // One drawable part of an imported scene: a glTF mesh primitive uploaded as a GPU mesh, and
    // the material it is drawn with. Several instances may share it.
    // mesh:               the GPU mesh, owned by the Renderer.
    // material:           the material, owned by the Renderer.
    // alphaMode:          the material's alpha mode (Blend parts usually want no collision).
    // localBounds:        bounds of its vertices in mesh space.
    // triangleCount:      triangles in the mesh.
    // collisionPositions: mesh-space vertex positions for collision; empty unless the import
    //                     kept them (SceneImportOptions::keepCollisionGeometry).
    // collisionIndices:   triangle list into `collisionPositions`.
    struct SceneMesh
    {
        render::MeshHandle mesh{};
        render::MaterialHandle material{};
        render::AlphaMode alphaMode{render::AlphaMode::Opaque};
        math::Aabb localBounds{};
        uint32_t triangleCount{0};
        std::vector<math::Vec3> collisionPositions;
        std::vector<uint32_t> collisionIndices;
    };

    // One placed copy of a SceneMesh.
    // mesh:        index into Scene::meshes.
    // transform:   world placement (import scale included).
    // worldBounds: the mesh's local bounds moved by `transform`, for culling.
    struct SceneInstance
    {
        uint32_t mesh{0};
        math::Transform transform{};
        math::Aabb worldBounds{};
    };

    // A named glTF node, kept so Game can find markers such as a player start.
    // name:      the node's name as authored.
    // transform: world placement (import scale included).
    struct SceneNode
    {
        std::string name;
        math::Transform transform{};
    };

    // What an import produced and how long it took.
    // loadSeconds:       wall time of importGltf(), parsing to last upload.
    // meshCount:         SceneMesh count (glTF primitives uploaded).
    // instanceCount:     SceneInstance count.
    // materialCount:     materials created.
    // textureCount:      textures created (after deduplication).
    // triangleCount:     triangles over all instances, what one full draw of the scene costs.
    // textureBytes:      GPU memory of all texture data, mip levels included.
    // compressedTextures: textures stored block compressed (Bc1/Bc3/Bc7).
    struct SceneStats
    {
        double loadSeconds{0.0};
        uint32_t meshCount{0};
        uint32_t instanceCount{0};
        uint32_t materialCount{0};
        uint32_t textureCount{0};
        uint64_t triangleCount{0};
        uint64_t textureBytes{0};
        uint32_t compressedTextures{0};
    };

    // An imported glTF scene: GPU resources (owned by the Renderer; the scene holds handles) and
    // where to draw them. Plain data; copying it copies handles, not resources.
    // meshes:    drawable parts.
    // instances: placed copies of the parts, in node order.
    // nodes:     every named node with its world transform.
    // bounds:    world bounds of all instances.
    // warnings:  glTF features the import skipped or approximated, one line each, deduplicated.
    // stats:     counts, timing and memory.
    struct Scene
    {
        std::vector<SceneMesh> meshes;
        std::vector<SceneInstance> instances;
        std::vector<SceneNode> nodes;
        math::Aabb bounds{};
        std::vector<std::string> warnings;
        SceneStats stats{};
    };

    // Settings for importGltf().
    // scale:                 uniform scale applied to the whole scene around the origin.
    // keepCollisionGeometry: keep mesh-space positions and indices in each SceneMesh so
    //                        createSceneCollision() can build colliders from them.
    // compressTextures:      store single-level colour textures (base colour, emissive; jpg, png
    //                        or KTX2) as Bc1, or Bc3 when they have alpha, with mips built on the
    //                        CPU. Data textures (normal, metallic-roughness, occlusion), and colour
    //                        ones without this option or Bc support, stay Rgba8 and the GPU builds
    //                        their mips: Bc1/Bc3 band normal maps. Basis KTX2 with a mip chain
    //                        becomes Bc7 whenever the GPU has Bc support.
    // workerThreads:         threads decoding textures; 0 picks the hardware thread count.
    struct SceneImportOptions
    {
        float scale{1.0f};
        bool keepCollisionGeometry{true};
        bool compressTextures{true};
        uint32_t workerThreads{0};
    };

    // Imports a glTF 2.0 file (.gltf with external or embedded buffers, or .glb) and uploads it.
    // Every triangle primitive of every mesh in the default scene's node tree becomes a SceneMesh
    // with a PbrMaterialDescription material; missing normals become flat normals and missing
    // tangents are generated. Textures (jpg, png, KTX2 including Basis Universal) are decoded on
    // worker threads, deduplicated per image and colour space, and get mips. Unsupported optional
    // features are skipped and listed in Scene::warnings.
    // renderer: the renderer that uploads and owns the resources.
    // path:     the .gltf or .glb file.
    // options:  scale, collision geometry and texture settings.
    // Returns:  the scene, or an error if the file can't be read or parsed, needs an unsupported
    //           extension, has no triangles, or an upload failed.
    [[nodiscard]] Result<Scene> importGltf(render::Renderer& renderer, const std::filesystem::path& path,
        const SceneImportOptions& options = {});

    // Finds a named node. Names compare case-insensitively, and a name also matches a node whose
    // name starts with it followed by '.' or '_' (Blender appends ".001").
    // scene:   the scene.
    // name:    the name to look for, e.g. "PlayerStart".
    // Returns: the first matching node's world transform, or nothing.
    [[nodiscard]] std::optional<math::Transform> findNode(const Scene& scene, std::string_view name);

    // Submits every instance of the scene for the next render().
    // renderer: the renderer the scene was imported with.
    // scene:    the scene.
    void submitScene(render::Renderer& renderer, const Scene& scene);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_SCENE_HPP
