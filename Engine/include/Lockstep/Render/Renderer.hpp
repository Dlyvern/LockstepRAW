#ifndef LOCKSTEP_RENDER_RENDERER_HPP
#define LOCKSTEP_RENDER_RENDERER_HPP

#include "Lockstep/Core/Bounds.hpp"
#include "Lockstep/Core/Transform.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/Camera.hpp"
#include "Lockstep/Render/CharacterSurface.hpp"
#include "Lockstep/Render/GraphicsQuality.hpp"
#include "Lockstep/Render/IrradianceVolume.hpp"
#include "Lockstep/Render/LensRain.hpp"
#include "Lockstep/Render/Lighting.hpp"
#include "Lockstep/Render/PbrMaterial.hpp"
#include "Lockstep/Render/PresentMode.hpp"
#include "Lockstep/Render/RenderSettings.hpp"
#include "Lockstep/Render/Resources.hpp"
#include "Lockstep/Render/Text.hpp"
#include "Lockstep/Render/Views.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace lockstep::platform
{
    class Window;
} //namespace lockstep::platform

namespace lockstep::render
{
    struct RendererState;

    // Settings for the GPU context the Renderer creates.
    // applicationName: name reported to the Vulkan driver.
    // presentMode:     how frames reach the screen; falls back towards VSync when unsupported
    //                  (see PresentMode).
    // quality:         graphics quality from the start (see GraphicsQuality), so textures loaded
    //                  before the first frame already use its texture resolution.
    // settingsFile:    the settings file the dev tools save the quality to (see GraphicsConfig);
    //                  empty for none.
    struct RendererDescription
    {
        std::string applicationName{"Lockstep"};
        PresentMode presentMode{k_defaultPresentMode};
        GraphicsQuality quality{};
        std::filesystem::path settingsFile{};
    };

    // Most planar reflectors kept per frame (see Renderer::submitReflector()).
    inline constexpr uint32_t k_maxPlanarReflectors{8};

    // Most skinning matrices one joint palette may hold.
    inline constexpr uint32_t k_maxPaletteJoints{256};

    // A joint palette queued for the current frame by Renderer::submitJointPalette(). Valid only
    // until the next render().
    // first: where the palette starts in the frame's palette storage.
    // count: its joint count; 0 for none (draws using it are dropped).
    struct JointPalette
    {
        uint32_t first{0};
        uint32_t count{0};
    };

    // Owns the GPU context, the render scene and the render graph, plus every GPU resource
    // created through it. It draws into a platform::Window it does not own. Each frame:
    // setCamera(), submit() what is visible, submitLight() the lights, then render(). Lit
    // materials are glTF metallic-roughness PBR. The frame is: the procedural sky into an
    // environment atlas (image-based lighting), cascaded sun shadows (plus one spot light) into a
    // shadow atlas, the nearest planar reflector's mirrored view, a forward HDR scene pass
    // (frustum-culled, opaque front to back, sky, blended back to front; also writes motion
    // vectors; first-person viewmodel draws, see k_viewModelLayer, go in front of the world with
    // their own projection), SSAO, bloom, tonemapping, then anti-aliasing (TAA, FXAA or MSAA, see
    // AntiAliasingSettings) and dithering; screen text (submitText()) and then Engine overlays
    // (dev tools) draw on top of the final image. Destroying the Renderer waits for the GPU to go
    // idle first.
    class Renderer
    {
    public:
        // Initializes the GPU context on a window and builds the forward render graph.
        // window:      the window to draw into. It must outlive the Renderer and must not be
        //              move-assigned to while the Renderer exists.
        // description: application name and presentation settings.
        // Returns:     the renderer, or an error if the device, swapchain or built-in shaders
        //              could not be created.
        [[nodiscard]] static Result<Renderer> create(platform::Window& window,
            const RendererDescription& description);

        ~Renderer();

        Renderer(Renderer&& other) noexcept;
        Renderer& operator=(Renderer&& other) noexcept;

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // Uploads a static mesh to the GPU.
        // vertices: vertex data.
        // indices:  triangle-list indices into `vertices`. Must not be empty.
        // Returns:  a handle to the mesh, or an error if the data is empty or the upload failed.
        [[nodiscard]] Result<MeshHandle> createMesh(std::span<const Vertex> vertices,
            std::span<const uint32_t> indices);

