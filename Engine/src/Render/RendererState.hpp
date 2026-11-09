#ifndef LOCKSTEP_RENDER_RENDERER_STATE_HPP
#define LOCKSTEP_RENDER_RENDERER_STATE_HPP

#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Render/Camera.hpp"
#include "Lockstep/Render/Renderer.hpp"
#include "Lockstep/Render/Resources.hpp"

#include "RenderPass.hpp"
#include "RenderPassAdapter.hpp"
#include "ResourcePool.hpp"

#include <Vela/Core/Context.hpp>
#include <Vela/Core/Window.hpp>
#include <Vela/Graphics/Material.hpp>
#include <Vela/Graphics/Mesh.hpp>
#include <Vela/Graphics/RenderGraph.hpp>
#include <Vela/Graphics/RenderScene.hpp>
#include <Vela/Graphics/Texture.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace lockstep::render
{
    // An Engine pass added to the render graph, and the graph-owned adapter that forwards to it.
    struct RegisteredPass
    {
        std::unique_ptr<RenderPass> pass;
        RenderPassAdapter* adapter{nullptr};
    };

    // Everything the Renderer owns, kept behind a pointer so the public header stays free of Vela
    // and member addresses stay stable (the render graph keeps a pointer to `scene`).
    //
    // Member order is destruction order in reverse and is load-bearing: the graph goes first (its
    // adapters point at `passes`), then the scene, then Engine passes, then GPU resources
    // (materials before the textures they bind), then the context. The window is not owned and
    // outlives all of it. The destructor waits for the GPU to go idle before any of that happens.
    struct RendererState
    {
        // Takes ownership of an initialized context. The graph is attached afterwards, once
        // `scene` has its final address.
        // window:          the open window the context draws into. Not owned; must outlive this.
        // context:         a context already attached to `window`.
        // unlitVertex:     SPIR-V of the unlit vertex shader. Kept alive because Vela materials
        //                  reference the shader code they were created from.
        // unlitFragment:   SPIR-V of the unlit fragment shader, kept alive for the same reason.
        RendererState(vela::core::Window& window, vela::core::Context&& context,
            std::vector<uint32_t>&& unlitVertex, std::vector<uint32_t>&& unlitFragment);

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

        std::vector<uint32_t> unlitVertexShader;
        std::vector<uint32_t> unlitFragmentShader;

        ResourcePool<TextureHandle, vela::graphics::Texture> textures;
        ResourcePool<MeshHandle, vela::graphics::Mesh> meshes;
        ResourcePool<MaterialHandle, vela::graphics::Material> materials;

        std::vector<RegisteredPass> passes;
        uint32_t passNameCounter{0};

        vela::graphics::RenderScene scene;
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
