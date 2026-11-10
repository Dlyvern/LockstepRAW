#ifndef LOCKSTEP_RENDER_RENDERER_STATE_HPP
#define LOCKSTEP_RENDER_RENDERER_STATE_HPP

#include "Lockstep/Core/Bounds.hpp"
#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/Camera.hpp"
#include "Lockstep/Render/GraphicsQuality.hpp"
#include "Lockstep/Render/PbrMaterial.hpp"
#include "Lockstep/Render/PresentMode.hpp"
#include "Lockstep/Render/Renderer.hpp"
#include "Lockstep/Render/RenderSettings.hpp"
#include "Lockstep/Render/Resources.hpp"

#include "EnvironmentResources.hpp"
#include "FrameLighting.hpp"
#include "FrameParticles.hpp"
#include "PlanarReflections.hpp"
#include "ScopeView.hpp"
#include "TemporalAntiAliasing.hpp"
#include "RenderPass.hpp"
#include "RenderPassAdapter.hpp"
#include "ResourcePool.hpp"

#include <Vela/Core/Context.hpp>
#include <Vela/Core/Window.hpp>
#include <Vela/Graphics/DynamicBuffer.hpp>
#include <Vela/Graphics/Material.hpp>
#include <Vela/Graphics/Mesh.hpp>
#include <Vela/Graphics/RenderGraph.hpp>
#include <Vela/Graphics/Texture.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lockstep::render
{
    // SPIR-V of the built-in material shaders, loaded once. Kept for creating materials later.
    struct MaterialShaders
    {
        std::vector<uint32_t> unlitVertex;
        std::vector<uint32_t> unlitFragment;
        std::vector<uint32_t> pbrVertex;
        std::vector<uint32_t> pbrFragment;
        std::vector<uint32_t> pbrSkinnedVertex;
        std::vector<uint32_t> shadowVertex;
        std::vector<uint32_t> shadowFragment;
        std::vector<uint32_t> shadowMaskVertex;
        std::vector<uint32_t> shadowMaskFragment;
        std::vector<uint32_t> shadowSkinnedVertex;
        std::vector<uint32_t> shadowMaskSkinnedVertex;
    };

    // A GPU mesh and its object-space bounds (for culling).
    // kind:       PbrVertex (static) or SkinnedPbrVertex (skinned) layout.
    // jointCount: skinned only: highest joint index used plus one; palettes drawing it must hold
    //             at least that many joints.
    struct MeshRecord
    {
        vela::graphics::Mesh mesh;
        math::Aabb bounds{};
        MeshKind kind{MeshKind::Static};
        uint32_t jointCount{0};
    };

    // Per-draw material constants pushed with each PBR draw (pbr.vert / pbr.frag DrawConstants
    // after the model matrix).
    // baseColorFactor:     linear RGBA.
    // emissiveNormalScale: rgb emissive factor, w normal scale.
    // surface:             x metallic, y roughness, z occlusion strength, w alpha cutoff (-1 when
    //                      not masked).
    // flags:               x 1 if double-sided, y 1 if alpha-blended (2 for a decal draw, set when
    //                      recorded), z the PbrView, w the joint palette base.
    // detail:              x parallax height scale (0 without a height map), y the SurfaceDetail
    //                      layer (0 none), z detail repeats per UV unit, w unused.
    struct PbrFactors
    {
        float baseColorFactor[4]{1.0f, 1.0f, 1.0f, 1.0f};
        float emissiveNormalScale[4]{0.0f, 0.0f, 0.0f, 1.0f};
        float surface[4]{0.0f, 1.0f, 1.0f, -1.0f};
        float flags[4]{0.0f, 0.0f, 0.0f, 0.0f};
        float detail[4]{0.0f, 0.0f, 1.0f, 0.0f};
    };

    // A texture and how many living or retired materials bind it; a destroyed texture is
    // released only once that count is 0.
    struct TextureRecord
    {
        vela::graphics::Texture texture;
        uint32_t materialUses{0};
    };

    // A material and everything its draws need.
    // material:     the Vela material for the scene pass (pipeline and set-1 bindings).
    // maskedShadow: for alpha-masked lit materials, the shadow-pass material that discards by
    //               the base colour alpha.
    // maskedShadowBaseColor: the texture maskedShadow masks by, for rebuilding it (null when
    //               there is no maskedShadow). Owned by the RendererState.
    // textureSlots: pool indices of the Game textures it binds (defaults excluded); each counts
    //               once in TextureRecord::materialUses while the material exists.
    // shading:      lit (PBR) or unlit.
    // kind:         the mesh kind its pipelines read (skinned materials skin on the GPU).
    // alphaMode:    opaque, masked or blended; blended draws are sorted after opaque ones and
    //               cast no shadows.
    // factors:      the constants pushed with each draw (lit only).
    struct MaterialRecord
    {
        vela::graphics::Material material;
        std::optional<vela::graphics::Material> maskedShadow;
        const vela::graphics::Texture* maskedShadowBaseColor{nullptr};
        std::vector<uint32_t> textureSlots;
        Shading shading{Shading::Lit};
        MeshKind kind{MeshKind::Static};
        AlphaMode alphaMode{AlphaMode::Opaque};
        PbrFactors factors{};
    };

    // One submitted draw, valid until the end of the frame's render().
    // previousModel:  the model matrix of the matching draw last frame (see assignPreviousModels()),
    //                 or `model` when there was none; gives motion vectors.
    // worldBounds:    the mesh bounds in world space.
    // cameraDistance: distance from the camera to the bounds' centre, for sorting.
    // layers:         the render layers it is on.
    // isReflector:    it is the surface of this frame's rendered planar reflection.
    // isScopeLens:    it is the eyepiece lens of this frame's rendered scope view.
    // isSightWindow:  it is the window of this frame's holographic sight.
    // isDecal:        submitted with submitDecal().
    // jointBase:      skinned draws: index in RendererState::jointData of the palette's first
    //                 matrix (current and previous matrices interleaved per joint).
    // emissiveScale:  multiplier on the material's emissive factor for this draw.
    struct DrawItem
    {
        const vela::graphics::Mesh* mesh{nullptr};
        const MaterialRecord* material{nullptr};
        math::Mat4 model{1.0f};
        math::Mat4 previousModel{1.0f};
        math::Aabb worldBounds{};
        float cameraDistance{0.0f};
        RenderLayers layers{k_defaultRenderLayer};
        bool isReflector{false};
        bool isScopeLens{false};
        bool isSightWindow{false};
        bool isDecal{false};
        uint32_t jointBase{0};
        float emissiveScale{1.0f};
    };

    // Indices into RendererState::draws chosen for one frame.
    // opaque:           visible opaque, masked and unlit world draws, front to back.
    // decals:           visible alpha-blended decal draws, in submission order.
    // blended:          visible alpha-blended world draws, back to front.
    // viewModelOpaque:  visible opaque and masked viewmodel draws, front to back.
    // viewModelBlended: visible alpha-blended viewmodel draws, back to front.
    // shadowCasters:    per shadow atlas tile, the draws that cast into it.
    struct FrameDrawLists
    {
        std::vector<uint32_t> opaque;
        std::vector<uint32_t> decals;
        std::vector<uint32_t> blended;
        std::vector<uint32_t> viewModelOpaque;
        std::vector<uint32_t> viewModelBlended;
        std::array<std::vector<uint32_t>, k_maxShadowTiles> shadowCasters;

        // Forgets every list, keeping their storage.
        void clear();
    };

    // What the last render() drew and lit, for the dev tools.
    // local:          point and spot lights used.
    // dropped:        lights rejected over k_maxLocalLights.
    // hasDirectional: the frame had a sun (submitted or from the sky).
    // shadowedLocalLights: point and spot lights that cast a shadow.
    // localShadowTiles: shadow atlas tiles those lights rendered.
    // cascades:       sun shadow cascades rendered.
    // submitted:      draws submitted.
    // visible:        draws that passed frustum culling.
    // shadowDraws:    shadow-map draws over all atlas tiles.
    struct FrameStats
    {
        uint32_t local{0};
        uint32_t dropped{0};
        bool hasDirectional{false};
        uint32_t shadowedLocalLights{0};
        uint32_t localShadowTiles{0};
        uint32_t cascades{0};
        uint32_t submitted{0};
        uint32_t visible{0};
        uint32_t shadowDraws{0};
    };

    class TextPass;

    // An Engine pass added to the render graph, and the name its graph-owned adapter was added
    // under.
    struct RegisteredPass
    {
        std::unique_ptr<RenderPass> pass;
        std::string graphName;
    };

    // Everything the Renderer owns, kept behind a pointer so the public header stays free of Vela
    // and member addresses stay stable (the frame passes in the graph point back into this state).
    //
    // Member order is destruction order in reverse and is load-bearing: the graph goes first (its
    // passes point at `passes`, `draws`, `prepared` and `lightBuffer`), then Engine passes, then
    // GPU resources (materials before the light buffer and textures they bind), then the context.
    // The window is not owned and outlives all of it. The destructor waits for the GPU to go idle
    // before any of that happens.
    //
    // `requestedSettings` is what Renderer::settings() hands to Game and the dev tools; `settings`
    // is it limited by `quality` (limitedByQuality(), redone at the start of every render()), and
    // is what the passes read. `renderScale` and `shadowAtlasWidth` are the values the graph's
    // attachments were last described with; `scaledPassNames` are the passes whose screen-sized
    // outputs follow `renderScale` (see addScaledPass()).
    struct RendererState
    {
        // Takes ownership of an initialized context. The graph is attached afterwards, once the
        // state has its final address.
        // window:          the open window the context draws into. Not owned; must outlive this.
        // context:         a context already attached to `window`.
        // shaders:         SPIR-V of the built-in material shaders.
        // lightBuffer:     the per-frame lighting storage buffer lit materials bind.
        // jointBuffer:     the per-frame joint palette storage buffer skinned materials bind.
        // environment:     default material textures and the BRDF lookup table.
        RendererState(vela::core::Window& window, vela::core::Context&& context, MaterialShaders&& shaders,
            vela::graphics::DynamicBuffer&& lightBuffer, vela::graphics::DynamicBuffer&& jointBuffer,
            EnvironmentResources&& environment);

        ~RendererState();

        RendererState(const RendererState&) = delete;
        RendererState& operator=(const RendererState&) = delete;
        RendererState(RendererState&&) = delete;
        RendererState& operator=(RendererState&&) = delete;

        // Adds an Engine pass that draws on top of the scene, after every pass added before it.
        // pass:    the pass. The RendererState owns it from now on. Must not be null.
        // Returns: success, or an error if the render graph rejected the pass.
        [[nodiscard]] Status addPass(std::unique_ptr<RenderPass> pass);

        // Removes a pass added with addPass() from the render graph, waits for the GPU to go idle
        // and destroys it. Does nothing if `pass` is not registered.
        // pass:    the pass to remove.
        // Returns: success, or an error if the graph refused to remove it (the pass is then kept
        //          alive until the RendererState is destroyed).
        [[nodiscard]] Status removePass(const RenderPass& pass);

        // Changes the present mode; the swapchain is recreated on the next render().
        // mode:    the mode to switch to (see PresentMode for the fallbacks).
        // Returns: success, or an error if Vela rejected the change.
        [[nodiscard]] Status setPresentMode(PresentMode mode);

        vela::core::Window& window;
        vela::core::Context context;
        PresentMode presentMode{k_defaultPresentMode};

        MaterialShaders shaders;

        ResourcePool<TextureHandle, TextureRecord> textures;
        EnvironmentResources environment;
        ResourcePool<MeshHandle, MeshRecord> meshes;
        vela::graphics::DynamicBuffer lightBuffer;
        vela::graphics::DynamicBuffer jointBuffer;
        ResourcePool<MaterialHandle, MaterialRecord> materials;
        std::optional<vela::graphics::Material> shadowMaterial;
        std::optional<vela::graphics::Material> skinnedShadowMaterial;
        float shadowMaterialBias{0.0f};
        uint32_t sceneSamples{1};
        float reflectionScale{0.5f};
        uint32_t scopeResolution{512};
        float volumetricFogScale{0.5f};
        float renderScale{1.0f};
        uint32_t shadowAtlasWidth{4096};
        std::vector<std::string> scaledPassNames;

        std::vector<RegisteredPass> passes;
        uint32_t passNameCounter{0};
        TextPass* textPass{nullptr};

        std::vector<DrawItem> draws;
        std::vector<math::Mat4> jointData;
        FrameDrawLists drawLists;
        FrameLights lights;
        std::vector<PlanarReflector> reflectors;
        std::vector<AmbientZone> ambientZones;
        PreparedLighting prepared;
        PreparedReflection reflection;
        std::optional<ScopeView> scopeView;
        std::optional<HolographicSight> sight;
        PreparedScope scope;
        FrameParticles particles;
        TemporalState temporal;
        RenderSettings requestedSettings{};
        RenderSettings settings{};
        GraphicsQuality quality{};
        std::filesystem::path settingsFile;
        math::Vec4 screenFade{0.0f, 0.0f, 0.0f, 0.0f};
        FrameStats lastFrame{};
        uint32_t frameIndex{0};
        float frameSeconds{0.0f};
        std::chrono::steady_clock::time_point lastRender{};
        bool hasLoggedStaleHandle{false};

        std::optional<vela::graphics::RenderGraph> graph;

        Camera camera{};
        float aspectRatio{1.0f};
        uint32_t frameHeight{0};
        math::Mat4 view{1.0f};
        math::Mat4 projection{1.0f};
        math::Mat4 unjitteredProjection{1.0f};
    };

    // Notes a stale handle passed to a submit function: asserts in debug builds, logs once in
    // release builds.
    // state: the renderer state.
    // what:  the function and handle kind, e.g. "submit: mesh".
    void reportStaleHandle(RendererState& state, const char* what);

    // Destroys the resources retired since the last call (see ResourcePool), materials first so the
    // textures they bind are released in the same call. Called at the end of render(), after the
    // frame's draws are cleared.
    // state: the renderer state.
    void releaseRetiredResources(RendererState& state);

    // Engine-internal access to a Renderer's state, for Engine modules that extend rendering
    // (Debug adds its overlay pass through it). Game cannot use it: RendererState is private.
    // renderer: the renderer.
    // Returns:  its state, which lives as long as the renderer.
    [[nodiscard]] RendererState& rendererState(Renderer& renderer);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RENDERER_STATE_HPP
