#ifndef LOCKSTEP_RENDER_TEXT_HPP
#define LOCKSTEP_RENDER_TEXT_HPP

#include "Lockstep/Core/Math.hpp"

#include <cstdint>

namespace lockstep::render
{
    // Which point of a text line its position is: the left end, the middle or the right end.
    // Every line of a multi-line text is aligned on its own.
    enum class TextAlign : uint8_t
    {
        Left,
        Center,
        Right
    };

    // Which Engine font screen text uses.
    // Sans: Noto Sans, the default proportional font.
    // Mono: Cousine, a monospaced font (terminal and readout looks).
    enum class TextFont : uint8_t
    {
        Sans,
        Mono
    };

    // How screen text looks (see Renderer::submitText()).
    // size:   pixel height of a line (ascender to descender) at 1080p; it scales with the window's
    //         framebuffer height, so text keeps its share of the screen at any resolution or DPI.
    // color:  sRGB colour as picked in an image editor; alpha is opacity.
    // align:  which end of each line sits at the position.
    // shadow: draws a soft dark drop shadow behind the text, for legibility over the scene.
    // font:   the font.
    struct TextStyle
    {
        float size{24.0f};
        math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
        TextAlign align{TextAlign::Left};
        bool shadow{true};
        TextFont font{TextFont::Sans};
    };

    // An axis-aligned screen rectangle in normalized screen coordinates, (0,0) top-left, (1,1)
    // bottom-right. Edges are snapped to whole framebuffer pixels when drawn, and a rectangle
    // thinner than a pixel is drawn one pixel wide, so thin lines stay crisp at any resolution.
    struct ScreenRect
    {
        math::Vec2 min{0.0f, 0.0f};
        math::Vec2 max{0.0f, 0.0f};
    };

    // How a screen image is drawn (see Renderer::submitImage()).
    // tint:  sRGB colour multiplied with the texture; alpha scales its opacity.
    // uvMin: texture coordinates at the rectangle's top-left corner.
    // uvMax: texture coordinates at its bottom-right corner.
    struct ImageStyle
    {
        math::Vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
        math::Vec2 uvMin{0.0f, 0.0f};
        math::Vec2 uvMax{1.0f, 1.0f};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_TEXT_HPP
