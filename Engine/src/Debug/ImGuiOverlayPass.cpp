#include "ImGuiOverlayPass.hpp"

#include "Lockstep/Core/Math.hpp"

#include "Render/ShaderLoader.hpp"
#include "Render/VelaConversions.hpp"

#include <Vela/Graphics/ImageData.hpp>
#include <Vela/Graphics/RenderTypes.hpp>
#include <Vela/Graphics/VertexLayout.hpp>

#include <algorithm>
#include <iostream>
#include <cstddef>
#include <span>
#include <string_view>
#include <utility>

namespace lockstep::debug
{
    namespace
    {
        constexpr std::string_view k_vertexShader{"imgui.vert.spv"};
        constexpr std::string_view k_fragmentShader{"imgui.frag.spv"};
        constexpr uint32_t k_fontTextureSlot{0};

        static_assert(sizeof(ImDrawIdx) == sizeof(uint16_t), "the index buffer is created as 16-bit");

        vela::graphics::VertexLayout imguiVertexLayout()
        {
            return vela::graphics::VertexLayout
            {
                sizeof(ImDrawVert),
                {
                    {0, vela::graphics::VertexAttributeFormat::Float2, static_cast<uint32_t>(offsetof(ImDrawVert, pos))},
                    {1, vela::graphics::VertexAttributeFormat::Float2, static_cast<uint32_t>(offsetof(ImDrawVert, uv))},
                    {2, vela::graphics::VertexAttributeFormat::Unorm8x4, static_cast<uint32_t>(offsetof(ImDrawVert, col))}
                }
            };
        }

        vela::graphics::RenderState overlayRenderState()
        {
            vela::graphics::RenderState renderState{};
            renderState.blend = vela::graphics::BlendMode::Alpha;
            renderState.depthTest = false;
            renderState.depthWrite = false;
            renderState.cull = vela::graphics::CullMode::None;

            return renderState;
        }

        Result<vela::graphics::Texture> createFontTexture(vela::core::Context& context)
        {
            unsigned char* pixels = nullptr;
            int width = 0;
            int height = 0;
            int bytesPerPixel = 0;

            ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytesPerPixel);

            if (pixels == nullptr || width <= 0 || height <= 0)
            {
                return Error{ErrorCode::Unknown, "ImGui font atlas could not be built"};
            }

            vela::graphics::ImageData image{};
            image.width = static_cast<uint32_t>(width);
            image.height = static_cast<uint32_t>(height);
            image.format = vela::graphics::TextureFormat::RGBA8Unorm;
            image.pixels = std::span<const std::byte>(reinterpret_cast<const std::byte*>(pixels),
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height)
                    * static_cast<std::size_t>(bytesPerPixel));

            auto texture = vela::graphics::Texture::create(context, image);

            if (!texture)
            {
                return render::toError(texture.error());
            }

