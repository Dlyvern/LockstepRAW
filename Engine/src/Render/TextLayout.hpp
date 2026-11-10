#ifndef LOCKSTEP_RENDER_TEXT_LAYOUT_HPP
#define LOCKSTEP_RENDER_TEXT_LAYOUT_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Render/Text.hpp"

#include "FontAtlas.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace lockstep::render
{
    // How an overlay vertex is shaded (overlay.frag).
    // Glyph: signed distance field coverage from the bound font atlas.
    // Solid: the vertex colour as is.
    // Image: the bound texture times the vertex colour.
    enum class OverlayShade : uint8_t
    {
        Glyph = 0,
        Solid,
        Image
    };

    // One corner of an overlay quad as overlay.vert reads it.
    // position: framebuffer pixels, (0,0) top-left.
    // uv:       texture coordinates.
    // color:    sRGB RGBA8 (r in the lowest byte); overlay.vert converts it to linear.
    // softness: glyphs only: half-width of the edge smoothstep in distance-field units (0..1); 0
    //           means one screen pixel, for crisp text. Shadows use a wider value.
    // shade:    an OverlayShade value.
    struct OverlayVertex
    {
        float position[2]{};
        float uv[2]{};
        uint32_t color{0};
        float softness{0.0f};
        float shade{0.0f};
    };

    // Text of one submitText() call, ready to lay out.
    // text:     the UTF-8 characters.
    // position: normalized screen position of the anchor.
    // style:    size, colour, alignment, shadow and font.
    struct TextBlock
    {
        std::string_view text;
        math::Vec2 position{};
        TextStyle style{};
    };

    // Packs an sRGB colour into the RGBA8 of OverlayVertex::color.
    // color:   the colour, components clamped to 0..1.
    // Returns: the packed colour.
    [[nodiscard]] uint32_t packOverlayColor(const math::Vec4& color);

    // Appends one quad's four corners.
    // vertices: the list to append to.
    // min, max: top-left and bottom-right corners in framebuffer pixels.
    // uvMin, uvMax: texture coordinates at those corners.
    // color:    packed colour.
    // softness: glyph edge softness (see OverlayVertex).
    // shade:    how it is shaded.
    void appendOverlayQuad(std::vector<OverlayVertex>& vertices, math::Vec2 min, math::Vec2 max, math::Vec2 uvMin,
        math::Vec2 uvMax, uint32_t color, float softness, OverlayShade shade);

    // Lays a text block out for a target size: its shadow quads (if the style has a shadow), then
    // its glyph quads, appended to `vertices`.
    // atlas:    the block's font.
    // block:    the text and style.
    // width:    target width in pixels.
    // height:   target height in pixels.
    // vertices: the list to append to.
    void layoutText(const FontAtlas& atlas, const TextBlock& block, float width, float height,
        std::vector<OverlayVertex>& vertices);

    // Measures text in pixels at 1080p.
    // atlas:   the font.
    // text:    UTF-8 text; '\n' starts a new line.
    // size:    TextStyle::size.
    // Returns: width of the widest line and height of all lines.
    [[nodiscard]] math::Vec2 measureTextBlock(const FontAtlas& atlas, std::string_view text, float size);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_TEXT_LAYOUT_HPP
