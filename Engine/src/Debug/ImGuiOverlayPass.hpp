#ifndef LOCKSTEP_DEBUG_IMGUI_OVERLAY_PASS_HPP
#define LOCKSTEP_DEBUG_IMGUI_OVERLAY_PASS_HPP

#include "Lockstep/Core/Result.hpp"

#include "Render/RenderPass.hpp"

#include <Vela/Core/Context.hpp>
#include <Vela/Graphics/DynamicBuffer.hpp>
#include <Vela/Graphics/Material.hpp>
#include <Vela/Graphics/Texture.hpp>

#include <imgui.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace lockstep::debug
{
    // Renders ImGui's draw data on top of the scene, and owns ImGui's frame lifecycle: beginFrame()
    // starts an ImGui frame, prepareFrame() (called by the Renderer before recording) ends it.
    // Owns every GPU resource ImGui needs: the material, the per-frame vertex and index buffers and
    // the textures ImGui asks for (ImGuiBackendFlags_RendererHasTextures: the font atlas is
    // created, updated and destroyed on ImGui's request, so fonts can be rasterized at any size).
    // Textures that are not ImGui's own (ImGui::Image() with a user texture) are not supported yet.
    class ImGuiOverlayPass final : public render::RenderPass
    {
    public:
        // Loads the ImGui shaders and creates the GPU resources. The pass keeps using the current
        // ImGui context, which must outlive it and must have ImGuiBackendFlags_RendererHasTextures
        // set.
        // context: the Vela context that owns the GPU resources.
        // Returns: the pass, or an error if a shader is missing or a GPU resource failed.
        [[nodiscard]] static Result<std::unique_ptr<ImGuiOverlayPass>> create(vela::core::Context& context);

        // Starts an ImGui frame. ImGui's IO (display size, delta time, input) must be set first.
        void beginFrame();

        // Ends the ImGui frame started by beginFrame(), if any, handles ImGui's texture requests
        // and keeps the draw data for record().
        void prepareFrame() override;

        // Draws the ImGui draw data of the frame ended by the last prepareFrame().
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

        // Marks every texture ImGui still holds as destroyed, so the ImGui context can be destroyed
        // after the pass. The GPU textures are released once frames in flight are done with them.
        ~ImGuiOverlayPass() override;

        ImGuiOverlayPass(const ImGuiOverlayPass&) = delete;
        ImGuiOverlayPass& operator=(const ImGuiOverlayPass&) = delete;
        ImGuiOverlayPass(ImGuiOverlayPass&&) = delete;
        ImGuiOverlayPass& operator=(ImGuiOverlayPass&&) = delete;

    private:
        ImGuiOverlayPass(vela::core::Context& context, ImGuiContext& imguiContext, std::vector<uint32_t>&& vertexShader,
            std::vector<uint32_t>&& fragmentShader, vela::graphics::Material&& material,
            vela::graphics::DynamicBuffer&& vertexBuffer, vela::graphics::DynamicBuffer&& indexBuffer);

        // Creates, updates or destroys one ImGui texture as its status asks.
        // texture: the ImGui texture with a pending request.
        // Returns: success, or an error if a GPU texture could not be created or updated.
        [[nodiscard]] Status handleTextureRequest(ImTextureData& texture);

        // Copies this frame's ImGui vertices and indices into the GPU buffers.
        // drawData: the frame's ImGui draw data, with at least one vertex and one index.
        // Returns:  success, or an error if a buffer could not be resized.
        [[nodiscard]] Status uploadDrawData(const ImDrawData& drawData);

        vela::core::Context* m_context{nullptr};
        ImGuiContext* m_imguiContext{nullptr};

        std::vector<uint32_t> m_vertexShader;
        std::vector<uint32_t> m_fragmentShader;

        vela::graphics::Material m_material;
        vela::graphics::DynamicBuffer m_vertexBuffer;
        vela::graphics::DynamicBuffer m_indexBuffer;

        std::vector<ImDrawVert> m_vertices;
        std::vector<ImDrawIdx> m_indices;
        std::vector<std::unique_ptr<vela::graphics::Texture>> m_textures;

        bool m_isFrameOpen{false};
        bool m_hasDrawData{false};
    };
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_IMGUI_OVERLAY_PASS_HPP