        // Uploads a static PBR mesh to the GPU and records its bounds for culling.
        // vertices: vertex data with normals, texture coordinates and tangents (see PbrVertex).
        // indices:  triangle-list indices into `vertices`, counter-clockwise front faces (as in
        //           glTF). Must not be empty.
        // Returns:  a handle to the mesh, or an error if the data is empty or the upload failed.
        [[nodiscard]] Result<MeshHandle> createMesh(std::span<const PbrVertex> vertices,
            std::span<const uint32_t> indices);

        // Uploads a skinned PBR mesh to the GPU. Draw it with submitSkinned() and a material
        // created for MeshKind::Skinned.
        // vertices: bind-pose vertices with joint indices and weights (see SkinnedPbrVertex).
        // indices:  triangle-list indices into `vertices`, counter-clockwise front faces. Must not
        //           be empty.
        // Returns:  a handle to the mesh, or an error if the data is empty, an index or a joint
        //           index is out of range, or the upload failed.
        [[nodiscard]] Result<MeshHandle> createMesh(std::span<const SkinnedPbrVertex> vertices,
            std::span<const uint32_t> indices);

        // Uploads a 2D texture (RGBA8 or block compressed, with or without mips) to the GPU.
        // description: pixel data of every level, size, format, filtering, level count and whether
        //              the GPU generates the mips. Pixels must hold exactly the sum of
        //              textureLevelBytes() over the levels.
        // Returns:     a handle to the texture, or an error if the description is invalid, a Bc
        //              format is used without supportsBlockCompression(), or the upload failed.
        [[nodiscard]] Result<TextureHandle> createTexture(const TextureDescription& description);

        // Returns: true if the GPU samples Bc texture formats; without it only Rgba8 textures can be
        //          created.
        [[nodiscard]] bool supportsBlockCompression() const;

        // Creates a textured material, lit or unlit.
        // description: the albedo texture, which must have been created by this renderer, and the
        //              shading parameters.
        // Returns:     a handle to the material, or an error if the texture handle is invalid or the
        //              pipeline could not be created.
        [[nodiscard]] Result<MaterialHandle> createMaterial(const MaterialDescription& description);

        // Creates a glTF 2.0 metallic-roughness material (see PbrMaterialDescription).
        // description: factors, textures created by this renderer (any may be invalid), alpha mode
        //              and sidedness.
        // kind:        the kind of mesh it draws; a material only draws meshes of its kind.
        // Returns:     a handle to the material, or an error if a valid-looking texture handle does
        //              not resolve or the pipeline could not be created.
        [[nodiscard]] Result<MaterialHandle> createMaterial(const PbrMaterialDescription& description,
            MeshKind kind = MeshKind::Static);

        // Changes the shading model of a lit material (see MaterialShading), e.g. skin for a face or
        // fabric for curtains, picked by Game after a level or model was loaded. Every draw with the
        // material changes from the next render().
        // material: a lit material. Invalid, stale or unlit handles are ignored.
        // shading:  the shading model and its parameters.
        // Returns:  true if the material was changed.
        bool setMaterialShading(MaterialHandle material, const MaterialShading& shading);

        // Queues a character surface (blood, dirt, rain soak) for the next render(), for that
        // character's submitSkinned() draws to pass. At most k_maxCharacterSurfaces per frame.
        // surface: the layers; stainCount is capped at k_maxSurfaceStains.
        // Returns: its id for this frame, or k_noCharacterSurface when the budget is used up.
        [[nodiscard]] CharacterSurfaceId submitCharacterSurface(const CharacterSurface& surface);

        // Destroys a mesh (static or skinned). The handle, and every copy of it, is stale at once;
        // the GPU memory is released once every frame in flight that could draw it has finished
        // (draws submitted earlier this frame still render). Submitting a stale handle is a bug:
        // it asserts in debug builds and is ignored, with one log line, in release builds.
        // mesh:    the mesh. Invalid or already destroyed handles are ignored.
        // Returns: true if a living mesh was destroyed.
        bool destroyMesh(MeshHandle mesh);

        // Destroys a material, deferred like destroyMesh(). The textures it binds stay alive until
        // they are destroyed themselves.
        // material: the material. Invalid or already destroyed handles are ignored.
        // Returns:  true if a living material was destroyed.
        bool destroyMaterial(MaterialHandle material);

        // Destroys a texture, deferred like destroyMesh(). Materials created with it keep sampling
        // it: its memory is released only after the last of them is destroyed as well.
        // texture: the texture. Invalid or already destroyed handles are ignored.
        // Returns: true if a living texture was destroyed.
        bool destroyTexture(TextureHandle texture);

