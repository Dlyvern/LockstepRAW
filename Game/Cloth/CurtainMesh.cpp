#include "Cloth/CurtainMesh.hpp"

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace math = lockstep::math;

        constexpr uint32_t k_columnsPerCell{3};
        constexpr uint32_t k_rowsPerCell{2};
        constexpr float k_pleatDepth{0.022f};
        constexpr float k_pleatPeriod{0.19f};
        constexpr math::Vec3 k_swingRoom{0.5f, 0.4f, 1.2f};

        void bind(lockstep::render::SkinnedPbrVertex& vertex, const ClothPanel& panel, float u, float v)
        {
            const float gridU = u * static_cast<float>(panel.columns - 1);
            const float gridV = v * static_cast<float>(panel.rows - 1);
            const uint32_t column = std::min(static_cast<uint32_t>(gridU), panel.columns - 2);
            const uint32_t row = std::min(static_cast<uint32_t>(gridV), panel.rows - 2);
            const float fu = gridU - static_cast<float>(column);
            const float fv = gridV - static_cast<float>(row);
            const uint32_t topLeft = row * panel.columns + column;

            vertex.joints[0] = static_cast<float>(topLeft);
            vertex.joints[1] = static_cast<float>(topLeft + 1);
            vertex.joints[2] = static_cast<float>(topLeft + panel.columns);
            vertex.joints[3] = static_cast<float>(topLeft + panel.columns + 1);
            vertex.weights[0] = (1.0f - fu) * (1.0f - fv);
            vertex.weights[1] = fu * (1.0f - fv);
            vertex.weights[2] = (1.0f - fu) * fv;
            vertex.weights[3] = fu * fv;
        }
    } //namespace

    CurtainMeshData buildCurtainMesh(const ClothPanel& panel)
    {
        CurtainMeshData data{};
        const uint32_t columns = (panel.columns - 1) * k_columnsPerCell + 1;
        const uint32_t rows = (panel.rows - 1) * k_rowsPerCell + 1;
        const float wave = 2.0f * math::k_pi / k_pleatPeriod;

        for (uint32_t row = 0; row < rows; ++row)
        {
            for (uint32_t column = 0; column < columns; ++column)
            {
                const float u = static_cast<float>(column) / static_cast<float>(columns - 1);
                const float v = static_cast<float>(row) / static_cast<float>(rows - 1);
                const float across = u * panel.width;
                const float x = across - panel.width * 0.5f;
                const float y = -v * panel.height;
                const float z = k_pleatDepth * std::sin(across * wave);
                const float slope = k_pleatDepth * wave * std::cos(across * wave);
                const math::Vec3 normal = math::normalize(math::Vec3{-slope, 0.0f, 1.0f});
                const math::Vec3 tangent = math::normalize(math::Vec3{1.0f, 0.0f, slope});

                lockstep::render::SkinnedPbrVertex vertex{};
                vertex.position[0] = x;
                vertex.position[1] = y;
                vertex.position[2] = z;
                vertex.normal[0] = normal.x;
                vertex.normal[1] = normal.y;
                vertex.normal[2] = normal.z;
                vertex.uv[0] = across;
                vertex.uv[1] = -y;
                vertex.tangent[0] = tangent.x;
                vertex.tangent[1] = tangent.y;
                vertex.tangent[2] = tangent.z;
                vertex.tangent[3] = 1.0f;
                bind(vertex, panel, u, v);
                data.vertices.push_back(vertex);
            }
        }

        for (uint32_t row = 0; row + 1 < rows; ++row)
        {
            for (uint32_t column = 0; column + 1 < columns; ++column)
            {
                const uint32_t topLeft = row * columns + column;
                const uint32_t bottomLeft = topLeft + columns;
                data.indices.insert(data.indices.end(), {topLeft, bottomLeft, topLeft + 1, topLeft + 1, bottomLeft,
                    bottomLeft + 1});
            }
        }

        data.bounds = math::Aabb{
            .min = math::Vec3{-panel.width * 0.5f, -panel.height, 0.0f} - k_swingRoom,
            .max = math::Vec3{panel.width * 0.5f, 0.0f, 0.0f} + k_swingRoom};
        return data;
    }
} //namespace game
