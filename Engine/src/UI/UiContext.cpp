#include "Lockstep/UI/UiContext.hpp"

#include "Lockstep/Core/Hash.hpp"
#include "Lockstep/Render/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace lockstep::ui
{
    namespace
    {
        constexpr float k_wheelStep{90.0f};
        constexpr float k_scrollBarWidth{4.0f};
        constexpr float k_minThumbHeight{24.0f};
        constexpr float k_cursorMoveThreshold{0.5f};

        Rect intersect(const Rect& left, const Rect& right)
        {
            const Rect result{.min = {std::max(left.min.x, right.min.x), std::max(left.min.y, right.min.y)},
                .max = {std::min(left.max.x, right.max.x), std::min(left.max.y, right.max.y)}};

            return Rect{.min = result.min, .max = {std::max(result.max.x, result.min.x), std::max(result.max.y, result.min.y)}};
        }

        int keyStep(const platform::Input& input, platform::Key positive, platform::Key negative)
        {
            return (input.wasKeyTyped(positive) ? 1 : 0) - (input.wasKeyTyped(negative) ? 1 : 0);
        }
    } //namespace

    void UiContext::begin(render::Renderer& renderer, const platform::Input& input, float frameSeconds,
        bool acceptsInput)
    {
        m_renderer = &renderer;
        m_input = acceptsInput ? input : platform::Input{};
        m_acceptsInput = acceptsInput;
        m_seconds += std::max(frameSeconds, 0.0f);
        m_opacity = 1.0f;
        ++m_frame;

        const math::Vec2 screen = renderer.screenSize();

        if (screen.x > 0.0f && screen.y > 0.0f)
        {
            m_canvas = math::Vec2{k_canvasHeight * screen.x / screen.y, k_canvasHeight};
        }

        m_cursor.reset();

        if (const std::optional<math::Vec2> cursor = m_input.cursorPosition())
        {
            m_cursor = math::Vec2{cursor->x * m_canvas.x, cursor->y * m_canvas.y};
        }

        m_hasCursorMoved = m_cursor && (!m_lastCursor || math::length(*m_cursor - *m_lastCursor) > k_cursorMoveThreshold);
        m_lastCursor = m_cursor;
        m_order.clear();
        m_clips.clear();
        m_scrolls.clear();
        renderer.setScreenClip(std::nullopt);

        const bool wasTyping = m_typing != 0;
        m_typing = 0;
        m_navigate = 0;
        m_adjust = 0;
        m_activate = false;

        if (!m_input.isMouseButtonDown(platform::MouseButton::Left))
        {
            m_active = 0;
        }

        if (m_capturing != 0)
        {
            return;
        }

        const bool isShiftDown = m_input.isKeyDown(platform::Key::LeftShift) || m_input.isKeyDown(platform::Key::RightShift);
        const int tab = m_input.wasKeyTyped(platform::Key::Tab) ? (isShiftDown ? -1 : 1) : 0;
        m_navigate = keyStep(m_input, platform::Key::Down, platform::Key::Up) + tab + std::exchange(m_pendingNavigate, 0);
        m_adjust = wasTyping ? 0 : keyStep(m_input, platform::Key::Right, platform::Key::Left);
        m_activate = m_input.wasKeyPressed(platform::Key::Enter) || (!wasTyping && m_input.wasKeyPressed(platform::Key::Space));

        if (m_navigate == 0 || m_previousOrder.empty())
        {
            return;
        }

        const auto count = static_cast<int>(m_previousOrder.size());
        const auto found = std::find(m_previousOrder.begin(), m_previousOrder.end(), m_focus);
        int index = 0;

        if (found == m_previousOrder.end())
        {
            index = m_navigate > 0 ? 0 : count - 1;
        }
        else
        {
            index = static_cast<int>(found - m_previousOrder.begin()) + m_navigate;
            index = ((index % count) + count) % count;
        }

        m_focus = m_previousOrder[static_cast<std::size_t>(index)];
    }

    void UiContext::end()
    {
        if (m_capturing != 0 && std::find(m_order.begin(), m_order.end(), m_capturing) == m_order.end())
        {
            m_capturing = 0;
        }

        m_previousOrder = m_order;

        if (m_renderer != nullptr)
        {
            m_renderer->setScreenClip(std::nullopt);
        }

        m_renderer = nullptr;
    }

    math::Vec2 UiContext::canvasSize() const noexcept
    {
        return m_canvas;
    }

    Theme& UiContext::theme() noexcept
    {
        return m_theme;
    }

    void UiContext::setOpacity(float opacity) noexcept
    {
        m_opacity = std::clamp(opacity, 0.0f, 1.0f);
    }

    math::Vec4 UiContext::faded(const math::Vec4& color) const noexcept
    {
        return math::Vec4{color.x, color.y, color.z, color.w * m_opacity};
    }

    render::ScreenRect UiContext::toScreen(const Rect& rect) const noexcept
    {
        return render::ScreenRect{.min = {rect.min.x / m_canvas.x, rect.min.y / m_canvas.y},
            .max = {rect.max.x / m_canvas.x, rect.max.y / m_canvas.y}};
    }

    void UiContext::fillRect(const Rect& rect, const math::Vec4& color)
    {
        if (m_renderer != nullptr)
        {
            m_renderer->submitRect(toScreen(rect), faded(color));
        }
    }

    void UiContext::strokeRect(const Rect& rect, const math::Vec4& color, float thickness)
    {
        const float line = std::min({thickness, rect.width() * 0.5f, rect.height() * 0.5f});
        fillRect(Rect{.min = rect.min, .max = {rect.max.x, rect.min.y + line}}, color);
        fillRect(Rect{.min = {rect.min.x, rect.max.y - line}, .max = rect.max}, color);
        fillRect(Rect{.min = {rect.min.x, rect.min.y + line}, .max = {rect.min.x + line, rect.max.y - line}}, color);
        fillRect(Rect{.min = {rect.max.x - line, rect.min.y + line}, .max = {rect.max.x, rect.max.y - line}}, color);
    }

    void UiContext::text(std::string_view utf8, math::Vec2 position, float size, const math::Vec4& color,
        render::TextAlign align, std::optional<render::TextFont> font)
    {
        if (m_renderer == nullptr || utf8.empty())
        {
            return;
        }

        m_renderer->submitText(utf8, math::Vec2{position.x / m_canvas.x, position.y / m_canvas.y},
            render::TextStyle{.size = size, .color = faded(color), .align = align, .shadow = m_theme.textShadow,
                .font = font.value_or(m_theme.font)});
    }

    math::Vec2 UiContext::measureText(std::string_view utf8, float size, std::optional<render::TextFont> font) const
    {
        if (m_renderer == nullptr)
        {
            return math::Vec2{0.0f, 0.0f};
        }

        return m_renderer->measureText(utf8, render::TextStyle{.size = size, .font = font.value_or(m_theme.font)});
    }

    void UiContext::image(render::TextureHandle texture, const Rect& rect, const render::ImageStyle& style)
    {
        if (m_renderer != nullptr)
        {
            render::ImageStyle shown = style;
            shown.tint = faded(style.tint);
            m_renderer->submitImage(texture, toScreen(rect), shown);
        }
    }

    void UiContext::applyClip()
    {
        if (m_renderer == nullptr)
        {
            return;
        }

        if (m_clips.empty())
        {
            m_renderer->setScreenClip(std::nullopt);
            return;
        }

        m_renderer->setScreenClip(toScreen(m_clips.back()));
    }

    void UiContext::pushClip(const Rect& rect)
    {
        m_clips.push_back(m_clips.empty() ? rect : intersect(m_clips.back(), rect));
        applyClip();
    }

    void UiContext::popClip()
    {
        if (!m_clips.empty())
        {
            m_clips.pop_back();
        }

        applyClip();
    }

    bool UiContext::isUnderCursor(const Rect& rect) const
    {
        return m_cursor && rect.contains(*m_cursor) && (m_clips.empty() || m_clips.back().contains(*m_cursor));
    }

    UiContext::Interaction UiContext::interact(uint64_t id, const Rect& rect)
    {
        m_order.push_back(id);
        Interaction interaction{};
        interaction.isHovered = m_capturing == 0 && isUnderCursor(rect);

        if (interaction.isHovered && m_hasCursorMoved)
        {
            m_focus = id;
        }

        interaction.isPressed = interaction.isHovered && m_input.wasMouseButtonPressed(platform::MouseButton::Left);

        if (interaction.isPressed)
        {
            m_focus = id;
            m_active = id;
        }

        interaction.isFocused = m_focus == id;
        interaction.isActivated = interaction.isPressed || (interaction.isFocused && m_activate);
        interaction.adjust = interaction.isFocused ? m_adjust : 0;

        if (interaction.isFocused && m_navigate != 0 && !m_scrolls.empty())
        {
            const ScrollArea& area = m_scrolls.back();
            float& offset = m_scrollOffsets[area.id];

            if (rect.min.y < area.rect.min.y)
            {
                offset -= area.rect.min.y - rect.min.y;
            }
            else if (rect.max.y > area.rect.max.y)
            {
                offset += rect.max.y - area.rect.max.y;
            }
        }

        return interaction;
    }

    void UiContext::drawRow(const Rect& rect, std::string_view label, bool isFocused)
    {
        fillRect(rect, isFocused ? m_theme.rowFocusFill : m_theme.rowFill);

        if (isFocused)
        {
            fillRect(Rect{.min = rect.min, .max = {rect.min.x + 4.0f, rect.max.y}}, m_theme.accent);
        }

        text(label, math::Vec2{rect.min.x + m_theme.padding, rect.center().y - m_theme.textSize * 0.5f}, m_theme.textSize,
            isFocused ? m_theme.text : math::Vec4{m_theme.text.x, m_theme.text.y, m_theme.text.z, m_theme.text.w * 0.82f});
    }

    float UiContext::beginScroll(std::string_view id, const Rect& rect, float contentHeight)
    {
        const uint64_t key = hash64(id);
        float& offset = m_scrollOffsets[key];
        const float maximum = std::max(contentHeight - rect.height(), 0.0f);

        if (isUnderCursor(rect))
        {
            offset -= m_input.scrollDelta().y * k_wheelStep;
        }

        offset = std::clamp(offset, 0.0f, maximum);
        m_scrolls.push_back(ScrollArea{.id = key, .rect = rect, .contentHeight = contentHeight});
        pushClip(rect);

        return offset;
    }

    void UiContext::endScroll()
    {
        if (m_scrolls.empty())
        {
            return;
        }

        const ScrollArea area = m_scrolls.back();
        m_scrolls.pop_back();
        popClip();

        if (area.contentHeight <= area.rect.height())
        {
            return;
        }

        const float offset = std::clamp(m_scrollOffsets[area.id], 0.0f, area.contentHeight - area.rect.height());
        const float visible = area.rect.height() / area.contentHeight;
        const float thumbHeight = std::max(area.rect.height() * visible, k_minThumbHeight);
        const float travel = area.rect.height() - thumbHeight;
        const float thumbTop = area.rect.min.y + travel * offset / (area.contentHeight - area.rect.height());
        const float left = area.rect.max.x - k_scrollBarWidth;
        fillRect(Rect{.min = {left, area.rect.min.y}, .max = area.rect.max},
            math::Vec4{m_theme.frame.x, m_theme.frame.y, m_theme.frame.z, m_theme.frame.w * 0.4f});
        fillRect(Rect{.min = {left, thumbTop}, .max = {area.rect.max.x, thumbTop + thumbHeight}}, m_theme.frame);
    }

    void UiContext::setFocus(std::string_view id)
    {
        m_focus = hash64(id);
    }

    bool UiContext::isFocused(std::string_view id) const
    {
        return m_focus == hash64(id);
    }

    bool UiContext::isCapturing() const noexcept
    {
        return m_capturing != 0;
    }

    bool UiContext::isTyping() const noexcept
    {
        return m_typing != 0;
    }
} //namespace lockstep::ui
