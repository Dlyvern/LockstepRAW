#include "Effects/ViewModelFlash.hpp"

#include "Effects/EffectTextures.hpp"

#include <Lockstep/Render/PbrMaterial.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/Render/Views.hpp>

#include <algorithm>
#include <iostream>
#include <vector>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;

        constexpr uint32_t k_stripFrames{k_flashFrames + 1};
        constexpr float k_cell{1.0f / static_cast<float>(k_stripFrames)};
        constexpr float k_starAhead{0.12f};
        constexpr float k_sideHalfWidth{0.3f};

        struct FlashMesh
        {
            std::vector<render::PbrVertex> vertices;
            std::vector<uint32_t> indices;

            void quad(const std::array<math::Vec3, 4>& corners, const std::array<math::Vec2, 4>& uvs,
                const math::Vec3& normal, const math::Vec3& tangent)
            {
                const auto base = static_cast<uint32_t>(vertices.size());

                for (std::size_t corner = 0; corner < 4; ++corner)
                {
                    vertices.push_back(render::PbrVertex{
                        .position = {corners[corner].x, corners[corner].y, corners[corner].z},
                        .normal = {normal.x, normal.y, normal.z},
                        .uv = {uvs[corner].x, uvs[corner].y},
                        .tangent = {tangent.x, tangent.y, tangent.z, 1.0f}});
                }

                indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
            }
        };

        FlashMesh flashMesh(uint32_t variant)
        {
            FlashMesh mesh{};
            const float left = k_cell * static_cast<float>(variant);
            const float right = left + k_cell;
            mesh.quad({math::Vec3{-0.5f, k_starAhead, 0.5f}, math::Vec3{0.5f, k_starAhead, 0.5f},
                          math::Vec3{0.5f, k_starAhead, -0.5f}, math::Vec3{-0.5f, k_starAhead, -0.5f}},
                {math::Vec2{left, 1.0f}, math::Vec2{right, 1.0f}, math::Vec2{right, 0.0f}, math::Vec2{left, 0.0f}},
                {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f});

            const float side = k_cell * static_cast<float>(k_flashFrames);
            mesh.quad({math::Vec3{-k_sideHalfWidth, 0.0f, 0.0f}, math::Vec3{-k_sideHalfWidth, 1.0f, 0.0f},
                          math::Vec3{k_sideHalfWidth, 1.0f, 0.0f}, math::Vec3{k_sideHalfWidth, 0.0f, 0.0f}},
                {math::Vec2{side, 0.0f}, math::Vec2{1.0f, 0.0f}, math::Vec2{1.0f, 1.0f}, math::Vec2{side, 1.0f}},
                {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f});
            mesh.quad({math::Vec3{0.0f, 0.0f, -k_sideHalfWidth}, math::Vec3{0.0f, 1.0f, -k_sideHalfWidth},
                          math::Vec3{0.0f, 1.0f, k_sideHalfWidth}, math::Vec3{0.0f, 0.0f, k_sideHalfWidth}},
                {math::Vec2{side, 0.0f}, math::Vec2{1.0f, 0.0f}, math::Vec2{1.0f, 1.0f}, math::Vec2{side, 1.0f}},
                {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});

            return mesh;
        }

        uint32_t nextRandom(ViewModelFlash& flash)
        {
            flash.random ^= flash.random << 13;
            flash.random ^= flash.random >> 17;
            flash.random ^= flash.random << 5;
            return flash.random;
        }

        float randomRange(ViewModelFlash& flash, float low, float high)
        {
            return low + (high - low) * static_cast<float>(nextRandom(flash) % 10000) / 10000.0f;
        }

        math::Quat basisRotation(const math::Vec3& xAxis, const math::Vec3& yAxis)
        {
            const math::Vec3 z = math::cross(xAxis, yAxis);
            math::Mat4 basis{1.0f};
            basis.setColumn(0, math::Vec4{xAxis.x, xAxis.y, xAxis.z, 0.0f});
            basis.setColumn(1, math::Vec4{yAxis.x, yAxis.y, yAxis.z, 0.0f});
            basis.setColumn(2, math::Vec4{z.x, z.y, z.z, 0.0f});

            return math::fromRotationMatrix(basis);
        }

        bool createLook(ViewModelFlash& flash, render::Renderer& renderer)
        {
            const std::vector<std::byte> pixels = effectStripPixels(k_flashFrame, k_stripFrames);
            auto texture = renderer.createTexture(render::TextureDescription{.pixels = pixels,
                .width = k_stripFrames * k_effectCellSize,
                .height = k_effectCellSize,
                .format = render::TextureFormat::Rgba8Srgb,
                .generateMips = true});

            if (!texture)
            {
                std::clog << "Game: viewmodel flash texture not created: " << texture.error().message << '\n';
                return false;
            }

            flash.texture = texture.value();
            auto material = renderer.createMaterial(render::PbrMaterialDescription{.baseColor = flash.texture,
                .baseColorFactor = {0.0f, 0.0f, 0.0f, 1.0f},
                .metallicFactor = 0.0f,
                .roughnessFactor = 1.0f,
                .emissive = flash.texture,
                .emissiveFactor = {1.0f, 0.72f, 0.42f},
                .alphaMode = render::AlphaMode::Mask,
                .alphaCutoff = 0.3f,
                .doubleSided = true});

            if (!material)
            {
                std::clog << "Game: viewmodel flash material not created: " << material.error().message << '\n';
                return false;
            }

            flash.material = material.value();

            for (uint32_t variant = 0; variant < flash.meshes.size(); ++variant)
            {
                const FlashMesh built = flashMesh(variant);
                auto mesh = renderer.createMesh(std::span<const render::PbrVertex>(built.vertices), built.indices);

                if (!mesh)
                {
                    std::clog << "Game: viewmodel flash mesh not created: " << mesh.error().message << '\n';
                    return false;
                }

                flash.meshes[variant] = mesh.value();
            }

            return true;
        }
    } //namespace

    void createViewModelFlash(ViewModelFlash& flash, render::Renderer& renderer)
    {
        if (flash.isTried)
        {
            return;
        }

        flash.isTried = true;

        if (!createLook(flash, renderer))
        {
            flash.material = render::MaterialHandle{};
        }
    }

    void fireViewModelFlash(ViewModelFlash& flash, float size)
    {
        flash.frame = 0;
        flash.variant = nextRandom(flash) % static_cast<uint32_t>(flash.meshes.size());
        flash.roll = randomRange(flash, 0.0f, 2.0f * math::k_pi);
        flash.scale = randomRange(flash, 0.8f, 1.2f) * size;
    }

    void drawViewModelFlash(ViewModelFlash& flash, render::Renderer& renderer, const std::optional<WeaponSocket>& socket,
        float aimed)
    {
        const ViewModelFlashTuning& tuning = flash.tuning;
        const int frame = flash.frame;
        flash.frame = std::min(flash.frame + 1, 1000);

        if (!tuning.enabled || frame >= std::clamp(tuning.frames, 1, 4) || !socket || !flash.material.isValid())
        {
            return;
        }

        const float aim = 1.0f + (tuning.aimScale - 1.0f) * std::clamp(aimed, 0.0f, 1.0f);
        const float size = tuning.size * flash.scale * aim;
        const float brightness = tuning.intensity * aim * (frame == 0 ? 1.0f : tuning.secondFrame);
        const math::Quat rotation = math::angleAxis(flash.roll, socket->forward) * basisRotation(socket->right,
            socket->forward);
        renderer.submit(flash.meshes[flash.variant], flash.material,
            math::Transform{.position = socket->muzzle,
                .rotation = rotation,
                .scale = {size, tuning.length * flash.scale * aim, size}},
            render::k_viewModelLayer, brightness);
    }
} //namespace game
