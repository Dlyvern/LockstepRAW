#include "Effects/BulletHoles.hpp"

#include "Audio/Surfaces.hpp"
#include "Level/SceneLevel.hpp"
#include "Weapons/Penetration.hpp"

#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/PbrMaterial.hpp>
#include <Lockstep/Render/Renderer.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>
#include <iostream>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;
        namespace physics = lockstep::physics;

        constexpr float k_minHoleSize{0.02f};
        constexpr float k_goldenAngle{2.39996f};
        constexpr float k_movedDistance{0.005f};
        constexpr float k_movedTurn{0.9999f};

        float calibreScale(WeaponKind weapon)
        {
            switch (weapon)
            {
                case WeaponKind::Glock:
                {
                    return 1.1f;
                }
                case WeaponKind::Kriss:
                {
                    return 1.3f;
                }
                case WeaponKind::M4:
                case WeaponKind::Count:
                {
                    return 1.0f;
                }
            }

            return 1.0f;
        }

        render::MaterialShading holeShading(HoleSurface surface, float reliefScale)
        {
            return render::MaterialShading{.decalRelief = holeStyle(surface).relief * reliefScale};
        }

        std::optional<render::MaterialHandle> createHoleMaterial(render::Renderer& renderer, HoleSurface surface,
            float reliefScale)
        {
            const std::vector<std::byte> pixels = holeTexturePixels(surface);
            auto texture = renderer.createTexture(render::TextureDescription{.pixels = pixels,
                .width = k_holeTextureSize,
                .height = k_holeTextureSize,
                .format = render::TextureFormat::Rgba8Srgb,
                .generateMips = true});

            if (!texture)
            {
                std::clog << "Game: bullet hole texture (" << holeSurfaceName(surface) << ") rejected: "
                          << texture.error().message << '\n';
                return std::nullopt;
            }

            const HoleStyle style = holeStyle(surface);
            auto material = renderer.createMaterial(render::PbrMaterialDescription{.baseColor = texture.value(),
                .metallicFactor = style.metallic,
                .roughnessFactor = style.roughness,
                .alphaMode = render::AlphaMode::Blend,
                .doubleSided = true,
                .shading = holeShading(surface, reliefScale)});

            if (!material)
            {
                std::clog << "Game: bullet hole material (" << holeSurfaceName(surface) << ") rejected: "
                          << material.error().message << '\n';
                return std::nullopt;
            }

            return material.value();
        }

        bool isMarkable(const CombatImpact& impact, const BulletHoleFrame& frame, bool allowRemote)
        {
            return !impact.isFlesh && !impact.body.isNull() && (allowRemote || !impact.isRemote)
                && frame.physics->isAlive(impact.body)
                && std::find(frame.unpaintable.begin(), frame.unpaintable.end(), impact.body) == frame.unpaintable.end();
        }

        bool hasMoved(const math::Transform& before, const math::Transform& now)
        {
            return math::length(now.position - before.position) > k_movedDistance
                || std::abs(math::dot(now.rotation, before.rotation)) < k_movedTurn;
        }

        void dropDetachedHoles(BulletHoles& holes, const physics::PhysicsWorld& world)
        {
            std::erase_if(holes.holes, [&world](const BulletHole& hole)
            {
                const std::optional<math::Transform> pose = world.transform(hole.body);
                return !pose || hasMoved(hole.bodyPose, *pose);
            });
        }

        void addHole(BulletHoles& holes, const BulletHole& hole, uint32_t budget)
        {
            holes.holes.push_back(hole);

            if (holes.holes.size() > budget)
            {
                holes.holes.erase(holes.holes.begin(),
                    holes.holes.begin() + static_cast<std::ptrdiff_t>(holes.holes.size() - budget));
            }
        }

        void makeMaterials(BulletHoles& holes, render::Renderer& renderer)
        {
            holes.isTried = true;
            holes.appliedRelief = holes.tuning.reliefScale;

            for (std::size_t index = 0; index < k_holeSurfaceCount; ++index)
            {
                holes.materials[index] = createHoleMaterial(renderer, static_cast<HoleSurface>(index),
                    holes.tuning.reliefScale);
            }
        }

        void updateRelief(BulletHoles& holes, render::Renderer& renderer)
        {
            if (holes.appliedRelief == holes.tuning.reliefScale)
            {
                return;
            }

            for (std::size_t index = 0; index < k_holeSurfaceCount; ++index)
            {
                if (holes.materials[index])
                {
                    static_cast<void>(renderer.setMaterialShading(*holes.materials[index],
                        holeShading(static_cast<HoleSurface>(index), holes.tuning.reliefScale)));
                }
            }

            holes.appliedRelief = holes.tuning.reliefScale;
        }
    } //namespace

    HoleSurface holeSurfaceOfBody(const SceneLevel* level, physics::BodyId body)
    {
        switch (shotMaterialOfBody(level, body))
        {
            case ShotMaterial::Glass:
            {
                return HoleSurface::Glass;
            }
            case ShotMaterial::Soft:
            {
                return HoleSurface::Fabric;
            }
            case ShotMaterial::Wood:
            {
                return HoleSurface::Wood;
            }
            case ShotMaterial::Drywall:
            {
                return HoleSurface::Plaster;
            }
            case ShotMaterial::Metal:
            {
                return HoleSurface::Metal;
            }
            case ShotMaterial::Hard:
            case ShotMaterial::Count:
            {
                break;
            }
        }

        switch (surfaceOfBody(level, body))
        {
            case Surface::Tile:
            {
                return HoleSurface::Tile;
            }
            case Surface::Wood:
            {
                return HoleSurface::Wood;
            }
            case Surface::Metal:
            {
                return HoleSurface::Metal;
            }
            case Surface::Concrete:
            case Surface::Grass:
            case Surface::Asphalt:
            case Surface::Count:
            {
                return HoleSurface::Concrete;
            }
        }

        return HoleSurface::Concrete;
    }

    void presentBulletHoles(BulletHoles& holes, render::Renderer& renderer, const BulletHoleFrame& frame)
    {
        if (!holes.isTried)
        {
            makeMaterials(holes, renderer);
        }

        if (!holes.tuning.enabled || frame.physics == nullptr)
        {
            return;
        }

        updateRelief(holes, renderer);
        dropDetachedHoles(holes, *frame.physics);
        const uint32_t budget = std::min(holes.tuning.maxHoles, frame.budget);

        for (const CombatImpact& impact : frame.impacts)
        {
            if (!isMarkable(impact, frame, holes.tuning.remote))
            {
                continue;
            }

            const HoleSurface surface = holeSurfaceOfBody(frame.level, impact.body);
            const std::optional<render::MaterialHandle>& material = holes.materials[static_cast<std::size_t>(surface)];
            const std::optional<math::Transform> pose = frame.physics->transform(impact.body);

            if (!material || !pose)
            {
                continue;
            }

            const float spin = static_cast<float>(holes.counts[static_cast<std::size_t>(surface)]) * k_goldenAngle;
            BulletHole hole{.decal = decalOnSurface(impact.point, impact.normal, spin),
                .body = impact.body,
                .bodyPose = *pose,
                .surface = surface};
            hole.decal.material = *material;
            hole.decal.size = holes.tuning.size * holeStyle(surface).sizeScale * calibreScale(impact.weapon);

            if (fitDecal(*frame.physics, hole.decal, k_minHoleSize))
            {
                addHole(holes, hole, budget);
                ++holes.counts[static_cast<std::size_t>(surface)];
            }
        }

        for (const BulletHole& hole : holes.holes)
        {
            renderer.submitDecal(frame.quad, hole.decal.material, math::Transform{.position = hole.decal.position,
                .rotation = hole.decal.rotation,
                .scale = math::Vec3{hole.decal.size, 1.0f, hole.decal.size}});
        }
    }

    void clearBulletHoles(BulletHoles& holes)
    {
        holes.holes.clear();
        holes.counts = {};
    }