        // Returns: GPU memory use and living resource counts.
        [[nodiscard]] GpuMemoryStats memoryStats() const;

        // Waits for the GPU to go idle, then frees the GPU memory of every destroyed mesh, material
        // and texture at once instead of after the frames in flight. For level switches: destroy
        // the old level, call this, then load the new one. Stalls the GPU, so not for every frame.
        // Call it before submitting this frame's draws; draws already submitted are discarded.
        // Returns: an error if waiting for the GPU failed.
        [[nodiscard]] Status releaseDestroyed();

        // Sets the point of view for the next render().
        // camera: camera position, orientation and lens.
        void setCamera(const Camera& camera);

        // Fades the final image towards a flat colour (fade to black, flash to white). Applied
        // after tonemapping and anti-aliasing and before overlays, so text and dev tools stay
        // visible, and independent of RenderSettings (the exposure the dev tools edit is kept).
        // Stays in effect until changed; amount 0 turns it off.
        // amount: how far towards `colour`, clamped to 0 (no fade) .. 1 (only `colour`).
        // colour: the linear RGB colour faded to.
        void setScreenFade(float amount, const math::Vec3& colour = math::Vec3{0.0f, 0.0f, 0.0f});

        // Queues one draw on k_defaultRenderLayer for the next render(). Invalid or stale handles,
        // and skinned meshes or materials (see submitSkinned()), are ignored (and assert in debug).
        // mesh:      the mesh to draw.
        // material:  the material to draw it with.
        // transform: world placement of the mesh.
        void submit(MeshHandle mesh, MaterialHandle material, const math::Transform& transform);

        // Queues one draw on chosen render layers for the next render(). Draws are matched to the
        // previous frame by mesh, material and submission order among draws of that pair, which
        // gives moving draws their motion vectors (TAA); keep the submission order stable.
        // mesh:      the mesh to draw.
        // material:  the material to draw it with.
        // transform: world placement of the mesh.
        // layers:    the layers it is on; views that share none of them skip it. With
        //            k_viewModelLayer it is a viewmodel draw, which needs a lit material (unlit
        //            ones are ignored, and assert in debug).
        void submit(MeshHandle mesh, MaterialHandle material, const math::Transform& transform, RenderLayers layers);

        // Queues one draw like submit() with layers, with its material's emissive light scaled for
        // this draw only (a lamp mesh dimming with its flickering light).
        // mesh:          the mesh to draw.
        // material:      the material to draw it with.
        // transform:     world placement of the mesh.
        // layers:        the layers it is on.
        // emissiveScale: multiplier on the material's emissive light, 0 (dark) and up; negative
        //                values count as 0. Lit materials only (unlit ones ignore it).
        void submit(MeshHandle mesh, MaterialHandle material, const math::Transform& transform, RenderLayers layers,
            float emissiveScale);

        // Queues a decal draw on k_defaultRenderLayer for the next render(): a thin mesh (usually a
        // quad) lying a few millimetres off a surface, drawn with an alpha-blended material over the
        // opaque scene right after it (before the sky and other transparent draws), in submission
        // order, so later decals cover earlier ones. Its ambient light goes through SSAO like the
        // surface below, and it casts no shadow. With an opaque or masked material it is an
        // ordinary draw. Otherwise like submit().
        // mesh:      the decal mesh.
        // material:  its material, usually AlphaMode::Blend.
        // transform: world placement of the mesh.
        void submitDecal(MeshHandle mesh, MaterialHandle material, const math::Transform& transform);

        // Queues skinning matrices for the next render(), for submitSkinned() draws to share.
        // joints:         per joint, model-space joint matrix times inverse bind matrix. At most
        //                 k_maxPaletteJoints.
        // previousJoints: the same matrices as of the previous frame, for motion vectors (TAA);
        //                 pass `joints` again when there are none. Must have the same size.
        // Returns:        the palette, or an empty one (count 0) if the sizes are wrong.
        [[nodiscard]] JointPalette submitJointPalette(std::span<const math::Mat4> joints,
            std::span<const math::Mat4> previousJoints);