            return std::move(texture).value();
        }

        Result<vela::graphics::DynamicBuffer> createBuffer(vela::core::Context& context,
            vela::graphics::DynamicBuffer::Usage usage)
        {
            auto buffer = vela::graphics::DynamicBuffer::create(context, usage, 0,
                vela::graphics::DynamicBuffer::IndexType::Uint16);

            if (!buffer)
            {
                return render::toError(buffer.error());
            }

            return std::move(buffer).value();
        }
    } //namespace

    Result<std::unique_ptr<ImGuiOverlayPass>> ImGuiOverlayPass::create(vela::core::Context& context)
    {
        ImGuiContext* imguiContext = ImGui::GetCurrentContext();

        if (imguiContext == nullptr)
        {
            return Error{ErrorCode::InvalidArgument, "ImGuiOverlayPass: no current ImGui context"};
        }

        auto vertexShader = render::loadShader(k_vertexShader);

        if (!vertexShader)
        {
            return vertexShader.error();
        }

        auto fragmentShader = render::loadShader(k_fragmentShader);

        if (!fragmentShader)
        {
            return fragmentShader.error();
        }

        auto material = vela::graphics::Material::create(context,
        {
            .vertexShader = vertexShader.value(),
            .fragmentShader = fragmentShader.value(),
            .vertexLayout = imguiVertexLayout(),
            .renderState = overlayRenderState()
        });

        if (!material)
        {
            return render::toError(material.error());
        }

        auto fontTexture = createFontTexture(context);

        if (!fontTexture)
        {
            return fontTexture.error();
        }

        if (auto bound = material.value().setTexture(k_fontTextureSlot, fontTexture.value()); !bound)
        {
            return render::toError(bound.error());
        }

        auto vertexBuffer = createBuffer(context, vela::graphics::DynamicBuffer::Usage::Vertex);

        if (!vertexBuffer)
        {
            return vertexBuffer.error();
        }

        auto indexBuffer = createBuffer(context, vela::graphics::DynamicBuffer::Usage::Index);

        if (!indexBuffer)
        {
            return indexBuffer.error();
        }

        return std::unique_ptr<ImGuiOverlayPass>(new ImGuiOverlayPass(*imguiContext, std::move(vertexShader).value(),
            std::move(fragmentShader).value(), std::move(material).value(), std::move(fontTexture).value(),
            std::move(vertexBuffer).value(), std::move(indexBuffer).value()));
    }

    ImGuiOverlayPass::ImGuiOverlayPass(ImGuiContext& imguiContext, std::vector<uint32_t>&& vertexShader,
        std::vector<uint32_t>&& fragmentShader, vela::graphics::Material&& material, vela::graphics::Texture&& fontTexture,
        vela::graphics::DynamicBuffer&& vertexBuffer, vela::graphics::DynamicBuffer&& indexBuffer)
        : m_imguiContext(&imguiContext),
          m_vertexShader(std::move(vertexShader)),
          m_fragmentShader(std::move(fragmentShader)),
          m_material(std::move(material)),
          m_fontTexture(std::move(fontTexture)),
          m_vertexBuffer(std::move(vertexBuffer)),
          m_indexBuffer(std::move(indexBuffer))
    {
    }

    void ImGuiOverlayPass::beginFrame()
    {
        ImGui::SetCurrentContext(m_imguiContext);

        if (m_isFrameOpen)
        {
            ImGui::EndFrame();
        }

        ImGui::NewFrame();
        m_isFrameOpen = true;
    }

    void ImGuiOverlayPass::prepareFrame()
    {
        m_hasDrawData = false;

        if (!m_isFrameOpen)
        {
            return;
        }

        ImGui::SetCurrentContext(m_imguiContext);
        ImGui::Render();
        m_isFrameOpen = false;

        const ImDrawData* drawData = ImGui::GetDrawData();
        m_hasDrawData = drawData != nullptr && drawData->TotalIdxCount > 0;
    }

    void ImGuiOverlayPass::record(vela::graphics::PassRecorder& recorder)
    {
        if (!m_hasDrawData)
        {
            return;
        }

        ImGui::SetCurrentContext(m_imguiContext);
        const ImDrawData& drawData = *ImGui::GetDrawData();

        if (drawData.TotalVtxCount == 0 || drawData.TotalIdxCount == 0)
        {
            return;
        }

        if (auto uploaded = uploadDrawData(drawData); !uploaded)
        {
            std::cerr << "DevTools: " << uploaded.error().message << '\n';
            return;
        }

        const math::Mat4 projection = math::ortho(drawData.DisplayPos.x, drawData.DisplayPos.x + drawData.DisplaySize.x,
            drawData.DisplayPos.y, drawData.DisplayPos.y + drawData.DisplaySize.y, -1.0f, 1.0f);

        recorder.bind(m_material);
        recorder.bindVertexBuffer(m_vertexBuffer);
        recorder.bindIndexBuffer(m_indexBuffer);
        recorder.setConstants(projection);

        const vela::graphics::Extent2D extent = recorder.extent();
        const ImVec2 clipOffset = drawData.DisplayPos;
        const ImVec2 clipScale = drawData.FramebufferScale;

        uint32_t indexOffset = 0;
        uint32_t vertexOffset = 0;

        for (const ImDrawList* list : drawData.CmdLists)
        {
            for (const ImDrawCmd& command : list->CmdBuffer)
            {
                if (command.UserCallback != nullptr)
                {
                    continue;
                }

                const float clipLeft = (command.ClipRect.x - clipOffset.x) * clipScale.x;
                const float clipTop = (command.ClipRect.y - clipOffset.y) * clipScale.y;
                const float clipRight = (command.ClipRect.z - clipOffset.x) * clipScale.x;
                const float clipBottom = (command.ClipRect.w - clipOffset.y) * clipScale.y;

                const int32_t left = std::max(static_cast<int32_t>(clipLeft), 0);
                const int32_t top = std::max(static_cast<int32_t>(clipTop), 0);
                const int32_t right = std::min(static_cast<int32_t>(clipRight), static_cast<int32_t>(extent.width));
                const int32_t bottom = std::min(static_cast<int32_t>(clipBottom), static_cast<int32_t>(extent.height));

                if (right <= left || bottom <= top)
                {
                    continue;
                }

                recorder.setScissor(left, top, static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top));
                recorder.drawIndexed(command.ElemCount, indexOffset + command.IdxOffset,
                    static_cast<int32_t>(vertexOffset + command.VtxOffset));
            }

            indexOffset += static_cast<uint32_t>(list->IdxBuffer.Size);
            vertexOffset += static_cast<uint32_t>(list->VtxBuffer.Size);
        }
    }

    Status ImGuiOverlayPass::uploadDrawData(const ImDrawData& drawData)
    {
        m_vertices.clear();
        m_indices.clear();
        m_vertices.reserve(static_cast<std::size_t>(drawData.TotalVtxCount));
        m_indices.reserve(static_cast<std::size_t>(drawData.TotalIdxCount));

        for (const ImDrawList* list : drawData.CmdLists)
        {
            m_vertices.insert(m_vertices.end(), list->VtxBuffer.begin(), list->VtxBuffer.end());
            m_indices.insert(m_indices.end(), list->IdxBuffer.begin(), list->IdxBuffer.end());
        }

        if (auto uploaded = m_vertexBuffer.update(std::as_bytes(std::span{m_vertices})); !uploaded)
        {
            return render::toError(uploaded.error());
        }

        if (auto uploaded = m_indexBuffer.update(std::as_bytes(std::span{m_indices})); !uploaded)
        {
            return render::toError(uploaded.error());
        }

        return {};
    }
} //namespace lockstep::debug
