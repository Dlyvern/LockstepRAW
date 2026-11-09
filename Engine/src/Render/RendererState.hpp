#ifndef LOCKSTEP_RENDER_RENDERER_STATE_HPP
#define LOCKSTEP_RENDER_RENDERER_STATE_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/Camera.hpp"
#include "Lockstep/Render/Renderer.hpp"
#include "Lockstep/Render/RenderSettings.hpp"
#include "Lockstep/Render/Resources.hpp"

#include "FrameLighting.hpp"
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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace lockstep::render
{
    // SPIR-V of the built-in material shaders, loaded once. Kept for creating materials later.
    struct MaterialShaders
    {
        std::vector<uint32_t> unlitVertex;
        std::vector<uint32_t> unlitFragment;
        std::vector<uint32_t> litVertex;
        std::vector<uint32_t> litFragment;
    };

    // A material and the shading parameters the scene pass pushes with each of its draws.
    // material: the Vela material (pipeline and set-1 bindings).
    // shading:  lit or unlit.
    // surface:  x specular, y shininess, z emissive, w unused. Read by lit.frag.
    struct MaterialRecord
    {
        vela::graphics::Material material;
        Shading shading{Shading::Lit};
        float surface[4]{0.0f, 0.0f, 0.0f, 0.0f};
    };

    // One submitted draw, valid until the end of the frame's render().
    struct DrawItem
    {
        const vela::graphics::Mesh* mesh{nullptr};
        const MaterialRecord* material{nullptr};
        math::Mat4 model{1.0f};
    };

    // What the last render() lit, for the dev tools.
    // local:          point and spot lights used.
    // dropped:        lights rejected over k_maxLocalLights.
    // hasDirectional: a directional light was submitted.
    // caster:         which light cast the shadow map.
    struct FrameLightStats
    {
        uint32_t local{0};
        uint32_t dropped{0};
        bool hasDirectional{false};
        ShadowCaster caster{ShadowCaster::None};
    };

    // An Engine pass added to the render graph, and the graph-owned adapter that forwards to it.
    struct RegisteredPass
    {
        std::unique_ptr<RenderPass> pass;
        RenderPassAdapter* adapter{nullptr};
    };

    // Everything the Renderer owns, kept behind a pointer so the public header stays free of Vela
    // and member addresses stay stable (the frame passes in the graph point back into this state).
    //
    // Member order is destruction order in reverse and is load-bearing: the graph goes first (its
    // passes point at `passes`, `draws`, `prepared` and `lightBuffer`), then Engine passes, then
    // GPU resources (materials before the light buffer and textures they bind), then the context.
    // The window is not owned and outlives all of it. The destructor waits for the GPU to go idle
    // before any of that happens.
    struct RendererState
    {
        // Takes ownership of an initialized context. The graph is attached afterwards, once the
        // state has its final address.
        // window:          the open window the context draws into. Not owned; must outlive this.
        // context:         a context already attached to `window`.
        // shaders:         SPIR-V of the built-in material shaders.
        // lightBuffer:     the per-frame lighting storage buffer lit materials bind.
        RendererState(vela::core::Window& window, vela::core::Context&& context, MaterialShaders&& shaders,
            vela::graphics::DynamicBuffer&& lightBuffer);

        ~RendererState();

        RendererState(const RendererState&) = delete;
        RendererState& operator=(const RendererState&) = delete;
        RendererState(RendererState&&) = delete;
        RendererState& operator=(RendererState&&) = delete;

        // Adds an Engine pass that draws on top of the scene, after every pass added before it.
        // pass:    the pass. The RendererState owns it from now on. Must not be null.
        // Returns: success, or an error if the render graph rejected the pass.
        [[nodiscard]] Status addPass(std::unique_ptr<RenderPass> pass);

        // Waits for the GPU to go idle, then destroys a pass added with addPass(). Does nothing if
        // `pass` is not registered.
        // pass: the pass to remove.
        void removePass(const RenderPass& pass);

        vela::core::Window& window;
        vela::core::Context context;

        MaterialShaders shaders;

        ResourcePool<TextureHandle, vela::graphics::Texture> textures;
        ResourcePool<MeshHandle, vela::graphics::Mesh> meshes;
        vela::graphics::DynamicBuffer lightBuffer;
        ResourcePool<MaterialHandle, MaterialRecord> materials;

        std::vector<RegisteredPass> passes;
        uint32_t passNameCounter{0};

        std::vector<DrawItem> draws;
        FrameLights lights;
        PreparedLighting prepared;
        RenderSettings settings{};
        FrameLightStats lastFrameLights{};
        uint32_t frameIndex{0};

        std::optional<vela::graphics::RenderGraph> graph;

        Camera camera{};
        float aspectRatio{1.0f};
    };

    // Engine-internal access to a Renderer's state, for Engine modules that extend rendering
    // (Debug adds its overlay pass through it). Game cannot use it: RendererState is private.
    // renderer: the renderer.
    // Returns:  its state, which lives as long as the renderer.
    [[nodiscard]] RendererState& rendererState(Renderer& renderer);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RENDERER_STATE_HPP
