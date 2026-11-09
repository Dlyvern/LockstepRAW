#ifndef LOCKSTEP_GAME_PRESENTATION_MATERIAL_SHOWCASE_HPP
#define LOCKSTEP_GAME_PRESENTATION_MATERIAL_SHOWCASE_HPP

#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <vector>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // Test objects for judging the PBR renderer, placed along the arena's north wall: a row of
    // dielectric spheres and a row of gold spheres going from smooth to rough, a normal-mapped
    // slowly turning cube, an emissive sphere, an alpha-masked (cut-out) panel and an alpha-blended
    // glass panel. Purely presentational.
    // sphere / cube / panel: meshes.
    // dielectrics / metals:  one material per roughness step, smooth first.
    // bumpy, emissive, cutout, glass: the single-object materials.
    struct MaterialShowcase
    {
        lockstep::render::MeshHandle sphere{};
        lockstep::render::MeshHandle cube{};
        lockstep::render::MeshHandle panel{};
        std::vector<lockstep::render::MaterialHandle> dielectrics;
        std::vector<lockstep::render::MaterialHandle> metals;
        lockstep::render::MaterialHandle bumpy{};
        lockstep::render::MaterialHandle emissive{};
        lockstep::render::MaterialHandle cutout{};
        lockstep::render::MaterialHandle glass{};
    };

    // Creates the showcase's meshes, textures and materials.
    // renderer: the renderer that owns them.
    // Returns:  the showcase, or an error if an upload failed.
    [[nodiscard]] lockstep::Result<MaterialShowcase> createMaterialShowcase(lockstep::render::Renderer& renderer);

    // Queues the showcase for drawing.
    // renderer: the renderer.
    // showcase: the showcase.
    // seconds:  presentation time; turns the normal-mapped cube.
    void drawMaterialShowcase(lockstep::render::Renderer& renderer, const MaterialShowcase& showcase, float seconds);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_MATERIAL_SHOWCASE_HPP
