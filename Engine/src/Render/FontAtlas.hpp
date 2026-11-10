#ifndef LOCKSTEP_RENDER_FONT_ATLAS_HPP
#define LOCKSTEP_RENDER_FONT_ATLAS_HPP

#include "Lockstep/Core/Result.hpp"

#include <Vela/Core/Context.hpp>
#include <Vela/Graphics/Texture.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace lockstep::render
{
    // Where one glyph sits in the atlas and how it is placed, in pixels at the atlas's bake size.
    // uvMin, uvMax: the glyph's rectangle in the atlas (0..1), padding included.
    // offsetMin:    top-left corner of that rectangle relative to the pen on the baseline (y down).
    // offsetMax:    bottom-right corner, likewise.
    // advance:      how far the pen moves after the glyph.
    // glyphIndex:   the font's glyph index, for kerning.
    struct AtlasGlyph
    {
        float uvMin[2]{};
        float uvMax[2]{};
        float offsetMin[2]{};
        float offsetMax[2]{};
        float advance{0.0f};
        int glyphIndex{0};
    };

    // A signed distance field font atlas baked once from a TTF: Basic Latin, Latin-1, Latin
    // Extended-A, Cyrillic and common punctuation (dashes, quotes, ellipsis, guillemets, No. sign).
    // The single-channel texture holds k_fontEdgeValue on glyph outlines, more inside, less
    // outside, falling by one unit per k_fontDistanceScale over a pixel at the bake size; it can be
    // drawn at any size with a smoothstep around the edge (overlay.frag).
    class FontAtlas
    {
    public:
        // Pixel height (ascender to descender) the glyphs are baked at.
        static constexpr float k_bakeSize{48.0f};

        // Distance-field value on a glyph outline, out of 255 (overlay.frag uses the same value).
        static constexpr uint8_t k_fontEdgeValue{128};

        // Distance-field units per pixel at the bake size, out of 255.
        static constexpr float k_fontDistanceScale{16.0f};

        // Loads a TTF staged next to the executable (in "<executable dir>/fonts/") and bakes the
        // atlas texture.
        // context:  Vela context that owns the texture.
        // fileName: the font file name, e.g. "NotoSans-Regular.ttf".
        // Returns:  the atlas, or an error if the file is missing, not a font, lacks '?' or the
        //           texture could not be created.
        [[nodiscard]] static Result<FontAtlas> create(vela::core::Context& context, std::string_view fileName);

        ~FontAtlas();

        FontAtlas(FontAtlas&& other) noexcept;
        FontAtlas& operator=(FontAtlas&& other) noexcept;

        FontAtlas(const FontAtlas&) = delete;
        FontAtlas& operator=(const FontAtlas&) = delete;

        // Finds the glyph of a character.
        // codepoint: the Unicode code point.
        // Returns:   its glyph, or '?' for characters the atlas does not hold.
        [[nodiscard]] const AtlasGlyph& glyph(char32_t codepoint) const;

        // Extra pen movement between two glyphs (negative pulls them together).
        // left:    the earlier glyph.
        // right:   the glyph after it.
        // Returns: the kerning in pixels at the bake size.
        [[nodiscard]] float kerning(const AtlasGlyph& left, const AtlasGlyph& right) const;

        // Returns: distance from the top of a line to its baseline, in pixels at the bake size.
        [[nodiscard]] float ascent() const;

        // Returns: distance from one baseline to the next, in pixels at the bake size.
        [[nodiscard]] float lineHeight() const;

        // Returns: the atlas texture (R8, linear filtering).
        [[nodiscard]] const vela::graphics::Texture& texture() const;

    private:
        struct FontData;

        FontAtlas(std::unique_ptr<FontData> font, vela::graphics::Texture&& texture);

        std::unique_ptr<FontData> m_font;
        vela::graphics::Texture m_texture;
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_FONT_ATLAS_HPP
