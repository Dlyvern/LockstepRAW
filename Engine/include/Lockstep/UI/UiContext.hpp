#ifndef LOCKSTEP_UI_UI_CONTEXT_HPP
#define LOCKSTEP_UI_UI_CONTEXT_HPP

#include "Lockstep/Core/Math.hpp"
#include "Lockstep/Platform/Input.hpp"
#include "Lockstep/Render/Resources.hpp"
#include "Lockstep/Render/Text.hpp"
#include "Lockstep/UI/UiTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace lockstep::ui
{
    // Immediate-mode screen UI drawn with the Renderer's screen overlay (no ImGui, so it works in
    // shipping builds). Each frame: begin(), draw and call widgets, end(). Widgets are identified
    // by a string id unique within the frame and laid out on the UI canvas (see k_canvasHeight).
    //
    // Navigation: the mouse hovers and clicks; the keyboard moves focus with Up / Down (or Tab /
    // Shift+Tab) through the widgets in the order they were called last frame, activates with
    // Enter or Space and changes sliders and choices with Left / Right. Hovering with a moving
    // mouse moves the focus too, so there is only ever one highlighted widget. Escape is left to
    // the caller (going back) except while a key capture waits, which it cancels.
    class UiContext
    {
    public:
        // Starts a UI frame.
        // renderer:     the renderer the frame draws into (kept until end()).
        // input:        this frame's input; ignored while `acceptsInput` is false.
        // frameSeconds: real time since the previous frame (caret blink, key repeat smoothing).
        // acceptsInput: false draws without reacting to input (a screen fading out).
        void begin(render::Renderer& renderer, const platform::Input& input, float frameSeconds,
            bool acceptsInput = true);

        // Ends the frame: keeps this frame's widget order for next frame's keyboard navigation.
        void end();

        // Returns: the canvas size, k_canvasHeight tall and as wide as the window's aspect ratio.
        [[nodiscard]] math::Vec2 canvasSize() const noexcept;

        // Returns: the colours and sizes widgets use; may be changed between widgets.
        [[nodiscard]] Theme& theme() noexcept;

        // Multiplies the alpha of everything drawn afterwards (fades between screens).
        // opacity: 0 (invisible) to 1.
        void setOpacity(float opacity) noexcept;

        // Fills a rectangle.
        // rect:  the rectangle.
        // color: sRGB colour.
        void fillRect(const Rect& rect, const math::Vec4& color);

        // Outlines a rectangle (the outline lies inside it).
        // rect:      the rectangle.
        // color:     sRGB colour.
        // thickness: line width in canvas units (at least one pixel is drawn).
        void strokeRect(const Rect& rect, const math::Vec4& color, float thickness);

        // Draws text.
        // utf8:     the text; '\n' starts a new line.
        // position: the anchor (x by `align`, y the top of the first line).
        // size:     text height in canvas units.
        // color:    sRGB colour.
        // align:    which end of each line is at position.x.
        // font:     the font; nothing uses the theme's.
        void text(std::string_view utf8, math::Vec2 position, float size, const math::Vec4& color,
            render::TextAlign align = render::TextAlign::Left, std::optional<render::TextFont> font = std::nullopt);

        // Measures text.
        // utf8:    the text.
        // size:    text height in canvas units.
        // font:    the font; nothing uses the theme's.
        // Returns: width of the widest line and height of all lines, in canvas units.
        [[nodiscard]] math::Vec2 measureText(std::string_view utf8, float size,
            std::optional<render::TextFont> font = std::nullopt) const;

        // Draws a texture into a rectangle.
        // texture: a renderer texture.
        // rect:    the rectangle.
        // style:   tint and texture coordinates.
        void image(render::TextureHandle texture, const Rect& rect, const render::ImageStyle& style = {});

        // Limits later drawing and mouse hits to a rectangle (intersected with the current limit).
        // rect: the visible rectangle.
        void pushClip(const Rect& rect);

        // Undoes the last pushClip().
        void popClip();

        // A push button: the label in the row, highlighted while focused.
        // id:      unique id.
        // rect:    the row.
        // label:   the text.
        // Returns: true on the frame it is clicked or activated.
        bool button(std::string_view id, const Rect& rect, std::string_view label);

        // An on / off switch: the label left, ON or OFF right.
        // id:      unique id.
        // rect:    the row.
        // label:   the text.
        // value:   the state, flipped on activation, Left or Right.
        // Returns: true on the frame it changed.
        bool toggle(std::string_view id, const Rect& rect, std::string_view label, bool& value);

        // A horizontal slider: the label left, a track and the value text right. Dragging sets the
        // value; Left / Right step it.
        // id:        unique id.
        // rect:      the row.
        // label:     the text.
        // value:     the value, clamped to [minimum, maximum].
        // minimum:   the left end.
        // maximum:   the right end.
        // step:      the keyboard step (and the snap of dragged values when > 0).
        // valueText: what is shown as the value.
        // Returns:   true on the frame it changed.
        bool slider(std::string_view id, const Rect& rect, std::string_view label, float& value, float minimum,
            float maximum, float step, std::string_view valueText);

        // A choice among options, shown as "< option >": Left / Right or clicking the arrows step
        // through them, activation steps forward (wrapping).
        // id:      unique id.
        // rect:    the row.
        // label:   the text.
        // index:   the chosen option, kept below options.size().
        // options: the option names. Must not be empty.
        // Returns: true on the frame it changed.
        bool cycler(std::string_view id, const Rect& rect, std::string_view label, std::size_t& index,
            std::span<const std::string> options);

        // A one-line text field: while focused, typed characters are added and Backspace removes
        // the last one.
        // id:        unique id.
        // rect:      the row.
        // label:     the text left of the field.
        // text:      the edited text.
        // maxLength: most characters kept.
        // allowed:   the characters accepted; empty accepts any printable ASCII character.
        // Returns:   true on the frame the text changed.
        bool textField(std::string_view id, const Rect& rect, std::string_view label, std::string& text,
            std::size_t maxLength, std::string_view allowed = {});

        // A key binding: shows `current`; activated, it waits for the next key or mouse button
        // (Escape cancels) and returns it.
        // id:      unique id.
        // rect:    the row.
        // label:   the action name.
        // current: the bound input's name.
        // Returns: the picked key or button on the frame it was picked.
        std::optional<CapturedInput> keyCapture(std::string_view id, const Rect& rect, std::string_view label,
            std::string_view current);

        // Starts a vertically scrolling area: later drawing is clipped to it and moved up by its
        // scroll offset; the mouse wheel over it and keyboard focus moving out of view scroll it.
        // id:            unique id.
        // rect:          the visible area.
        // contentHeight: height of everything inside, in canvas units.
        // Returns:       the scroll offset to subtract from content y coordinates.
        float beginScroll(std::string_view id, const Rect& rect, float contentHeight);

        // Ends the area started by beginScroll() and draws its scroll bar.
        void endScroll();

        // Moves the keyboard focus to a widget (e.g. the first button of a new screen).
        // id: the widget's id.
        void setFocus(std::string_view id);

        // id:      a widget id.
        // Returns: true if that widget has the focus.
        [[nodiscard]] bool isFocused(std::string_view id) const;

        // Returns: true while a key capture waits for its key; Escape then belongs to it.
        [[nodiscard]] bool isCapturing() const noexcept;

        // Returns: true if a text field has the focus (letter keys are typing, not shortcuts).
        [[nodiscard]] bool isTyping() const noexcept;

    private:
        // A scroll area being filled.
        struct ScrollArea
        {
            uint64_t id{0};
            Rect rect{};
            float contentHeight{0.0f};
        };

        // How a widget row reacts this frame.
        struct Interaction
        {
            bool isFocused{false};
            bool isHovered{false};
            bool isActivated{false};
            bool isPressed{false};
            int adjust{0};
        };

        // Registers a focusable widget and works out how it reacts.
        // id:      the widget's hashed id.
        // rect:    its row.
        // Returns: its interaction.
        Interaction interact(uint64_t id, const Rect& rect);

        // Draws a widget row's background, focus marker and label.
        // rect:      the row.
        // label:     the text.
        // isFocused: draw it highlighted.
        void drawRow(const Rect& rect, std::string_view label, bool isFocused);

        // Returns: `color` with its alpha scaled by the opacity.
        [[nodiscard]] math::Vec4 faded(const math::Vec4& color) const noexcept;

        // Returns: a canvas rectangle in normalized screen coordinates.
        [[nodiscard]] render::ScreenRect toScreen(const Rect& rect) const noexcept;

        // Returns: true if the cursor is inside `rect` and the current clip.
        [[nodiscard]] bool isUnderCursor(const Rect& rect) const;

        // Hands the innermost clip rectangle (or none) to the renderer for later draws.
        void applyClip();

        render::Renderer* m_renderer{nullptr};
        platform::Input m_input{};
        Theme m_theme{};
        math::Vec2 m_canvas{k_canvasHeight * 16.0f / 9.0f, k_canvasHeight};
        float m_opacity{1.0f};
        float m_seconds{0.0f};
        bool m_acceptsInput{true};
        std::optional<math::Vec2> m_cursor;
        std::optional<math::Vec2> m_lastCursor;
        bool m_hasCursorMoved{false};
        uint64_t m_focus{0};
        uint64_t m_active{0};
        uint64_t m_typing{0};
        uint64_t m_capturing{0};
        uint64_t m_frame{0};
        uint64_t m_captureFrame{0};
        int m_navigate{0};
        int m_pendingNavigate{0};
        int m_adjust{0};
        bool m_activate{false};
        std::vector<uint64_t> m_order;
        std::vector<uint64_t> m_previousOrder;
        std::vector<Rect> m_clips;
        std::vector<ScrollArea> m_scrolls;
        std::unordered_map<uint64_t, float> m_scrollOffsets;
    };
} //namespace lockstep::ui

#endif //LOCKSTEP_UI_UI_CONTEXT_HPP
