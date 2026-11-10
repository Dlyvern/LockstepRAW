#ifndef LOCKSTEP_GAME_GORE_BLOOD_ASSETS_HPP
#define LOCKSTEP_GAME_GORE_BLOOD_ASSETS_HPP

#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <array>
#include <cstddef>
#include <optional>

namespace lockstep::assets
{
    class FileSystem;
} //namespace lockstep::assets

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // Number of splatter looks (each its own generated texture).
    inline constexpr std::size_t k_splatterLooks{3};

    // The GPU resources blood is drawn with: the decal quad, the droplet mesh, and decal
    // materials: the blood splat art of the cooked gore scene (characters/gore/scene, node
    // Blood_Splat, from Content/source/characters/gore/blood.png) for splatters and pools when it
    // is mounted, else procedurally generated textures (see bloodTexture()); drips are always
    // generated. Owned by the Renderer for the whole game.
    // art:        the gore scene, kept loaded while its material is used.
    // decalQuad:  unit quad in the XZ plane facing +Y, for submitDecal().
    // droplet:    unit cube for flying droplets.
    // splatters:  wet, alpha-blended splatter materials.
    // pool:       the pool material.
    // drop:       the small drip material.
    // dropletLook: glossy dark red, opaque, for flying droplets.
    struct BloodAssets
    {
        lockstep::render::MeshHandle decalQuad{};
        lockstep::render::MeshHandle droplet{};
        std::array<lockstep::render::MaterialHandle, k_splatterLooks> splatters{};
        lockstep::render::MaterialHandle pool{};
        lockstep::render::MaterialHandle drop{};
        lockstep::render::MaterialHandle dropletLook{};
        std::optional<lockstep::assets::Scene> art{};
    };

    // Loads the blood art (or generates stand-ins) and creates the materials and meshes.
    // renderer: the renderer.
    // files:    the mounted content (the gore scene is in characters.pak).
    // Returns:  the assets, or the first creation error.
    [[nodiscard]] lockstep::Result<BloodAssets> createBloodAssets(lockstep::render::Renderer& renderer,
        const lockstep::assets::FileSystem& files);
} //namespace game

#endif //LOCKSTEP_GAME_GORE_BLOOD_ASSETS_HPP
