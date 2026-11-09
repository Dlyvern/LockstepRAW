#ifndef LOCKSTEP_RENDER_RENDERER_HPP
#define LOCKSTEP_RENDER_RENDERER_HPP

#include "Lockstep/Core/Transform.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/Camera.hpp"
#include "Lockstep/Render/Lighting.hpp"
#include "Lockstep/Render/PbrMaterial.hpp"
#include "Lockstep/Render/PresentMode.hpp"
#include "Lockstep/Render/RenderSettings.hpp"
#include "Lockstep/Render/Resources.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>

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
    struct RendererDescription
    {
        std::string applicationName{"Lockstep"};
        PresentMode presentMode{k_defaultPresentMode};
    };

    // Owns the GPU context, the render scene and the render graph, plus every GPU resource
    // created through it. It draws into a platform::Window it does not own. Each frame:
    // setCamera(), submit() what is visible, submitLight() the lights, then render(). Lit
    // materials are glTF metallic-roughness PBR. The frame is: the procedural sky into an
    // environment atlas (image-based lighting), cascaded sun shadows (plus one spot light) into a
    // shadow atlas, a forward HDR scene pass (frustum-culled, opaque front to back, sky, blended back
    // to front), SSAO, bloom, tonemapping and FXAA; Engine overlays (dev tools) draw on top of the
    // final image. Destroying the Renderer waits for the GPU to go idle first.
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
        // Returns:     a handle to the material, or an error if a valid-looking texture handle does
        //              not resolve or the pipeline could not be created.
        [[nodiscard]] Result<MaterialHandle> createMaterial(const PbrMaterialDescription& description);

        // Sets the point of view for the next render().
        // camera: camera position, orientation and lens.
        void setCamera(const Camera& camera);

        // Queues one draw for the next render(). Invalid handles are ignored (and assert in debug).
        // mesh:      the mesh to draw.
        // material:  the material to draw it with.
        // transform: world placement of the mesh.
        void submit(MeshHandle mesh, MaterialHandle material, const math::Transform& transform);

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

        // Changes how frames reach the screen, from the next render() on. Only the swapchain is
        // recreated; nothing else is rebuilt.
        // mode:    the present mode; falls back towards VSync when unsupported (see PresentMode).
        // Returns: success, or an error if the change was rejected (the old mode stays).
        [[nodiscard]] Status setPresentMode(PresentMode mode);

        // Returns: the present mode last asked for, at creation or with setPresentMode().
        [[nodiscard]] PresentMode presentMode() const;

        // Returns: the feature switches and tunables (lighting, shadows, fog, tonemapping). Changes
        //          apply from the next render(). The dev tools edit the same settings.
        [[nodiscard]] RenderSettings& settings();

        // Returns: the feature switches and tunables, read-only.
        [[nodiscard]] const RenderSettings& settings() const;

        // Renders and presents everything submitted since the last render(), then clears the
        // submission list.
        // Returns: success, or an error if the frame could not be recorded or presented.
        [[nodiscard]] Status render();

    private:
        friend RendererState& rendererState(Renderer& renderer);

        explicit Renderer(std::unique_ptr<RendererState> state);

        std::unique_ptr<RendererState> m_state;
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RENDERER_HPP
