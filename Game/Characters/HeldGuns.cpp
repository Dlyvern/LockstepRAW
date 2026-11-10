#include "Characters/HeldGuns.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <iostream>
#include <string>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;

        constexpr std::string_view k_muzzleNode{"Muzzle"};
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};

        const char* scenePath(WeaponKind weapon)
        {
            return weapon == WeaponKind::Glock ? "weapons/glock/scene" : "weapons/m4/scene";
        }
    } //namespace

    std::optional<HeldGun> loadHeldGun(lockstep::render::Renderer& renderer, const assets::FileSystem& files,
        WeaponKind weapon, const math::Vec3& muzzleLocal)
    {
        const std::string path = scenePath(weapon);

        if (weapon == WeaponKind::Kriss || !files.exists(path))
        {
            std::clog << "Game: held gun '" << path << "' not mounted; the partner shows the KRISS instead\n";
            return std::nullopt;
        }

        auto loaded = assets::loadCookedScene(renderer, files, path);

        if (!loaded)
        {
            std::clog << "Game: held gun '" << path << "' not loaded: " << loaded.error().message << '\n';
            return std::nullopt;
        }

        const std::optional<math::Transform> muzzle = assets::findNode(loaded.value(), k_muzzleNode);

        if (!muzzle)
        {
            std::clog << "Game: held gun '" << path << "' has no node '" << k_muzzleNode << "'\n";
            return HeldGun{.scene = std::move(loaded).value()};
        }

        const math::Quat rotation = muzzle->rotation * math::angleAxis(math::k_pi, k_up);
        const math::Vec3 position = muzzle->position - rotation * muzzleLocal;

        return HeldGun{.scene = std::move(loaded).value(), .main = math::Transform{.position = position, .rotation = rotation}};
    }

    void drawHeldGun(lockstep::render::Renderer& renderer, const HeldGun& gun, const math::Mat4& hand,
        lockstep::render::RenderLayers layers)
    {
        for (const assets::SceneInstance& instance : gun.scene.instances)
        {
            if (!instance.isVisible || instance.mesh >= gun.scene.meshes.size())
            {
                continue;
            }

            const assets::SceneMesh& mesh = gun.scene.meshes[instance.mesh];
            math::Transform world{};
            static_cast<void>(math::decompose(hand * math::toMatrix(instance.transform), world.position, world.rotation,
                world.scale));
            renderer.submit(mesh.mesh, mesh.material, world, layers);
        }
    }
} //namespace game