        // Queues one skinned draw for the next render(). The mesh's vertices are moved by the
        // palette's matrices on the GPU in every view (camera, shadows, planar reflection) and get
        // motion vectors from the previous matrices. Like submit(), draws are matched to the
        // previous frame by mesh, material and submission order, and the match's transform is
        // the previous model matrix, so a moving character gets object motion as well as joint
        // motion; a draw with no match (first frame, TAA off) uses `transform` for both.
        // Invalid or stale handles,
        // mismatched mesh or material kinds and empty palettes are ignored (and assert in debug).
        // mesh:        a mesh created from SkinnedPbrVertex data.
        // material:    a material created for MeshKind::Skinned.
        // transform:   world placement of the model space the palette is in.
        // palette:     this frame's joint palette from submitJointPalette().
        // localBounds: model-space bounds of the posed mesh, for culling.
        // layers:      the render layers it is on.
        // surface:     the character surface drawn over it (submitCharacterSurface() this frame), or
        //              k_noCharacterSurface.
        void submitSkinned(MeshHandle mesh, MaterialHandle material, const math::Transform& transform,
            JointPalette palette, const math::Aabb& localBounds, RenderLayers layers = k_defaultRenderLayer,
            CharacterSurfaceId surface = k_noCharacterSurface);

        // Queues a planar reflector (a mirror) for the next render(). Up to k_maxPlanarReflectors
        // are kept per frame; the nearest visible one within RenderSettings::reflections.maxDistance
        // is rendered, the others show the environment reflection.
        // reflector: the mirror's mesh instance and what its reflection shows.
        void submitReflector(const PlanarReflector& reflector);

        // Queues a magnified optic for the next render() (see ScopeView). One per frame; a later
        // call replaces an earlier one. Without a call (or with RenderSettings::scope off) no scope
        // view is rendered and the lens draw shows its own material.
        // scope: the optic, its lens draw and its view.
        void submitScope(const ScopeView& scope);

        // Queues a holographic sight's reticle for the next render() (see HolographicSight). One per
        // frame; a later call replaces an earlier one. Without a call (or with
        // RenderSettings::scope::holographic off) the window is plain glass.
        // sight: the sight's window draw and reticle.
        void submitSight(const HolographicSight& sight);

        // Sets the directional light for the next render(). Without a call there is none that frame.
        // light: the light.
        void submitLight(const DirectionalLight& light);

        // Queues a point light for the next render(). Dropped once k_maxLocalLights point and spot
        // lights are queued.
        // light: the light.
        void submitLight(const PointLight& light);

        // Queues a spot light for the next render(). Dropped once k_maxLocalLights point and spot
        // lights are queued.
        // light: the light.
        void submitLight(const SpotLight& light);

        // Queues a drip puddle for the next render() (see DripPuddle). Each frame shades the
        // k_maxDripPuddles nearest the camera (SurfaceEffectSettings::dripPuddles).
        // puddle: the puddle.
        void submitDripPuddle(const DripPuddle& puddle);

        // Replaces the interior ambient zones (see AmbientZone and InteriorAmbientSettings). They stay
        // until replaced; pass an empty span when a level is unloaded. Any number may be set; each
        // frame shades with the k_maxFrameAmbientZones nearest the camera.
        // zones: the zones, copied.
        void setAmbientZones(std::span<const AmbientZone> zones);

        // Replaces the baked irradiance volume lit surfaces inside it sample for bounce light (see
        // IrradianceVolume, IrradianceVolumeSettings). It stays until replaced; pass null when its
        // level is unloaded.
        // volume:  the volume, copied; null for none.
        // Returns: success, or InvalidArgument for a malformed volume (the previous one is then
        //          removed), or an upload error.
        [[nodiscard]] Status setIrradianceVolume(const IrradianceVolume* volume);

        // Queues the rain drops on the lens for the next render() only (see LensRain and
        // LensRainSettings), replacing earlier ones this frame.
        // droplets: the drops, copied (at most k_maxLensDroplets are drawn).
        void submitLensDroplets(std::span<const LensDroplet> droplets);

        // Queues screen text for the next render() only, like submit(). Text is drawn after
        // tonemapping and anti-aliasing, before the dev tools overlay, in submission order, all in
        // one draw. '\n' starts a new line; characters the Engine font lacks show as '?'.
        // utf8:     the text, UTF-8 encoded. Copied; it need not outlive the call.
        // position: normalized screen coordinates, (0,0) top-left, (1,1) bottom-right. x is the
        //           point style.align picks on each line, y the top of the first line.
        // style:    size, colour, alignment and shadow.
        void submitText(std::string_view utf8, math::Vec2 position, const TextStyle& style = {});

