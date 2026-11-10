#ifndef LOCKSTEP_UI_UI_TYPES_HPP
#define LOCKSTEP_UI_UI_TYPES_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Platform/Input.hpp"
#include "Lockstep/Render/Text.hpp"

#include <optional>

namespace lockstep::ui
{
    // Height of the UI canvas in canvas units. The canvas is always this tall and as wide as the
    // window's aspect ratio makes it, so one unit is one pixel at 1080p and layouts keep their
    // proportions at any resolution (the same unit as render::TextStyle::size).
    inline constexpr float k_canvasHeight{1080.0f};

    // An axis-aligned rectangle on the UI canvas, in canvas units, (0,0) top-left.
    struct Rect
    {
        math::Vec2 min{0.0f, 0.0f};
        math::Vec2 max{0.0f, 0.0f};

        // Returns: max.x - min.x.
        [[nodiscard]] float width() const noexcept;

        // Returns: max.y - min.y.
        [[nodiscard]] float height() const noexcept;

        // Returns: the middle point.
        [[nodiscard]] math::Vec2 center() const noexcept;

        // point:   a canvas point.
        // Returns: true if the point is inside (edges included).
        [[nodiscard]] bool contains(math::Vec2 point) const noexcept;
    };

    // rect:    a rectangle.
    // amount:  distance to move every edge inwards (negative grows it).
    // Returns: the shrunk rectangle.
    [[nodiscard]] Rect inset(const Rect& rect, float amount) noexcept;

    // Builds a rectangle from its top-left corner and size.
    // position: top-left corner.
    // size:     width and height.
    // Returns:  the rectangle.
    [[nodiscard]] Rect rectAt(math::Vec2 position, math::Vec2 size) noexcept;

    // Builds a rectangle of a given size centred on a point.
    // center:  the centre.
    // size:    width and height.
    // Returns: the rectangle.
    [[nodiscard]] Rect rectAround(math::Vec2 center, math::Vec2 size) noexcept;

    // Stacks rows top to bottom inside an area, for menu layouts.
    class Column
    {
    public:
        // area:    the rectangle the rows fill from its top.
        // spacing: gap between rows, in canvas units.
        Column(const Rect& area, float spacing) noexcept;

        // Takes the next row.
        // height:  the row height.
        // Returns: the row's rectangle (full width of the area).
        [[nodiscard]] Rect next(float height) noexcept;

        // Leaves an empty gap.
        // height: the gap, in canvas units (the row spacing is not added).
        void skip(float height) noexcept;

        // Returns: the y coordinate where the next row starts.
        [[nodiscard]] float cursor() const noexcept;

    private:
        Rect m_area;
        float m_spacing{0.0f};
        float m_cursor{0.0f};
    };

    // Colours and sizes of the widgets (sRGB colours, sizes in canvas units).
    // font:          the font of every widget label and value.
    // textSize:      label and value text size.
    // text:          normal text.
    // textDim:       secondary text (hints, disabled values).
    // accent:        the focus marker and active values.
    // rowFill:       background of a widget row.
    // rowFocusFill:  background of the focused (or hovered) widget row.
    // frame:         thin outlines (slider tracks, text field boxes).
    // lineThickness: outline thickness.
    // padding:       space between a row's edge and its text.
    // textShadow:    draw a soft drop shadow under text (legibility over a busy background).
    struct Theme
    {
        render::TextFont font{render::TextFont::Mono};
        float textSize{26.0f};
        math::Vec4 text{0.86f, 0.85f, 0.80f, 1.0f};
        math::Vec4 textDim{0.55f, 0.55f, 0.52f, 1.0f};
        math::Vec4 accent{0.78f, 0.12f, 0.10f, 1.0f};
        math::Vec4 rowFill{0.0f, 0.0f, 0.0f, 0.0f};
        math::Vec4 rowFocusFill{0.86f, 0.85f, 0.80f, 0.08f};
        math::Vec4 frame{0.86f, 0.85f, 0.80f, 0.35f};
        float lineThickness{1.5f};
        float padding{16.0f};
        bool textShadow{true};
    };

    // A key or mouse button picked with UiContext::keyCapture(); exactly one is set.
    struct CapturedInput
    {
        std::optional<platform::Key> key;
        std::optional<platform::MouseButton> button;
    };
} //namespace lockstep::ui

#endif //LOCKSTEP_UI_UI_TYPES_HPP
