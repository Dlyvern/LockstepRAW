#include "ImGuiOverlayPass.hpp"

#include "Lockstep/Core/Log.hpp"
#include "Lockstep/Core/Math.hpp"

#include "Render/ShaderLoader.hpp"
#include "Render/VelaConversions.hpp"

#include <Vela/Graphics/ImageData.hpp>
#include <Vela/Graphics/RenderTypes.hpp>
#include <Vela/Graphics/VertexLayout.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

namespace lockstep::debug
{
    namespace
    {
        constexpr std::string_view k_vertexShader{"imgui.vert.spv"};
        constexpr std::string_view k_fragmentShader{"imgui.frag.spv"};
        constexpr uint32_t k_textureSlot{0};
        constexpr std::size_t k_rgbaBytes{4};

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

        vela::graphics::ImageData toImageData(ImTextureData& texture, std::vector<std::byte>& expanded)
        {
            const std::size_t pixelCount = static_cast<std::size_t>(texture.Width) * static_cast<std::size_t>(texture.Height);
            const auto* source = static_cast<const std::byte*>(texture.GetPixels());

            vela::graphics::ImageData image{};
            image.width = static_cast<uint32_t>(texture.Width);
            image.height = static_cast<uint32_t>(texture.Height);
            image.format = vela::graphics::TextureFormat::RGBA8Unorm;

            if (texture.Format == ImTextureFormat_RGBA32)
            {
                image.pixels = std::span<const std::byte>(source, pixelCount * k_rgbaBytes);
                return image;
            }

            expanded.assign(pixelCount * k_rgbaBytes, std::byte{0xFF});

            for (std::size_t pixel = 0; pixel < pixelCount; ++pixel)
            {
                expanded[pixel * k_rgbaBytes + 3] = source[pixel];
            }

            image.pixels = expanded;

            return image;
        }

        vela::graphics::SamplerDescription imguiSampler()
        {
            vela::graphics::SamplerDescription sampler{};
            sampler.addressU = vela::graphics::AddressMode::ClampToEdge;
            sampler.addressV = vela::graphics::AddressMode::ClampToEdge;
            sampler.addressW = vela::graphics::AddressMode::ClampToEdge;
            sampler.maxLod = 0.0f;

            return sampler;
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

        return std::unique_ptr<ImGuiOverlayPass>(new ImGuiOverlayPass(context, *imguiContext,
            std::move(vertexShader).value(), std::move(fragmentShader).value(), std::move(material).value(),
            std::move(vertexBuffer).value(), std::move(indexBuffer).value()));
    }

    ImGuiOverlayPass::ImGuiOverlayPass(vela::core::Context& context, ImGuiContext& imguiContext,
        std::vector<uint32_t>&& vertexShader, std::vector<uint32_t>&& fragmentShader, vela::graphics::Material&& material,
        vela::graphics::DynamicBuffer&& vertexBuffer, vela::graphics::DynamicBuffer&& indexBuffer)
        : m_context(&context),
          m_imguiContext(&imguiContext),
          m_vertexShader(std::move(vertexShader)),
          m_fragmentShader(std::move(fragmentShader)),
          m_material(std::move(material)),
          m_vertexBuffer(std::move(vertexBuffer)),
          m_indexBuffer(std::move(indexBuffer))
    {
    }

    ImGuiOverlayPass::~ImGuiOverlayPass()
    {
        ImGui::SetCurrentContext(m_imguiContext);

        for (ImTextureData* texture : ImGui::GetPlatformIO().Textures)
        {
            if (texture->RefCount == 1 && texture->Status != ImTextureStatus_Destroyed)
            {
                texture->SetTexID(ImTextureID_Invalid);
                texture->SetStatus(ImTextureStatus_Destroyed);
            }
        }
    }

    Status ImGuiOverlayPass::handleTextureRequest(ImTextureData& texture)
    {
        std::vector<std::byte> expanded;

        if (texture.Status == ImTextureStatus_WantCreate)
        {
            auto created = vela::graphics::Texture::create(*m_context, toImageData(texture, expanded), imguiSampler());

            if (!created)
            {
                return render::toError(created.error());
            }

            m_textures.push_back(std::make_unique<vela::graphics::Texture>(std::move(created).value()));
            texture.SetTexID(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(m_textures.back().get())));
            texture.SetStatus(ImTextureStatus_OK);
        }
        else if (texture.Status == ImTextureStatus_WantUpdates)
        {
            auto* gpuTexture = reinterpret_cast<vela::graphics::Texture*>(static_cast<uintptr_t>(texture.GetTexID()));

            if (auto updated = gpuTexture->update(toImageData(texture, expanded)); !updated)
            {
                return render::toError(updated.error());
            }

            texture.SetStatus(ImTextureStatus_OK);
        }
        else if (texture.Status == ImTextureStatus_WantDestroy)
        {
            const auto* gpuTexture = reinterpret_cast<const vela::graphics::Texture*>(
                static_cast<uintptr_t>(texture.GetTexID()));

            std::erase_if(m_textures, [gpuTexture](const std::unique_ptr<vela::graphics::Texture>& owned)
            {
                return owned.get() == gpuTexture;
            });

            texture.SetTexID(ImTextureID_Invalid);
            texture.SetStatus(ImTextureStatus_Destroyed);
        }

        return {};
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

        ImDrawData* drawData = ImGui::GetDrawData();

        if (drawData == nullptr)
        {
            return;
        }

        if (drawData->Textures != nullptr)
        {
            for (ImTextureData* texture : *drawData->Textures)
            {
                if (texture->Status == ImTextureStatus_OK)
                {
                    continue;
                }

                if (auto handled = handleTextureRequest(*texture); !handled)
                {
                    log(LogLevel::Error, "DevTools", "ImGui texture request failed: " + handled.error().message);
                }
            }
        }

        m_hasDrawData = drawData->TotalIdxCount > 0;
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
            log(LogLevel::Error, "DevTools", uploaded.error().message);
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
                const ImTextureID textureId = command.GetTexID();

                if (command.UserCallback != nullptr || textureId == ImTextureID_Invalid)
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

                recorder.bindTexture(k_textureSlot,
                    *reinterpret_cast<const vela::graphics::Texture*>(static_cast<uintptr_t>(textureId)));
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
