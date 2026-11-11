#ifndef LOCKSTEP_GAME_PRESENTATION_MATERIAL_LOOKS_HPP
#define LOCKSTEP_GAME_PRESENTATION_MATERIAL_LOOKS_HPP

#include <cstdint>
#include <span>

namespace lockstep::assets
{
    struct Scene;
} //namespace lockstep::assets

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // Dev (`--dev-material-ab <frames>`): switches the material shading features (skin, sheen,
    // character surfaces, decal relief) on and off every `period` frames and logs each switch, so
    // their GPU cost can be read from `--dev-gpu-timings` within one run.
    // period: frames per phase; 0 is off.
    // frame:  frames counted so far.
    struct MaterialTimingDrill
    {
        uint64_t period{0};
        uint64_t frame{0};
    };

    // Reads `--dev-material-ab <frames>` (dev builds only).
    // arguments: the command line.
    // Returns:   the drill (period 0 without the flag).
    [[nodiscard]] MaterialTimingDrill parseMaterialTimingDrill(std::span<const char* const> arguments);

    // Advances the drill by one frame and switches the features when a phase ends.
    // drill:    the drill.
    // renderer: the renderer whose settings it switches.
    void updateMaterialTimingDrill(MaterialTimingDrill& drill, lockstep::render::Renderer& renderer);

    // Picks the shading model (render::ShadingModel) of a loaded scene's materials by name, since
    // glTF and the cooker don't carry one:
    // - skinned model parts by material name: "face", "skin", "hand" or "head" (not "headgear") are
    //   skin; "body", "robber", "tac_set", "uniform", "cloth" or "fabric" are fabric (light sheen,
    //   the body textures mix cloth with gear).
    // - level meshes by node name (or a named ancestor's): "mattress", "sofa", "couch", "curtain",
    //   "drape", "pillow", "blanket", "cushion", "fabric", "cloth", "carpet", "rug", "towel",
    //   "uniform", "linen", "trashbag" (none of them with "frame") are fabric.
    // Case-insensitive. Call once after loading a level or a character scene.
    // renderer: the renderer the scene was loaded with.
    // scene:    the loaded scene.
    void applyMaterialLooks(lockstep::render::Renderer& renderer, const lockstep::assets::Scene& scene);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_MATERIAL_LOOKS_HPP
