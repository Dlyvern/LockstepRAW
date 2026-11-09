#ifndef LOCKSTEP_ASSETS_SCENE_HPP
#define LOCKSTEP_ASSETS_SCENE_HPP

#include "Lockstep/Anim/SkinnedModel.hpp"
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
    class FileSystem;

    // SceneMesh::collisionShape of a mesh without a cooked collision shape.
    inline constexpr uint32_t k_noCollisionShape{0xFFFFFFFFu};

    // SceneNode::parent and SceneInstance::node when there is no such node.
    inline constexpr uint32_t k_noNode{0xFFFFFFFFu};

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
    // collisionShape:     index into Scene::collisionShapes for a cooked scene, else
    //                     k_noCollisionShape. Identical meshes share one shape.
    struct SceneMesh
    {
        render::MeshHandle mesh{};
        render::MaterialHandle material{};
        render::AlphaMode alphaMode{render::AlphaMode::Opaque};
        math::Aabb localBounds{};
        uint32_t triangleCount{0};
        std::vector<math::Vec3> collisionPositions;
        std::vector<uint32_t> collisionIndices;
        uint32_t collisionShape{k_noCollisionShape};
    };

    // One placed copy of a SceneMesh.
    // mesh:        index into Scene::meshes.
    // transform:   world placement (import scale included). Game may change it at runtime
    //              (setInstanceTransform() in SceneCollision.hpp also moves the collider).
    // worldBounds: the mesh's local bounds moved by `transform`, for culling.
    // node:        index into Scene::nodes of the glTF node holding the mesh, or of its nearest
    //              named ancestor when that node has no name; k_noNode if there is none (also for
    //              levels cooked before the format kept the hierarchy).
    // isVisible:   submitScene() skips the instance while false. Not stored; starts true.
    struct SceneInstance
    {
        uint32_t mesh{0};
        math::Transform transform{};
        math::Aabb worldBounds{};
        uint32_t node{k_noNode};
        bool isVisible{true};
    };

    // A named glTF node, kept so Game can find markers such as a player start, and objects it
    // moves or hides as a whole (the instances under a node, see isNodeInSubtree()).
    // name:      the node's name as authored.
    // transform: world placement (import scale included).
    // parent:    index into Scene::nodes of the nearest named ancestor, or k_noNode for none.
    //            Unnamed glTF nodes in between are skipped, so this is always an earlier node.
    struct SceneNode
    {
        std::string name;
        math::Transform transform{};
        uint32_t parent{k_noNode};
    };

    // One placed skinned model. Game animates it with an anim::SkinnedInstance and submits it
    // with that instance's submit(); submitScene() does not draw it.
    // model:     index into Scene::skinnedModels.
    // transform: world placement of the model's space (the import scale for a glTF scene).
    // node:      index into Scene::nodes of the first mesh node using the skin, or of its nearest
    //            named ancestor; k_noNode if there is none.
    struct SceneSkinnedInstance
    {
        uint32_t model{0};
        math::Transform transform{};
        uint32_t node{k_noNode};
    };

    // Kind of a SceneLight (glTF KHR_lights_punctual).
    enum class SceneLightType : uint8_t
    {
        Directional = 0,
        Point,
        Spot
    };

    // A light authored in the source file (KHR_lights_punctual), with glTF's units: point and
    // spot intensity in candela, directional in lux. SceneLights.hpp converts them into render
    // lights; Game decides which ones to use.
    // type:       directional, point or spot.
    // name:       the light's name, or its node's name.
    // color:      linear RGB.
    // intensity:  see above.
    // range:      distance where the light ends, 0 for unlimited.
    // innerAngle: spot only: half-angle of the full-intensity cone, in radians.
    // outerAngle: spot only: half-angle where it fades out, in radians.
    // transform:  world placement; the light shines along the transform's local -Z.
    struct SceneLight
    {
        SceneLightType type{SceneLightType::Point};
        std::string name;
        math::Vec3 color{1.0f, 1.0f, 1.0f};
        float intensity{1.0f};
        float range{0.0f};
        float innerAngle{0.0f};
        float outerAngle{0.785398f};
        math::Transform transform{};
    };

    // What an import produced and how long it took.
    // loadSeconds:       wall time of importGltf() or loadCookedScene(), first read to last upload.
    // meshCount:         SceneMesh count (glTF primitives uploaded).
    // instanceCount:     SceneInstance count.
    // materialCount:     materials created.
    // textureCount:      textures created (after deduplication).
    // triangleCount:     triangles over all instances, what one full draw of the scene costs.
    // textureBytes:      GPU memory of all texture data, mip levels included.
    // compressedTextures: textures stored block compressed (any Bc format).
    // contentBytes:      bytes read through the file system (cooked scenes only).
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
        uint64_t contentBytes{0};
    };

    // Every GPU resource a scene load created, each listed once, so releaseScene() can destroy
    // them. Shared ones (a material used by several meshes) appear once.
    // meshes:    static and skinned meshes.
    // materials: static and skinned materials.
    // textures:  textures.
    struct SceneResources
    {
        std::vector<render::MeshHandle> meshes;
        std::vector<render::MaterialHandle> materials;
        std::vector<render::TextureHandle> textures;
    };

    // A loaded scene (an imported glTF or a cooked level): GPU resources (owned by the Renderer;
    // the scene holds handles) and where to draw them. Plain data; copying it copies handles, not
    // resources. Free the resources with releaseScene() (each scene once, not each copy).
    // meshes:          drawable parts.
    // instances:       placed copies of the parts, in node order.
    // nodes:           every named node with its world transform (markers such as
    //                  "PlayerStart" or "Interact_Door" included).
    // lights:          lights authored in the source.
    // skinnedModels:   skinned models (one per glTF skin) with their skeletons and clips.
    // skinnedInstances: where the skinned models are placed.
    // collisionShapes: PhysX triangle meshes cooked by LockstepCooker, referenced by
    //                  SceneMesh::collisionShape; empty for an imported glTF.
    // bounds:          world bounds of all instances.
    // warnings:        source features the import skipped or approximated, one line each,
    //                  deduplicated.
    // stats:           counts, timing and memory.
    // resources:       the GPU resources the load created, for releaseScene().
    struct Scene
    {
        std::vector<SceneMesh> meshes;
        std::vector<SceneInstance> instances;
        std::vector<SceneNode> nodes;
        std::vector<SceneLight> lights;
        std::vector<anim::SkinnedModel> skinnedModels;
        std::vector<SceneSkinnedInstance> skinnedInstances;
        std::vector<std::vector<std::byte>> collisionShapes;
        math::Aabb bounds{};
        std::vector<std::string> warnings;
        SceneStats stats{};
        SceneResources resources;
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
    // with a PbrMaterialDescription material, except meshes of skinned nodes: each glTF skin
    // becomes a SkinnedModel (skeleton, skinned meshes and materials, the animations of its
    // joints) with a SceneSkinnedInstance; missing normals become flat normals and missing
    // tangents are generated. Textures (jpg, png, KTX2 including Basis Universal) are decoded on
    // worker threads, deduplicated per image and colour space, and get mips. Unsupported optional
    // features are skipped and listed in Scene::warnings.
    // Everything, buffers and images included, is read through the file system.
    // renderer: the renderer that uploads and owns the resources.
    // files:    the file system to read from.
    // path:     virtual path of the .gltf or .glb file.
    // options:  scale, collision geometry and texture settings.
    // Returns:  the scene, or an error if the file can't be read or parsed, needs an unsupported
    //           extension, has no triangles (static or skinned), or an upload failed.
    [[nodiscard]] Result<Scene> importGltf(render::Renderer& renderer, const FileSystem& files, std::string_view path,
        const SceneImportOptions& options = {});

    // Imports a glTF file from disk: the importGltf() above, over a file system that mounts only
    // the file's folder. The direct dev path for files that were not cooked.
    // renderer: the renderer that uploads and owns the resources.
    // path:     the .gltf or .glb file.
    // options:  scale, collision geometry and texture settings.
    // Returns:  the scene, or an error as above.
    [[nodiscard]] Result<Scene> importGltf(render::Renderer& renderer, const std::filesystem::path& path,
        const SceneImportOptions& options = {});

    // Settings for loadCookedScene().
    // workerThreads: threads reading and decompressing meshes and textures; 0 picks the hardware
    //                thread count.
    struct CookedSceneOptions
    {
        uint32_t workerThreads{0};
    };

    // Loads a scene cooked by LockstepCooker through the file system: reads the level file, its
    // meshes (PbrVertex layout), textures (final GPU format with every mip) and cooked collision
    // shapes, and uploads them as stored. Nothing is decoded, transcoded, mipmapped or cooked.
    // renderer: the renderer that uploads and owns the resources.
    // files:    the file system holding the cooked files.
    // path:     virtual path of the level file, e.g. "levels/sponza/scene".
    // options:  worker count.
    // Returns:  the scene (its meshes keep no collision positions; createSceneCollision() uses
    //           the cooked shapes), or AssetLoadFailed if a file is missing, damaged or of another
    //           format version, InvalidArgument if a texture needs Bc support the GPU lacks, or an
    //           upload error.
    [[nodiscard]] Result<Scene> loadCookedScene(render::Renderer& renderer, const FileSystem& files,
        std::string_view path, const CookedSceneOptions& options = {});

    // Finds a named node. Names compare case-insensitively, and a name also matches a node whose
    // name starts with it followed by '.' or '_' (Blender appends ".001").
    // scene:   the scene.
    // name:    the name to look for, e.g. "PlayerStart".
    // Returns: the first matching node's world transform, or nothing.
    [[nodiscard]] std::optional<math::Transform> findNode(const Scene& scene, std::string_view name);

    // Finds a named node like findNode(), by its index.
    // scene:   the scene.
    // name:    the name to look for, matched as in findNode().
    // Returns: the index into Scene::nodes of the first matching node, or nothing.
    [[nodiscard]] std::optional<uint32_t> findNodeIndex(const Scene& scene, std::string_view name);

    // Tells whether a node is `root` or one of its descendants.
    // scene:   the scene.
    // node:    index into Scene::nodes, or k_noNode (never in a subtree).
    // root:    index into Scene::nodes.
    // Returns: true if walking up from `node` through SceneNode::parent reaches `root`.
    [[nodiscard]] bool isNodeInSubtree(const Scene& scene, uint32_t node, uint32_t root);

    // Lists the instances under a node.
    // scene:   the scene.
    // root:    index into Scene::nodes.
    // Returns: indices into Scene::instances of every instance whose node is in the subtree of
    //          `root`, in order.
    [[nodiscard]] std::vector<uint32_t> subtreeInstances(const Scene& scene, uint32_t root);

    // Destroys every GPU resource the scene's load created (static and skinned meshes, materials,
    // textures; see Renderer::destroyMesh() for when the memory is handed back) and clears the
    // scene, so its handles can no longer be submitted. Copies of the scene's handles held
    // elsewhere (skinned instances, mirrors) become stale: drop them first. The static colliders
    // are separate: destroySceneCollision(), or the releaseScene() overload in SceneCollision.hpp.
    // A failed importGltf() or loadCookedScene() releases what it created by itself.
    // renderer: the renderer the scene was loaded with.
    // scene:    the scene; empty afterwards.
    void releaseScene(render::Renderer& renderer, Scene& scene);

    // Submits every visible static instance of the scene for the next render(). Skinned
    // instances are drawn by Game through anim::SkinnedInstance.
    // renderer: the renderer the scene was imported with.
    // scene:    the scene.
    void submitScene(render::Renderer& renderer, const Scene& scene);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_SCENE_HPP
