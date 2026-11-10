#ifndef LOCKSTEP_COOKER_SURFACE_RULES_HPP
#define LOCKSTEP_COOKER_SURFACE_RULES_HPP

#include <cstdint>
#include <filesystem>
#include <vector>

namespace lockstep::cooker
{
    struct SourceScene;

    // Where the cooker finds height maps and whether it assigns micro-detail layers. glTF can express
    // neither, so both follow conventions (see applySurfaceRules()).
    // heightDirectories: folders searched for a height map belonging to a material's normal map.
    // detail:            give materials a SurfaceDetail layer by name.
    struct SurfaceRuleOptions
    {
        std::vector<std::filesystem::path> heightDirectories;
        bool detail{true};
    };

    // What applySurfaceRules() assigned.
    // heightMaps:      materials that got a height map (parallax).
    // detailLayers:    materials that got a micro-detail layer.
    struct SurfaceRuleReport
    {
        uint32_t heightMaps{0};
        uint32_t detailLayers{0};
    };

    // Gives a scene's opaque and masked materials parallax height maps and micro-detail layers.
    // Materials are measured first: metres per UV unit from the world area and UV area of the
    // triangles that use them (instance scale included).
    //
    // Height maps: a material whose normal map image is named "<id>_nor_<res>" (or "_nor_gl_",
    // "_nor_dx_"; Poly Haven) uses "<dir>/<id>/<id>_disp_<res>.png|jpg" or "<dir>/<id>_disp_<res>.png|jpg"
    // (other resolutions if that one is missing); one named "<name>_normal" uses "<dir>/<name>_height.png|jpg".
    // Each height map is stretched to 0-1 when cooked; its parallax depth in metres comes from the
    // material's name (bricks 25 mm, paving 15 mm, asphalt 12 mm, concrete 10 mm, tiles 8 mm, plaster
    // and paint 4 mm, else 10 mm) and is divided by the metres per UV unit.
    //
    // Detail layers, by words in the material or base colour image name (first match wins): Metal
    // (metal, steel, iron, shutter, pipe, rail, chrome, rust, desk, cart, cabinet, locker, vent, ...),
    // Paint for smooth man-made surfaces (linoleum, plastic, laminate, sign, clock), Mineral (concrete, cement,
    // brick, plaster, asphalt, road, pavement, stone, terrazzo, tile, ceiling, wall, floor, facade, ...),
    // Fibrous (fabric, cloth, sofa, curtain, bed, carpet, wood, crate, table, cardboard, ...), then
    // Paint (paint, door, frame). The layer repeats every half metre. Decals, glass, windows,
    // screens, alpha-blended materials, materials without UV area and materials whose UVs stretch
    // over more than 32 m per unit (it would repeat over 64 times per UV unit) get none.
    // scene:   the scene read by readSourceScene(); its materials and textures are extended.
    // options: where height maps are and whether detail is on.
    // Returns: how many materials got what.
    [[nodiscard]] SurfaceRuleReport applySurfaceRules(SourceScene& scene, const SurfaceRuleOptions& options);
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_SURFACE_RULES_HPP
