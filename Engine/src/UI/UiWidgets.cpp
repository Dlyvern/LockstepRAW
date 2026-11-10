#include "Lockstep/UI/UiContext.hpp"

#include "Lockstep/Core/Hash.hpp"

#include <algorithm>
#include <cmath>

namespace lockstep::ui
{
    namespace
    {
        constexpr float k_valueWidthShare{0.42f};
        constexpr float k_sliderValueWidth{110.0f};
        constexpr float k_sliderThumbWidth{6.0f};
        constexpr float k_caretBlinkSeconds{0.53f};
        constexpr char k_firstPrintable{' '};
        constexpr char k_lastPrintable{'~'};

        Rect valueArea(const Rect& row, float padding)
        {
            return Rect{.min = {row.max.x - row.width() * k_valueWidthShare, row.min.y},
                .max = {row.max.x - padding, row.max.y}};
        }

        bool isAllowed(char32_t codepoint, std::string_view allowed)
        {
            if (codepoint < static_cast<char32_t>(k_firstPrintable) || codepoint > static_cast<char32_t>(k_lastPrintable))
            {
                return false;
            }

            return allowed.empty() || allowed.find(static_cast<char>(codepoint)) != std::string_view::npos;
        }

        float snapped(float value, float minimum, float step)
        {
            return step > 0.0f ? minimum + std::round((value - minimum) / step) * step : value;
        }
    } //namespace

    bool UiContext::button(std::string_view id, const Rect& rect, std::string_view label)
    {
        const Interaction interaction = interact(hash64(id), rect);
        drawRow(rect, label, interaction.isFocused);

        return interaction.isActivated;
    }

    bool UiContext::toggle(std::string_view id, const Rect& rect, std::string_view label, bool& value)
    {
        const Interaction interaction = interact(hash64(id), rect);
        const bool changed = interaction.isActivated || interaction.adjust != 0;

        if (changed)
        {
            value = !value;
        }

        drawRow(rect, label, interaction.isFocused);
        const Rect area = valueArea(rect, m_theme.padding);
        text(value ? "ON" : "OFF", math::Vec2{area.max.x, rect.center().y - m_theme.textSize * 0.5f}, m_theme.textSize,
            value ? m_theme.text : m_theme.textDim, render::TextAlign::Right);

        return changed;
    }

    bool UiContext::slider(std::string_view id, const Rect& rect, std::string_view label, float& value, float minimum,
        float maximum, float step, std::string_view valueText)
    {
        const uint64_t key = hash64(id);
        const Interaction interaction = interact(key, rect);
        const Rect area = valueArea(rect, m_theme.padding);
        const Rect track{.min = {area.min.x, rect.center().y - 1.0f},
            .max = {area.max.x - k_sliderValueWidth, rect.center().y + 1.0f}};
        const float before = value;

        if (interaction.isPressed && !(m_cursor && m_cursor->x >= track.min.x - k_sliderThumbWidth
                && m_cursor->x <= track.max.x + k_sliderThumbWidth))
        {
            m_active = 0;
        }

        if (m_active == key && m_cursor && m_input.isMouseButtonDown(platform::MouseButton::Left) && track.width() > 0.0f)
        {
            const float share = std::clamp((m_cursor->x - track.min.x) / track.width(), 0.0f, 1.0f);
            value = snapped(minimum + share * (maximum - minimum), minimum, step);
        }

        if (interaction.adjust != 0)
        {
            value = snapped(value + static_cast<float>(interaction.adjust) * step, minimum, step);
        }

        value = std::clamp(value, minimum, maximum);
        drawRow(rect, label, interaction.isFocused);
        fillRect(track, m_theme.frame);

        const float share = maximum > minimum ? (value - minimum) / (maximum - minimum) : 0.0f;
        const float thumbX = track.min.x + share * track.width();
        fillRect(Rect{.min = track.min, .max = {thumbX, track.max.y}}, m_theme.text);
        fillRect(Rect{.min = {thumbX - k_sliderThumbWidth * 0.5f, rect.center().y - 9.0f},
            .max = {thumbX + k_sliderThumbWidth * 0.5f, rect.center().y + 9.0f}},
            interaction.isFocused ? m_theme.accent : m_theme.text);
        text(valueText, math::Vec2{area.max.x, rect.center().y - m_theme.textSize * 0.5f}, m_theme.textSize, m_theme.text,
            render::TextAlign::Right);

        return value != before;
    }

