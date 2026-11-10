#include "TextLayout.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::render
{
    namespace
    {
        constexpr float k_referenceHeight{1080.0f};
        constexpr float k_shadowOffset{0.06f};
        constexpr float k_shadowOpacity{0.8f};
        constexpr float k_shadowSoftness{0.12f};
        constexpr char32_t k_replacementCodepoint{0xFFFD};

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

        uint8_t toByte(float value)
        {
            return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
        }

        void appendGlyph(std::vector<OverlayVertex>& vertices, const AtlasGlyph& glyph, math::Vec2 min, math::Vec2 max,
            uint32_t color, float softness)
        {
            appendOverlayQuad(vertices, min, max, {glyph.uvMin[0], glyph.uvMin[1]}, {glyph.uvMax[0], glyph.uvMax[1]},
                color, softness, OverlayShade::Glyph);
        }

        void layoutPass(const FontAtlas& atlas, const TextBlock& block, float width, float height, math::Vec2 offset,
            uint32_t color, float softness, std::vector<OverlayVertex>& vertices)
        {
            const float pixelSize = block.style.size * height / k_referenceHeight;
            const float scale = pixelSize / FontAtlas::k_bakeSize;
            const float align = alignFactor(block.style.align);
            const std::string_view text = block.text;
            float baseline = block.position.y * height + atlas.ascent() * scale;
            std::size_t lineStart = 0;

            while (lineStart <= text.size())
            {
                const std::size_t lineEnd = std::min(text.find('\n', lineStart), text.size());
                const std::string_view line = text.substr(lineStart, lineEnd - lineStart);
                const float alignShift = align > 0.0f ? lineWidth(atlas, line) * scale * align : 0.0f;

                float penX = std::round(block.position.x * width - alignShift);
                const float penY = std::round(baseline);
                const AtlasGlyph* previous = nullptr;
                std::size_t cursor = 0;

                while (cursor < line.size())
                {
                    const AtlasGlyph& glyph = atlas.glyph(decodeUtf8(line, cursor));

                    if (previous != nullptr)
                    {
                        penX += atlas.kerning(*previous, glyph) * scale;
                    }

                    if (glyph.offsetMax[0] > glyph.offsetMin[0])
                    {
                        const math::Vec2 min{penX + glyph.offsetMin[0] * scale + offset.x,
                            penY + glyph.offsetMin[1] * scale + offset.y};
                        const math::Vec2 max{penX + glyph.offsetMax[0] * scale + offset.x,
                            penY + glyph.offsetMax[1] * scale + offset.y};
                        appendGlyph(vertices, glyph, min, max, color, softness);
                    }

                    penX += glyph.advance * scale;
                    previous = &glyph;
                }

                baseline += atlas.lineHeight() * scale;
                lineStart = lineEnd + 1;
            }
        }
    } //namespace

    uint32_t packOverlayColor(const math::Vec4& color)
    {
        return static_cast<uint32_t>(toByte(color.x))
            | (static_cast<uint32_t>(toByte(color.y)) << 8)
            | (static_cast<uint32_t>(toByte(color.z)) << 16)
            | (static_cast<uint32_t>(toByte(color.w)) << 24);
    }

    void appendOverlayQuad(std::vector<OverlayVertex>& vertices, math::Vec2 min, math::Vec2 max, math::Vec2 uvMin,
        math::Vec2 uvMax, uint32_t color, float softness, OverlayShade shade)
    {
        const auto shadeValue = static_cast<float>(shade);
        vertices.push_back({{min.x, min.y}, {uvMin.x, uvMin.y}, color, softness, shadeValue});
        vertices.push_back({{max.x, min.y}, {uvMax.x, uvMin.y}, color, softness, shadeValue});
        vertices.push_back({{max.x, max.y}, {uvMax.x, uvMax.y}, color, softness, shadeValue});
        vertices.push_back({{min.x, max.y}, {uvMin.x, uvMax.y}, color, softness, shadeValue});
    }

    void layoutText(const FontAtlas& atlas, const TextBlock& block, float width, float height,
        std::vector<OverlayVertex>& vertices)
    {
        if (block.style.shadow)
        {
            const float pixelSize = block.style.size * height / k_referenceHeight;
            const float shadowOffset = std::max(1.0f, std::round(pixelSize * k_shadowOffset));
            const uint32_t shadowColor = packOverlayColor(
                math::Vec4(0.0f, 0.0f, 0.0f, block.style.color.w * k_shadowOpacity));
            layoutPass(atlas, block, width, height, {shadowOffset, shadowOffset}, shadowColor, k_shadowSoftness,
                vertices);
        }

        layoutPass(atlas, block, width, height, {0.0f, 0.0f}, packOverlayColor(block.style.color), 0.0f, vertices);
    }

    math::Vec2 measureTextBlock(const FontAtlas& atlas, std::string_view text, float size)
    {
        const float scale = size / FontAtlas::k_bakeSize;
        float widest = 0.0f;
        std::size_t lines = 0;
        std::size_t lineStart = 0;

        while (lineStart <= text.size())
        {
            const std::size_t lineEnd = std::min(text.find('\n', lineStart), text.size());
            widest = std::max(widest, lineWidth(atlas, text.substr(lineStart, lineEnd - lineStart)));
            ++lines;
            lineStart = lineEnd + 1;
        }

        const float extraLines = static_cast<float>(lines > 0 ? lines - 1 : 0);

        return math::Vec2{widest * scale, size + extraLines * atlas.lineHeight() * scale};
    }
} //namespace lockstep::render
