#include "OverlayPass.hpp"

#include "Lockstep/Core/Log.hpp"

#include "RendererState.hpp"
#include "ShaderLoader.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/RenderTypes.hpp>
#include <Vela/Graphics/VertexLayout.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_vertexShader{"overlay.vert.spv"};
        constexpr std::string_view k_fragmentShader{"overlay.frag.spv"};
        constexpr std::string_view k_sansFontFile{"NotoSans-Regular.ttf"};
        constexpr std::string_view k_monoFontFile{"Cousine-Regular.ttf"};
        constexpr uint32_t k_textureSlot{0};
        constexpr uint32_t k_verticesPerQuad{4};
        constexpr uint32_t k_indicesPerQuad{6};

        struct OverlayConstants
        {
            float pixelToClip[2]{};
            float padding[2]{};
        };

        vela::graphics::VertexLayout overlayVertexLayout()
        {
            return vela::graphics::VertexLayout
            {
                sizeof(OverlayVertex),
                {
                    {0, vela::graphics::VertexAttributeFormat::Float2, static_cast<uint32_t>(offsetof(OverlayVertex, position))},
                    {1, vela::graphics::VertexAttributeFormat::Float2, static_cast<uint32_t>(offsetof(OverlayVertex, uv))},
                    {2, vela::graphics::VertexAttributeFormat::Unorm8x4, static_cast<uint32_t>(offsetof(OverlayVertex, color))},
                    {3, vela::graphics::VertexAttributeFormat::Float, static_cast<uint32_t>(offsetof(OverlayVertex, softness))},
                    {4, vela::graphics::VertexAttributeFormat::Float, static_cast<uint32_t>(offsetof(OverlayVertex, shade))}
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

        Result<vela::graphics::DynamicBuffer> createBuffer(vela::core::Context& context,
            vela::graphics::DynamicBuffer::Usage usage)
        {
            auto buffer = vela::graphics::DynamicBuffer::create(context, usage, 0,
                vela::graphics::DynamicBuffer::IndexType::Uint32);

            if (!buffer)
            {
                return toError(buffer.error());
            }

            return std::move(buffer).value();
        }

        void snapToPixels(math::Vec2& min, math::Vec2& max)
        {
            min = math::Vec2{std::round(min.x), std::round(min.y)};
            max = math::Vec2{std::max(std::round(max.x), min.x + 1.0f), std::max(std::round(max.y), min.y + 1.0f)};
        }

        ScreenRect toPixels(const ScreenRect& rect, float width, float height)
        {
            return ScreenRect{.min = {rect.min.x * width, rect.min.y * height},
                .max = {rect.max.x * width, rect.max.y * height}};
        }
    } //namespace

    void OverlayPass::OverlayQueue::clear()
    {
        text.clear();
        items.clear();
        clips.clear();
        clips.push_back(ScreenRect{.min = {0.0f, 0.0f}, .max = {1.0f, 1.0f}});
        currentClip = 0;
    }

    Result<std::unique_ptr<OverlayPass>> OverlayPass::create(vela::core::Context& context, const RendererState& state)
    {
        auto vertexShader = loadShader(k_vertexShader);

        if (!vertexShader)
        {
            return vertexShader.error();
        }

        auto fragmentShader = loadShader(k_fragmentShader);

        if (!fragmentShader)
        {
            return fragmentShader.error();
        }

        auto sans = FontAtlas::create(context, k_sansFontFile);

        if (!sans)
        {
            return sans.error();
        }

        auto mono = FontAtlas::create(context, k_monoFontFile);

        if (!mono)
        {
            return mono.error();
        }

        auto material = vela::graphics::Material::create(context,
        {
            .vertexShader = vertexShader.value(),
            .fragmentShader = fragmentShader.value(),
            .vertexLayout = overlayVertexLayout(),
            .renderState = overlayRenderState()
        });

        if (!material)
        {
            return toError(material.error());
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

        return std::unique_ptr<OverlayPass>(new OverlayPass(state, std::move(sans).value(), std::move(mono).value(),
            std::move(material).value(), std::move(vertexBuffer).value(), std::move(indexBuffer).value()));
    }

    OverlayPass::OverlayPass(const RendererState& state, FontAtlas&& sans, FontAtlas&& mono,
        vela::graphics::Material&& material, vela::graphics::DynamicBuffer&& vertexBuffer,
        vela::graphics::DynamicBuffer&& indexBuffer)
        : m_state(state),
          m_sans(std::move(sans)),
          m_mono(std::move(mono)),
          m_material(std::move(material)),
          m_vertexBuffer(std::move(vertexBuffer)),
          m_indexBuffer(std::move(indexBuffer))
    {
        m_queued.clear();
        m_frame.clear();
    }

    const FontAtlas& OverlayPass::atlas(TextFont font) const
    {
        return font == TextFont::Mono ? m_mono : m_sans;
    }

    void OverlayPass::submitText(std::string_view utf8, math::Vec2 position, const TextStyle& style)
    {
        if (utf8.empty() || style.size <= 0.0f || style.color.w <= 0.0f)
        {
            return;
        }

        m_queued.items.push_back(OverlayItem{.kind = ItemKind::Text, .textOffset = m_queued.text.size(),
            .textLength = utf8.size(), .position = position, .style = style, .clip = m_queued.currentClip});
        m_queued.text.append(utf8);
    }

    void OverlayPass::submitRect(const ScreenRect& rect, const math::Vec4& color)
    {
        if (color.w <= 0.0f || rect.max.x <= rect.min.x || rect.max.y <= rect.min.y)
        {
            return;
        }

        m_queued.items.push_back(OverlayItem{.kind = ItemKind::Rect, .rect = rect, .color = color,
            .clip = m_queued.currentClip});
    }

    void OverlayPass::submitImage(TextureHandle texture, const ScreenRect& rect, const ImageStyle& style)
    {
        if (!texture.isValid() || style.tint.w <= 0.0f || rect.max.x <= rect.min.x || rect.max.y <= rect.min.y)
        {
            return;
        }

        m_queued.items.push_back(OverlayItem{.kind = ItemKind::Image, .rect = rect, .texture = texture, .image = style,
            .clip = m_queued.currentClip});
    }

    void OverlayPass::setClip(const std::optional<ScreenRect>& clip)
    {
        if (!clip)
        {
            m_queued.currentClip = 0;
            return;
        }

        m_queued.currentClip = static_cast<uint32_t>(m_queued.clips.size());
        m_queued.clips.push_back(*clip);
    }

    math::Vec2 OverlayPass::measureText(std::string_view utf8, const TextStyle& style) const
    {
        return measureTextBlock(atlas(style.font), utf8, style.size);
    }

    void OverlayPass::prepareFrame()
    {
        std::swap(m_frame, m_queued);
        m_queued.clear();
    }

    std::optional<const vela::graphics::Texture*> OverlayPass::appendItem(const OverlayItem& item, float width,
        float height)
    {
        switch (item.kind)
        {
            case ItemKind::Text:
            {
                const FontAtlas& font = atlas(item.style.font);
                layoutText(font, TextBlock{.text = std::string_view(m_frame.text).substr(item.textOffset, item.textLength),
                    .position = item.position, .style = item.style}, width, height, m_vertices);
                return &font.texture();
            }
            case ItemKind::Rect:
            {
                const ScreenRect pixels = toPixels(item.rect, width, height);
                math::Vec2 min = pixels.min;
                math::Vec2 max = pixels.max;
                snapToPixels(min, max);
                appendOverlayQuad(m_vertices, min, max, {0.0f, 0.0f}, {0.0f, 0.0f}, packOverlayColor(item.color), 0.0f,
                    OverlayShade::Solid);
                return nullptr;
            }
            case ItemKind::Image:
            {
                const TextureRecord* texture = m_state.textures.find(item.texture);

                if (texture == nullptr)
                {
                    return std::nullopt;
                }

                const ScreenRect pixels = toPixels(item.rect, width, height);
                math::Vec2 min = pixels.min;
                math::Vec2 max = pixels.max;
                snapToPixels(min, max);
                appendOverlayQuad(m_vertices, min, max, item.image.uvMin, item.image.uvMax,
                    packOverlayColor(item.image.tint), 0.0f, OverlayShade::Image);
                return &texture->texture;
            }
        }

        return std::nullopt;
    }

    void OverlayPass::addToBatches(const vela::graphics::Texture* texture, uint32_t clip, uint32_t firstQuad)
    {
        const auto quadCount = static_cast<uint32_t>(m_vertices.size() / k_verticesPerQuad) - firstQuad;

        if (quadCount == 0)
        {
            return;
        }

        if (!m_batches.empty())
        {
            DrawBatch& last = m_batches.back();
            const bool isTextureCompatible = texture == nullptr || last.texture == nullptr || last.texture == texture;

            if (last.clip == clip && isTextureCompatible)
            {
                last.texture = last.texture != nullptr ? last.texture : texture;
                last.quadCount += quadCount;
                return;
            }
        }

        m_batches.push_back(DrawBatch{.texture = texture, .clip = clip, .firstQuad = firstQuad, .quadCount = quadCount});
    }

    void OverlayPass::record(vela::graphics::PassRecorder& recorder)
    {
        const vela::graphics::Extent2D extent = recorder.extent();

        if (m_frame.items.empty() || extent.width == 0 || extent.height == 0)
        {
            return;
        }

        const auto width = static_cast<float>(extent.width);
        const auto height = static_cast<float>(extent.height);
        m_vertices.clear();
        m_batches.clear();

        for (const OverlayItem& item : m_frame.items)
        {
            const auto firstQuad = static_cast<uint32_t>(m_vertices.size() / k_verticesPerQuad);

            if (const std::optional<const vela::graphics::Texture*> texture = appendItem(item, width, height))
            {
                addToBatches(*texture, item.clip, firstQuad);
            }
        }

        if (m_vertices.empty())
        {
            return;
        }

        const auto quadCount = static_cast<uint32_t>(m_vertices.size() / k_verticesPerQuad);
        m_indices.clear();
        m_indices.reserve(static_cast<std::size_t>(quadCount) * k_indicesPerQuad);

        for (uint32_t quad = 0; quad < quadCount; ++quad)
        {
            const uint32_t first = quad * k_verticesPerQuad;
            m_indices.insert(m_indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
        }

        if (auto uploaded = m_vertexBuffer.update(std::as_bytes(std::span{m_vertices})); !uploaded)
        {
            log(LogLevel::Error, "Render", "Overlay vertex upload failed: " + toError(uploaded.error()).message);
            return;
        }

        if (auto uploaded = m_indexBuffer.update(std::as_bytes(std::span{m_indices})); !uploaded)
        {
            log(LogLevel::Error, "Render", "Overlay index upload failed: " + toError(uploaded.error()).message);
            return;
        }

        const OverlayConstants constants{.pixelToClip = {2.0f / width, 2.0f / height}};

        recorder.bind(m_material);
        recorder.bindVertexBuffer(m_vertexBuffer);
        recorder.bindIndexBuffer(m_indexBuffer);
        recorder.setConstants(constants);

        for (const DrawBatch& batch : m_batches)
        {
            const ScreenRect clip = toPixels(m_frame.clips[batch.clip], width, height);
            const auto left = static_cast<int32_t>(std::clamp(std::floor(clip.min.x), 0.0f, width));
            const auto top = static_cast<int32_t>(std::clamp(std::floor(clip.min.y), 0.0f, height));
            const auto right = static_cast<int32_t>(std::clamp(std::ceil(clip.max.x), 0.0f, width));
            const auto bottom = static_cast<int32_t>(std::clamp(std::ceil(clip.max.y), 0.0f, height));

            if (right <= left || bottom <= top)
            {
                continue;
            }

            recorder.bindTexture(k_textureSlot, batch.texture != nullptr ? *batch.texture : m_sans.texture());
            recorder.setScissor(left, top, static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top));
            recorder.drawIndexed(batch.quadCount * k_indicesPerQuad, batch.firstQuad * k_indicesPerQuad, 0);
        }
    }
} //namespace lockstep::render
