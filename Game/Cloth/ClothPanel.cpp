#include "Cloth/ClothPanel.hpp"

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace math = lockstep::math;

        constexpr uint32_t k_minColumns{3};
        constexpr uint32_t k_maxColumns{14};
        constexpr uint32_t k_minRows{3};
        constexpr uint32_t k_maxRows{10};
        constexpr uint32_t k_iterations{6};
        constexpr float k_stretchStiffness{1.0f};
        constexpr float k_shearStiffness{0.6f};
        constexpr float k_bendStiffness{0.2f};
        constexpr float k_windResponse{0.08f};

        uint32_t gridCount(float length, float spacing, uint32_t lowest, uint32_t highest)
        {
            const float cells = std::round(length / std::max(spacing, 0.01f));
            return std::clamp(static_cast<uint32_t>(std::max(cells, 0.0f)) + 1u, lowest, highest);
        }

        void addLink(ClothPanel& panel, uint32_t first, uint32_t second, float stiffness)
        {
            panel.links.push_back(ClothPanel::Link{.first = first,
                .second = second,
                .length = math::length(panel.rest[first] - panel.rest[second]),
                .stiffness = stiffness});
        }

        bool isPinned(const ClothPanel& panel, uint32_t index)
        {
            return index < panel.columns;
        }

        math::Vec3 sheetNormal(const ClothPanel& panel, uint32_t column, uint32_t row)
        {
            const uint32_t left = column > 0 ? column - 1 : column;
            const uint32_t right = column + 1 < panel.columns ? column + 1 : column;
            const uint32_t up = row > 0 ? row - 1 : row;
            const uint32_t down = row + 1 < panel.rows ? row + 1 : row;
            const math::Vec3 across = panel.positions[row * panel.columns + right] - panel.positions[row * panel.columns + left];
            const math::Vec3 upward = panel.positions[up * panel.columns + column] - panel.positions[down * panel.columns + column];
            const math::Vec3 normal = math::cross(across, upward);
            const float length = math::length(normal);

            return length > 1e-6f ? normal * (1.0f / length) : math::Vec3{0.0f, 0.0f, 1.0f};
        }
    } //namespace

    ClothPanel createClothPanel(float width, float height, float spacing)
    {
        ClothPanel panel{};
        panel.width = std::max(width, 0.05f);
        panel.height = std::max(height, 0.05f);
        panel.columns = gridCount(panel.width, spacing, k_minColumns, k_maxColumns);
        panel.rows = gridCount(panel.height, spacing * 1.4f, k_minRows, k_maxRows);

        for (uint32_t row = 0; row < panel.rows; ++row)
        {
            for (uint32_t column = 0; column < panel.columns; ++column)
            {
                const float u = static_cast<float>(column) / static_cast<float>(panel.columns - 1);
                const float v = static_cast<float>(row) / static_cast<float>(panel.rows - 1);
                panel.rest.push_back(math::Vec3{(u - 0.5f) * panel.width, -v * panel.height, 0.0f});
            }
        }

        panel.positions = panel.rest;
        panel.previous = panel.rest;

        for (uint32_t row = 0; row < panel.rows; ++row)
        {
            for (uint32_t column = 0; column < panel.columns; ++column)
            {
                const uint32_t index = row * panel.columns + column;
                const bool hasRight = column + 1 < panel.columns;
                const bool hasDown = row + 1 < panel.rows;

                if (hasRight)
                {
                    addLink(panel, index, index + 1, k_stretchStiffness);
                }

                if (hasDown)
                {
                    addLink(panel, index, index + panel.columns, k_stretchStiffness);
                }

                if (hasRight && hasDown)
                {
                    addLink(panel, index, index + panel.columns + 1, k_shearStiffness);
                    addLink(panel, index + 1, index + panel.columns, k_shearStiffness);
                }

                if (column + 2 < panel.columns)
                {
                    addLink(panel, index, index + 2, k_bendStiffness);
                }

                if (row + 2 < panel.rows)
                {
                    addLink(panel, index, index + 2 * panel.columns, k_bendStiffness);
                }
            }
        }

        return panel;
    }

    void stepCloth(ClothPanel& panel, const ClothForces& forces, float seconds)
    {
        const float squared = seconds * seconds;

        for (uint32_t row = 1; row < panel.rows; ++row)
        {
            for (uint32_t column = 0; column < panel.columns; ++column)
            {
                const uint32_t index = row * panel.columns + column;
                const math::Vec3 normal = sheetNormal(panel, column, row);
                const math::Vec3 velocity = (panel.positions[index] - panel.previous[index]) * (1.0f / seconds);
                const math::Vec3 air = forces.wind - velocity;
                const math::Vec3 push = normal * (math::dot(air, normal) * std::abs(math::dot(air, normal)) * k_windResponse);
                const math::Vec3 current = panel.positions[index];
                panel.positions[index] = current + (current - panel.previous[index]) * forces.damping
                    + (forces.gravity + push) * squared;
                panel.previous[index] = current;
            }
        }

        for (uint32_t iteration = 0; iteration < k_iterations; ++iteration)
        {
            for (const ClothPanel::Link& link : panel.links)
            {
                const bool firstPinned = isPinned(panel, link.first);
                const bool secondPinned = isPinned(panel, link.second);

                if (firstPinned && secondPinned)
                {
                    continue;
                }

                const math::Vec3 delta = panel.positions[link.second] - panel.positions[link.first];
                const float length = math::length(delta);

                if (length <= 1e-6f)
                {
                    continue;
                }

                const float error = (length - link.length) / length * link.stiffness;
                const float firstShare = firstPinned ? 0.0f : secondPinned ? 1.0f : 0.5f;
                panel.positions[link.first] = panel.positions[link.first] + delta * (error * firstShare);
                panel.positions[link.second] = panel.positions[link.second] - delta * (error * (1.0f - firstShare));
            }
        }
    }

    bool pushOutOfCapsule(ClothPanel& panel, const math::Vec3& foot, float height, float radius)
    {
        bool moved = false;

        for (uint32_t index = panel.columns; index < panel.positions.size(); ++index)
        {
            math::Vec3& position = panel.positions[index];

            if (position.y < foot.y || position.y > foot.y + height)
            {
                continue;
            }

            const math::Vec3 away{position.x - foot.x, 0.0f, position.z - foot.z};
            const float distance = math::length(away);

            if (distance >= radius)
            {
                continue;
            }

            const math::Vec3 direction = distance > 1e-4f ? away * (1.0f / distance)
                : math::Vec3{0.0f, 0.0f, position.z >= foot.z ? 1.0f : -1.0f};
            position = position + direction * (radius - distance);
            moved = true;
        }

        return moved;
    }

    void kickCloth(ClothPanel& panel, const math::Vec3& point, const math::Vec3& velocity, float radius, float seconds)
    {
        for (uint32_t index = panel.columns; index < panel.positions.size(); ++index)
        {
            const float distance = math::length(panel.positions[index] - point);

            if (distance >= radius)
            {
                continue;
            }

            const float falloff = 1.0f - distance / radius;
            panel.previous[index] = panel.previous[index] - velocity * (falloff * falloff * seconds);
        }
    }

    float clothMotion(const ClothPanel& panel)
    {
        float largest = 0.0f;

        for (std::size_t index = 0; index < panel.positions.size(); ++index)
        {
            largest = std::max(largest, math::length(panel.positions[index] - panel.previous[index]));
        }

        return largest;
    }

    void restCloth(ClothPanel& panel)
    {
        panel.positions = panel.rest;
        panel.previous = panel.rest;
    }

    void clothJointMatrices(const ClothPanel& panel, std::vector<math::Mat4>& joints)
    {
        joints.resize(panel.positions.size());

        for (uint32_t row = 0; row < panel.rows; ++row)
        {
            for (uint32_t column = 0; column < panel.columns; ++column)
            {
                const uint32_t index = row * panel.columns + column;
                const uint32_t left = column > 0 ? column - 1 : column;
                const uint32_t right = column + 1 < panel.columns ? column + 1 : column;
                const math::Vec3 across = math::normalize(panel.positions[row * panel.columns + right]
                    - panel.positions[row * panel.columns + left]);
                const math::Vec3 normal = sheetNormal(panel, column, row);
                const math::Vec3 tangent = math::normalize(across - normal * math::dot(across, normal));
                const math::Vec3 up = math::cross(normal, tangent);
                const math::Vec3& position = panel.positions[index];
                const math::Vec3& rest = panel.rest[index];
                const math::Vec3 offset = position - (tangent * rest.x + up * rest.y + normal * rest.z);

                joints[index] = math::Mat4{math::Vec4{tangent.x, tangent.y, tangent.z, 0.0f},
                    math::Vec4{up.x, up.y, up.z, 0.0f}, math::Vec4{normal.x, normal.y, normal.z, 0.0f},
                    math::Vec4{offset.x, offset.y, offset.z, 1.0f}};
            }
        }
    }
} //namespace game
