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
    // Owns every GPU resource ImGui needs: the material, the font atlas texture and the per-frame
    // vertex and index buffers.
    //
    // Uses ImGui's legacy font atlas path: the atlas is baked once at creation, because Vela
    // textures cannot be updated after creation. Only the font texture is bound; textures passed
    // to ImGui::Image() are not supported yet.
    class ImGuiOverlayPass final : public render::RenderPass
    {
    public:
        // Loads the ImGui shaders, bakes the font atlas of the current ImGui context and creates
        // the GPU resources. The pass keeps using that context, which must outlive it.
        // context: the Vela context that owns the GPU resources.
        // Returns: the pass, or an error if a shader is missing or a GPU resource failed.
        [[nodiscard]] static Result<std::unique_ptr<ImGuiOverlayPass>> create(vela::core::Context& context);

        // Starts an ImGui frame. ImGui's IO (display size, delta time, input) must be set first.
        void beginFrame();

        // Ends the ImGui frame started by beginFrame(), if any, and keeps its draw data for record().
        void prepareFrame() override;

        // Draws the ImGui draw data of the frame ended by the last prepareFrame().
        // recorder: Vela's recorder for this pass.
        void record(vela::graphics::PassRecorder& recorder) override;

    private:
        ImGuiOverlayPass(ImGuiContext& imguiContext, std::vector<uint32_t>&& vertexShader,
            std::vector<uint32_t>&& fragmentShader, vela::graphics::Material&& material, vela::graphics::Texture&& fontTexture,
            vela::graphics::DynamicBuffer&& vertexBuffer, vela::graphics::DynamicBuffer&& indexBuffer);

        // Copies this frame's ImGui vertices and indices into the GPU buffers.
        // drawData: the frame's ImGui draw data, with at least one vertex and one index.
        // Returns:  success, or an error if a buffer could not be resized.
        [[nodiscard]] Status uploadDrawData(const ImDrawData& drawData);

        ImGuiContext* m_imguiContext{nullptr};

        std::vector<uint32_t> m_vertexShader;
        std::vector<uint32_t> m_fragmentShader;

        vela::graphics::Material m_material;
        vela::graphics::Texture m_fontTexture;
        vela::graphics::DynamicBuffer m_vertexBuffer;
        vela::graphics::DynamicBuffer m_indexBuffer;

        std::vector<ImDrawVert> m_vertices;
        std::vector<ImDrawIdx> m_indices;

        bool m_isFrameOpen{false};
        bool m_hasDrawData{false};
    };
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_IMGUI_OVERLAY_PASS_HPP