        // Queues a filled screen rectangle for the next render() only. Screen rectangles, images
        // and text share one overlay: they draw after tonemapping and anti-aliasing, before the dev
        // tools, in submission order (later ones cover earlier ones).
        // rect:  the rectangle (see ScreenRect).
        // color: sRGB colour; alpha is opacity.
        void submitRect(const ScreenRect& rect, const math::Vec4& color);

        // Queues a textured screen rectangle for the next render() only, in the overlay order of
        // submitRect(). An invalid or stale texture is skipped.
        // texture: a texture created by this renderer (colour formats; sampled with its filter).
        // rect:    the rectangle.
        // style:   tint and texture coordinates.
        void submitImage(TextureHandle texture, const ScreenRect& rect, const ImageStyle& style = {});

        // Limits the overlay draws submitted after this call (text, rectangles, images) to a
        // rectangle, until the next call; every frame starts without a limit.
        // clip: the visible rectangle in normalized screen coordinates, or nothing for the whole
        //       screen.
        void setScreenClip(const std::optional<ScreenRect>& clip);

        // Measures text as submitText() would lay it out.
        // utf8:    the text; '\n' starts a new line.
        // style:   size and font (alignment and colour don't matter).
        // Returns: width of the widest line and height of all lines, in pixels at 1080p (the unit
        //          of TextStyle::size; multiply by the framebuffer height / 1080 for real pixels).
        [[nodiscard]] math::Vec2 measureText(std::string_view utf8, const TextStyle& style) const;

        // Returns: the size of the presented image in pixels (the window's framebuffer), for
        //          laying out screen overlays; (0, 0) while the window is minimized.
        [[nodiscard]] math::Vec2 screenSize() const;

        // Changes how frames reach the screen, from the next render() on. Only the swapchain is
        // recreated; nothing else is rebuilt.
        // mode:    the present mode; falls back towards VSync when unsupported (see PresentMode).
        // Returns: success, or an error if the change was rejected (the old mode stays).
        [[nodiscard]] Status setPresentMode(PresentMode mode);

        // Returns: the present mode last asked for, at creation or with setPresentMode().
        [[nodiscard]] PresentMode presentMode() const;

        // Changes the graphics quality. Render resolution, shadow atlas size and effect limits apply
        // from the next render() (attachments are rebuilt once); the texture resolution applies to
        // textures created afterwards, so reload the level to see it.
        // quality: the new quality; fields are clamped to their ranges.
        void setGraphicsQuality(const GraphicsQuality& quality);

        // Returns: the graphics quality in effect.
        [[nodiscard]] const GraphicsQuality& graphicsQuality() const;

        // Sets a temporary frame rate cap on top of GraphicsQuality::frameRateLimit, for screens
        // that need no high frame rate (menus, intro, loading). Stays until changed.
        // framesPerSecond: the cap, 0 to remove it (otherwise clamped to 20 to 1000).
        void setFrameRateCap(uint32_t framesPerSecond);

        // Returns: frames per second the run loop renders at most: the lower of
        //          GraphicsQuality::frameRateLimit and setFrameRateCap() that is set, or 0 for none.
        [[nodiscard]] uint32_t frameRateLimit() const;

        // Returns: the feature switches and tunables (lighting, shadows, fog, tonemapping). Changes
        //          apply from the next render(), limited by graphicsQuality() (a feature it switches
        //          off stays off; sample counts and resolutions are scaled). The dev tools edit the
        //          same settings.
        [[nodiscard]] RenderSettings& settings();

        // Returns: the feature switches and tunables, read-only.
        [[nodiscard]] const RenderSettings& settings() const;

        // Renders and presents everything submitted since the last render(), then clears the
        // submission list.
        // Returns: success, or an error if the frame could not be recorded or presented.
        [[nodiscard]] Status render();

        // Saves the next rendered frame as a PNG, as presented (tonemapped, with overlays), for dev
        // checks of how things look without a screenshot tool. Nothing stalls: the pixels are
        // copied at the end of the next render() and the file is written during a later one, once
        // the GPU finished that frame (about frames-in-flight frames later); success or failure is
        // logged on the "Render" channel then.
        // path:    the file to write (overwritten); RGB, uncompressed, so about 6 MB at 1080p.
        // Returns: an error if the path is empty or a capture is still pending.
        [[nodiscard]] Status captureNextFrame(std::string_view path);

    private:
        friend RendererState& rendererState(Renderer& renderer);

        explicit Renderer(std::unique_ptr<RendererState> state);

        std::unique_ptr<RendererState> m_state;
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RENDERER_HPP
