#ifndef LOCKSTEP_COOKER_SCENE_COOK_HPP
#define LOCKSTEP_COOKER_SCENE_COOK_HPP

#include <Lockstep/Core/Result.hpp>

#include "SurfaceRules.hpp"
#include "TextureCaps.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace lockstep::cooker
{
    // What to cook and where.
    // source:  the .gltf or .glb file.
    // out:     the output folder, the root of the virtual paths (mount it with --content).
    // name:    virtual folder for this scene's files ("levels/sponza"); the level file is
    //          "<name>/scene". The folder is emptied first.
    // scale:   uniform scale applied to the scene.
    // threads: worker threads; 0 picks the hardware thread count.
    // textureCaps: texture size limits (see textureCaps()).
    // splitLimbs: split humanoid skinned models into limb regions with wound caps (--limbs; see
    //          splitLimbs()); models whose skeleton lacks a limb joint are cooked whole.
    // wound:   with splitLimbs, a glTF whose node "Wound_Stump" holds the wound stump mesh (axis
    //          +Y, wound face on top); it is placed at every cut and its material becomes the
    //          wound material (--wound). Empty for plain caps.
    // surfaces: height maps for parallax and micro-detail layers by material (see
    //          applySurfaceRules(); --height-dir, --no-surface-detail).
    // singleSided: opaque materials are cooked single-sided whatever glTF says (--single-sided);
    //          masked and blended ones keep it. For closed models such as characters, whose
    //          Blender export marks every material double-sided: back faces then cost no shading.
    struct CookOptions
    {
        std::filesystem::path source;
        std::filesystem::path out;
        std::string name;
        float scale{1.0f};
        uint32_t threads{0};
        TextureCapOptions textureCaps{};
        bool splitLimbs{false};
        std::filesystem::path wound;
        SurfaceRuleOptions surfaces{};
        bool singleSided{false};
    };

    // Cooks a glTF scene into engine-ready files: meshes in PbrVertex layout, textures in their
    // final Bc format with every mip, PhysX collision cooked once per distinct mesh, and the level
    // file. Prints progress, sizes, encode error and import warnings. The same input gives the
    // same bytes.
    // options: source, output and name.
    // Returns: success, or the first read, decode, cook or write error. An image that can't be
    //          decoded is reported and left out (its materials use their factors).
    [[nodiscard]] Status cookScene(const CookOptions& options);
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_SCENE_COOK_HPP