#if LOCKSTEP_DEV_TOOLS
    void bulletHolesDevSection(BulletHoles& holes)
    {
        if (!ImGui::TreeNode("Bullet holes"))
        {
            return;
        }

        int maxHoles = static_cast<int>(holes.tuning.maxHoles);
        ImGui::Checkbox("Bullet holes", &holes.tuning.enabled);
        ImGui::SameLine();
        ImGui::Checkbox("From partner and NPC shots", &holes.tuning.remote);
        ImGui::SliderFloat("Hole size (m)", &holes.tuning.size, 0.02f, 0.2f, "%.3f");
        ImGui::SliderFloat("Crater relief scale", &holes.tuning.reliefScale, 0.0f, 3.0f, "%.2f");

        if (ImGui::SliderInt("Holes kept (quality caps)", &maxHoles, 16, 512))
        {
            holes.tuning.maxHoles = static_cast<uint32_t>(maxHoles);
        }

        ImGui::Text("%zu holes", holes.holes.size());

        for (std::size_t index = 0; index < k_holeSurfaceCount; ++index)
        {
            ImGui::Text("  %s: %u made", holeSurfaceName(static_cast<HoleSurface>(index)), holes.counts[index]);
        }

        if (ImGui::Button("Clear holes"))
        {
            clearBulletHoles(holes);
        }

        ImGui::TreePop();
    }
#endif
} //namespace game
