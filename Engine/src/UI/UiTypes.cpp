#include "Lockstep/UI/UiTypes.hpp"

namespace lockstep::ui
{
    float Rect::width() const noexcept
    {
        return max.x - min.x;
    }

    float Rect::height() const noexcept
    {
        return max.y - min.y;
    }

    math::Vec2 Rect::center() const noexcept
    {
        return math::Vec2{(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
    }

    bool Rect::contains(math::Vec2 point) const noexcept
    {
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
    }

    Rect inset(const Rect& rect, float amount) noexcept
    {
        return Rect{.min = {rect.min.x + amount, rect.min.y + amount}, .max = {rect.max.x - amount, rect.max.y - amount}};
    }

    Rect rectAt(math::Vec2 position, math::Vec2 size) noexcept
    {
        return Rect{.min = position, .max = {position.x + size.x, position.y + size.y}};
    }

    Rect rectAround(math::Vec2 center, math::Vec2 size) noexcept
    {
        return Rect{.min = {center.x - size.x * 0.5f, center.y - size.y * 0.5f},
            .max = {center.x + size.x * 0.5f, center.y + size.y * 0.5f}};
    }

    Column::Column(const Rect& area, float spacing) noexcept
        : m_area(area),
          m_spacing(spacing),
          m_cursor(area.min.y)
    {
    }

    Rect Column::next(float height) noexcept
    {
        const Rect row{.min = {m_area.min.x, m_cursor}, .max = {m_area.max.x, m_cursor + height}};
        m_cursor += height + m_spacing;

        return row;
    }

    void Column::skip(float height) noexcept
    {
        m_cursor += height;
    }

    float Column::cursor() const noexcept
    {
        return m_cursor;
    }
} //namespace lockstep::ui
