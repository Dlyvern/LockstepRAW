#include "Weapons/WeaponDebris.hpp"

#include <Lockstep/Render/PbrMaterial.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;

        constexpr float k_casingRadius{0.00575f};
        constexpr float k_casingLength{0.0228f};
        constexpr uint32_t k_casingSides{10};
        constexpr math::Vec3 k_magazineHalf{0.015f, 0.1225f, 0.0225f};
        constexpr math::Vec4 k_brass{0.80f, 0.56f, 0.26f, 1.0f};
        constexpr math::Vec4 k_polymer{0.035f, 0.035f, 0.038f, 1.0f};

        struct MeshBuilder
        {
            std::vector<render::PbrVertex> vertices;
            std::vector<uint32_t> indices;

            void vertex(const math::Vec3& position, const math::Vec3& normal, const math::Vec3& tangent, float u, float v)
            {
                vertices.push_back(render::PbrVertex{.position = {position.x, position.y, position.z},
                    .normal = {normal.x, normal.y, normal.z},
                    .uv = {u, v},
                    .tangent = {tangent.x, tangent.y, tangent.z, 1.0f}});
            }

            void quad(const math::Vec3& centre, const math::Vec3& normal, const math::Vec3& tangent, float halfTangent,
                float halfBitangent)
            {
                const math::Vec3 bitangent = math::cross(normal, tangent);
                const auto base = static_cast<uint32_t>(vertices.size());
                vertex(centre - tangent * halfTangent - bitangent * halfBitangent, normal, tangent, 0.0f, 1.0f);
                vertex(centre + tangent * halfTangent - bitangent * halfBitangent, normal, tangent, 1.0f, 1.0f);
                vertex(centre + tangent * halfTangent + bitangent * halfBitangent, normal, tangent, 1.0f, 0.0f);
                vertex(centre - tangent * halfTangent + bitangent * halfBitangent, normal, tangent, 0.0f, 0.0f);
                indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
            }
        };

        MeshBuilder cylinder(float radius, float length, uint32_t sides)
        {
            MeshBuilder mesh{};
            const float half = 0.5f * length;

            for (uint32_t side = 0; side <= sides; ++side)
            {
                const float angle = 2.0f * math::k_pi * static_cast<float>(side) / static_cast<float>(sides);
                const math::Vec3 normal{std::cos(angle), 0.0f, -std::sin(angle)};
                const math::Vec3 tangent{-std::sin(angle), 0.0f, -std::cos(angle)};
                const float u = static_cast<float>(side) / static_cast<float>(sides);
                mesh.vertex(normal * radius + math::Vec3{0.0f, -half, 0.0f}, normal, tangent, u, 1.0f);
                mesh.vertex(normal * radius + math::Vec3{0.0f, half, 0.0f}, normal, tangent, u, 0.0f);
            }

            for (uint32_t side = 0; side < sides; ++side)
            {
                const uint32_t bottom = side * 2;
                mesh.indices.insert(mesh.indices.end(), {bottom, bottom + 2, bottom + 3, bottom, bottom + 3, bottom + 1});
            }

            for (const float end : {-1.0f, 1.0f})
            {
                const math::Vec3 normal{0.0f, end, 0.0f};
                const math::Vec3 tangent{1.0f, 0.0f, 0.0f};
                const auto centre = static_cast<uint32_t>(mesh.vertices.size());
                mesh.vertex(normal * half, normal, tangent, 0.5f, 0.5f);

                for (uint32_t side = 0; side < sides; ++side)
                {
                    const float angle = 2.0f * math::k_pi * static_cast<float>(side) / static_cast<float>(sides);
                    const math::Vec3 rim{std::cos(angle) * radius, end * half, -std::sin(angle) * radius};
                    mesh.vertex(rim, normal, tangent, 0.5f + 0.5f * std::cos(angle), 0.5f + 0.5f * std::sin(angle));
                }

                for (uint32_t side = 0; side < sides; ++side)
                {
                    const uint32_t from = centre + 1 + side;
                    const uint32_t to = centre + 1 + (side + 1) % sides;

                    if (end > 0.0f)
                    {
                        mesh.indices.insert(mesh.indices.end(), {centre, from, to});
                    }
                    else
                    {
                        mesh.indices.insert(mesh.indices.end(), {centre, to, from});
                    }
                }
            }

            return mesh;
        }

        MeshBuilder box(const math::Vec3& half)
        {
            MeshBuilder mesh{};
            mesh.quad({half.x, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, half.z, half.y);
            mesh.quad({-half.x, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, half.z, half.y);
            mesh.quad({0.0f, half.y, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, half.x, half.z);
            mesh.quad({0.0f, -half.y, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, half.x, half.z);
            mesh.quad({0.0f, 0.0f, half.z}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, half.x, half.y);
            mesh.quad({0.0f, 0.0f, -half.z}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, half.x, half.y);

            return mesh;
        }

        lockstep::Status createLook(render::Renderer& renderer, DebrisPool& pool, const MeshBuilder& builder,
            const render::PbrMaterialDescription& description)
        {
            auto mesh = renderer.createMesh(std::span<const render::PbrVertex>(builder.vertices), builder.indices);

            if (!mesh)
            {
                return mesh.error();
            }

            auto material = renderer.createMaterial(description, render::MeshKind::Static);

            if (!material)
            {
                static_cast<void>(renderer.destroyMesh(mesh.value()));
                return material.error();
            }

            pool.mesh = mesh.value();
            pool.material = material.value();

            return {};
        }
    } //namespace

    lockstep::Status createCasingLook(render::Renderer& renderer, DebrisPool& pool)
    {
        return createLook(renderer, pool, cylinder(k_casingRadius, k_casingLength, k_casingSides),
            render::PbrMaterialDescription{.baseColorFactor = k_brass,
                .metallicFactor = 1.0f,
                .roughnessFactor = 0.32f,
                .detail = render::SurfaceDetail::Metal});
    }

    lockstep::Status createMagazineLook(render::Renderer& renderer, DebrisPool& pool)
    {
        return createLook(renderer, pool, box(k_magazineHalf),
            render::PbrMaterialDescription{.baseColorFactor = k_polymer,
                .metallicFactor = 0.0f,
                .roughnessFactor = 0.55f,
                .detail = render::SurfaceDetail::Paint});
    }
} //namespace game
