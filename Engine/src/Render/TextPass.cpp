#include "TextPass.hpp"

#include "Lockstep/Core/Log.hpp"

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
        constexpr std::string_view k_vertexShader{"text.vert.spv"};
        constexpr std::string_view k_fragmentShader{"text.frag.spv"};
        constexpr std::string_view k_fontFile{"NotoSans-Regular.ttf"};
        constexpr uint32_t k_atlasSlot{0};
        constexpr float k_referenceHeight{1080.0f};
        constexpr float k_shadowOffset{0.06f};
        constexpr float k_shadowOpacity{0.8f};
        constexpr float k_shadowSoftness{0.12f};
        constexpr char32_t k_replacementCodepoint{0xFFFD};
        constexpr std::size_t k_verticesPerQuad{4};
        constexpr std::size_t k_indicesPerQuad{6};

        struct TextConstants
        {
            float pixelToClip[2]{};
            float padding[2]{};
        };

        vela::graphics::VertexLayout textVertexLayout()
        {
            return vela::graphics::VertexLayout
            {
                sizeof(TextVertex),
                {
                    {0, vela::graphics::VertexAttributeFormat::Float2, static_cast<uint32_t>(offsetof(TextVertex, position))},
                    {1, vela::graphics::VertexAttributeFormat::Float2, static_cast<uint32_t>(offsetof(TextVertex, uv))},
                    {2, vela::graphics::VertexAttributeFormat::Unorm8x4, static_cast<uint32_t>(offsetof(TextVertex, color))},
                    {3, vela::graphics::VertexAttributeFormat::Float, static_cast<uint32_t>(offsetof(TextVertex, softness))}
                }
            };
        }

        vela::graphics::RenderState textRenderState()
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

        char32_t decodeUtf8(std::string_view text, std::size_t& cursor)
        {
            const auto lead = static_cast<unsigned char>(text[cursor++]);

            if (lead < 0x80)
            {
                return lead;
            }

            std::size_t continuationCount = 0;
            char32_t codepoint = 0;

            if ((lead & 0xE0) == 0xC0)
            {
                continuationCount = 1;
                codepoint = lead & 0x1F;
            }
            else if ((lead & 0xF0) == 0xE0)
            {
                continuationCount = 2;
                codepoint = lead & 0x0F;
            }
            else if ((lead & 0xF8) == 0xF0)
            {
                continuationCount = 3;
                codepoint = lead & 0x07;
            }
            else
            {
                return k_replacementCodepoint;
            }

            for (std::size_t index = 0; index < continuationCount; ++index)
            {
                if (cursor >= text.size() || (static_cast<unsigned char>(text[cursor]) & 0xC0) != 0x80)
                {
                    return k_replacementCodepoint;
                }

                codepoint = (codepoint << 6) | (static_cast<unsigned char>(text[cursor++]) & 0x3F);
            }

            return codepoint;
        }

        float lineWidth(const FontAtlas& atlas, std::string_view line)
        {
            float width = 0.0f;
            const AtlasGlyph* previous = nullptr;
            std::size_t cursor = 0;

            while (cursor < line.size())
            {
                const AtlasGlyph& glyph = atlas.glyph(decodeUtf8(line, cursor));

                if (previous != nullptr)
                {
                    width += atlas.kerning(*previous, glyph);
                }

                width += glyph.advance;
                previous = &glyph;
            }

            return width;
        }

        float alignFactor(TextAlign align)
        {
            switch (align)
            {
                case TextAlign::Left:
                {
                    return 0.0f;
                }
                case TextAlign::Center:
                {
                    return 0.5f;
                }
                case TextAlign::Right:
                {
                    return 1.0f;
                }
            }

            return 0.0f;
        }

        void appendQuad(std::vector<TextVertex>& vertices, const AtlasGlyph& glyph, float left, float top, float right,
            float bottom, uint32_t color, float softness)
        {
            vertices.push_back({{left, top}, {glyph.uvMin[0], glyph.uvMin[1]}, color, softness});
            vertices.push_back({{right, top}, {glyph.uvMax[0], glyph.uvMin[1]}, color, softness});
            vertices.push_back({{right, bottom}, {glyph.uvMax[0], glyph.uvMax[1]}, color, softness});
            vertices.push_back({{left, bottom}, {glyph.uvMin[0], glyph.uvMax[1]}, color, softness});
        }

        uint8_t toByte(float value)
        {
            return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
        }

        uint32_t packColor(const math::Vec4& color)
        {
            return static_cast<uint32_t>(toByte(color.x))
                | (static_cast<uint32_t>(toByte(color.y)) << 8)
                | (static_cast<uint32_t>(toByte(color.z)) << 16)
                | (static_cast<uint32_t>(toByte(color.w)) << 24);
        }
    } //namespace

    void TextPass::TextQueue::clear()
    {
        text.clear();
        entries.clear();
    }

    Result<std::unique_ptr<TextPass>> TextPass::create(vela::core::Context& context)
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

        auto atlas = FontAtlas::create(context, k_fontFile);

        if (!atlas)
        {
            return atlas.error();
        }

        auto material = vela::graphics::Material::create(context,
        {
            .vertexShader = vertexShader.value(),
            .fragmentShader = fragmentShader.value(),
            .vertexLayout = textVertexLayout(),
            .renderState = textRenderState()
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

        return std::unique_ptr<TextPass>(new TextPass(std::move(atlas).value(), std::move(material).value(),
            std::move(vertexBuffer).value(), std::move(indexBuffer).value()));
    }

    TextPass::TextPass(FontAtlas&& atlas, vela::graphics::Material&& material,
        vela::graphics::DynamicBuffer&& vertexBuffer, vela::graphics::DynamicBuffer&& indexBuffer)
        : m_atlas(std::move(atlas)),
          m_material(std::move(material)),
          m_vertexBuffer(std::move(vertexBuffer)),
          m_indexBuffer(std::move(indexBuffer))
    {
    }

    void TextPass::submit(std::string_view utf8, math::Vec2 position, const TextStyle& style)
    {
        if (utf8.empty() || style.size <= 0.0f || style.color.w <= 0.0f)
        {
            return;
        }

        m_queued.entries.push_back({.offset = m_queued.text.size(), .length = utf8.size(), .position = position,
            .style = style});
        m_queued.text.append(utf8);
    }

    void TextPass::prepareFrame()
    {
        std::swap(m_frame, m_queued);
        m_queued.clear();
    }

    void TextPass::layoutEntry(const TextEntry& entry, std::string_view text, float width, float height)
    {
        const float pixelSize = entry.style.size * height / k_referenceHeight;
        const float scale = pixelSize / FontAtlas::k_bakeSize;
        const float align = alignFactor(entry.style.align);
        const float shadowOffset = std::max(1.0f, std::round(pixelSize * k_shadowOffset));
        const uint32_t textColor = packColor(entry.style.color);
        const uint32_t shadowColor = packColor(math::Vec4(0.0f, 0.0f, 0.0f, entry.style.color.w * k_shadowOpacity));
        float baseline = entry.position.y * height + m_atlas.ascent() * scale;
        std::size_t lineStart = 0;

        while (lineStart <= text.size())
        {
            const std::size_t lineEnd = std::min(text.find('\n', lineStart), text.size());
            const std::string_view line = text.substr(lineStart, lineEnd - lineStart);
            const float alignShift = align > 0.0f ? lineWidth(m_atlas, line) * scale * align : 0.0f;

            float penX = std::round(entry.position.x * width - alignShift);
            const float penY = std::round(baseline);
            const AtlasGlyph* previous = nullptr;
            std::size_t cursor = 0;

            while (cursor < line.size())
            {
                const AtlasGlyph& glyph = m_atlas.glyph(decodeUtf8(line, cursor));

                if (previous != nullptr)
                {
                    penX += m_atlas.kerning(*previous, glyph) * scale;
                }

                if (glyph.offsetMax[0] > glyph.offsetMin[0])
                {
                    const float left = penX + glyph.offsetMin[0] * scale;
                    const float top = penY + glyph.offsetMin[1] * scale;
                    const float right = penX + glyph.offsetMax[0] * scale;
                    const float bottom = penY + glyph.offsetMax[1] * scale;

                    appendQuad(m_textVertices, glyph, left, top, right, bottom, textColor, 0.0f);

                    if (entry.style.shadow)
                    {
                        appendQuad(m_vertices, glyph, left + shadowOffset, top + shadowOffset, right + shadowOffset,
                            bottom + shadowOffset, shadowColor, k_shadowSoftness);
                    }
                }

                penX += glyph.advance * scale;
                previous = &glyph;
            }

            baseline += m_atlas.lineHeight() * scale;
            lineStart = lineEnd + 1;
        }
    }

    void TextPass::record(vela::graphics::PassRecorder& recorder)
    {
        const vela::graphics::Extent2D extent = recorder.extent();

        if (m_frame.entries.empty() || extent.width == 0 || extent.height == 0)
        {
            return;
        }

        const auto width = static_cast<float>(extent.width);
        const auto height = static_cast<float>(extent.height);

        m_vertices.clear();
        m_textVertices.clear();

        for (const TextEntry& entry : m_frame.entries)
        {
            layoutEntry(entry, std::string_view(m_frame.text).substr(entry.offset, entry.length), width, height);
        }

        m_vertices.insert(m_vertices.end(), m_textVertices.begin(), m_textVertices.end());

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
            log(LogLevel::Error, "Render", "Text vertex upload failed: " + toError(uploaded.error()).message);
            return;
        }

        if (auto uploaded = m_indexBuffer.update(std::as_bytes(std::span{m_indices})); !uploaded)
        {
            log(LogLevel::Error, "Render", "Text index upload failed: " + toError(uploaded.error()).message);
            return;
        }

        const TextConstants constants{.pixelToClip = {2.0f / width, 2.0f / height}};

        recorder.bind(m_material);
        recorder.bindVertexBuffer(m_vertexBuffer);
        recorder.bindIndexBuffer(m_indexBuffer);
        recorder.bindTexture(k_atlasSlot, m_atlas.texture());
        recorder.setConstants(constants);
        recorder.drawIndexed(static_cast<uint32_t>(m_indices.size()), 0, 0);
    }
} //namespace lockstep::render