    bool UiContext::cycler(std::string_view id, const Rect& rect, std::string_view label, std::size_t& index,
        std::span<const std::string> options)
    {
        const Interaction interaction = interact(hash64(id), rect);
        const Rect area = valueArea(rect, m_theme.padding);

        if (options.empty())
        {
            drawRow(rect, label, interaction.isFocused);
            return false;
        }

        const std::size_t count = options.size();
        const std::size_t before = std::min(index, count - 1);
        int step = interaction.adjust;

        if (interaction.isPressed && m_cursor)
        {
            step = m_cursor->x < area.center().x ? -1 : 1;
        }
        else if (interaction.isActivated)
        {
            step = 1;
        }

        const auto countValue = static_cast<int>(count);
        index = (before + static_cast<std::size_t>((step % countValue + countValue) % countValue)) % count;
        drawRow(rect, label, interaction.isFocused);

        const float textY = rect.center().y - m_theme.textSize * 0.5f;
        const math::Vec4 arrows = interaction.isFocused ? m_theme.accent : m_theme.textDim;
        text("<", math::Vec2{area.min.x, textY}, m_theme.textSize, arrows);
        text(">", math::Vec2{area.max.x, textY}, m_theme.textSize, arrows, render::TextAlign::Right);
        text(options[index], math::Vec2{area.center().x, textY}, m_theme.textSize, m_theme.text, render::TextAlign::Center);

        return index != before;
    }

    bool UiContext::textField(std::string_view id, const Rect& rect, std::string_view label, std::string& text,
        std::size_t maxLength, std::string_view allowed)
    {
        const Interaction interaction = interact(hash64(id), rect);
        const std::string before = text;

        if (interaction.isFocused)
        {
            m_typing = hash64(id);

            for (const char32_t codepoint : m_input.typedText())
            {
                if (text.size() < maxLength && isAllowed(codepoint, allowed))
                {
                    text.push_back(static_cast<char>(codepoint));
                }
            }

            if (m_input.wasKeyTyped(platform::Key::Backspace) && !text.empty())
            {
                text.pop_back();
            }

            if (m_input.wasKeyPressed(platform::Key::Enter))
            {
                m_pendingNavigate = 1;
            }
        }

        drawRow(rect, label, interaction.isFocused);
        const Rect area = valueArea(rect, m_theme.padding);
        const Rect box{.min = {area.min.x - 8.0f, rect.min.y + 6.0f}, .max = {area.max.x + 8.0f, rect.max.y - 6.0f}};
        strokeRect(box, interaction.isFocused ? m_theme.text : m_theme.frame, m_theme.lineThickness);

        const float textY = rect.center().y - m_theme.textSize * 0.5f;
        this->text(text, math::Vec2{area.min.x, textY}, m_theme.textSize, m_theme.text);

        if (interaction.isFocused && std::fmod(m_seconds, k_caretBlinkSeconds * 2.0f) < k_caretBlinkSeconds)
        {
            const float caretX = area.min.x + measureText(text, m_theme.textSize).x + 2.0f;
            fillRect(Rect{.min = {caretX, textY}, .max = {caretX + 2.0f, textY + m_theme.textSize}}, m_theme.accent);
        }

        return text != before;
    }

    std::optional<CapturedInput> UiContext::keyCapture(std::string_view id, const Rect& rect, std::string_view label,
        std::string_view current)
    {
        const uint64_t key = hash64(id);
        const Interaction interaction = interact(key, rect);
        std::optional<CapturedInput> captured;

        if (m_capturing == key && m_frame > m_captureFrame && m_acceptsInput)
        {
            for (std::size_t index = 0; index < static_cast<std::size_t>(platform::Key::Count) && !captured; ++index)
            {
                const auto candidate = static_cast<platform::Key>(index);

                if (m_input.wasKeyPressed(candidate))
                {
                    m_capturing = 0;

                    if (candidate != platform::Key::Escape)
                    {
                        captured = CapturedInput{.key = candidate, .button = std::nullopt};
                    }

                    break;
                }
            }

            for (std::size_t index = 0; index < static_cast<std::size_t>(platform::MouseButton::Count) && m_capturing != 0;
                 ++index)
            {
                const auto button = static_cast<platform::MouseButton>(index);

                if (m_input.wasMouseButtonPressed(button))
                {
                    m_capturing = 0;
                    captured = CapturedInput{.key = std::nullopt, .button = button};
                }
            }
        }
        else if (interaction.isActivated && m_capturing == 0)
        {
            m_capturing = key;
            m_captureFrame = m_frame;
        }

        const bool isWaiting = m_capturing == key;
        drawRow(rect, label, interaction.isFocused || isWaiting);
        const Rect area = valueArea(rect, m_theme.padding);
        const float textY = rect.center().y - m_theme.textSize * 0.5f;

        if (isWaiting)
        {
            text("PRESS A KEY / ESC", math::Vec2{area.max.x, textY}, m_theme.textSize, m_theme.accent,
                render::TextAlign::Right);
        }
        else
        {
            text(current, math::Vec2{area.max.x, textY}, m_theme.textSize, m_theme.text, render::TextAlign::Right);
        }

        return captured;
    }
} //namespace lockstep::ui
