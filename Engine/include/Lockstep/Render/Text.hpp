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

    // How screen text looks (see Renderer::submitText()).
    // size:   pixel height of a line (ascender to descender) at 1080p; it scales with the window's
    //         framebuffer height, so text keeps its share of the screen at any resolution or DPI.
    // color:  sRGB colour as picked in an image editor; alpha is opacity.
    // align:  which end of each line sits at the position.
    // shadow: draws a soft dark drop shadow behind the text, for legibility over the scene.
    struct TextStyle
    {
        float size{24.0f};
        math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
        TextAlign align{TextAlign::Left};
        bool shadow{true};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_TEXT_HPP
