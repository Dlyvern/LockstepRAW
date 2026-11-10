#ifndef LOCKSTEP_COOKER_LIMB_SPLIT_HPP
#define LOCKSTEP_COOKER_LIMB_SPLIT_HPP

#include <Lockstep/Anim/Limbs.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Render/PbrMaterial.hpp>

#include "Assets/GltfSkins.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace lockstep::cooker
{
    // One part of a split skinned model.
    // geometry:     its skinned vertices and triangles.
    // material:     index into the level's materials.
    // materialName: the source material's name ("Wound_Cap" for caps).
    // limb, role:   its place in the split.
    struct LimbSplitPart
    {
        assets::SkinnedGeometry geometry;
        uint32_t material{0};
        std::string materialName;
        anim::Limb limb{anim::Limb::None};
        anim::LimbPartRole role{anim::LimbPartRole::Whole};
    };

    // A skinned model split into limb regions.
    // parts:     region parts (every source part cut by limb, in source order, empty pieces left
    //            out), then for every limb the stump cap and the piece cap.
    // limbs:     every limb but the torso, with its root joint and hull points.
    // capLoops:  boundary loops closed by caps, for the cook log.
    struct LimbSplit
    {
        std::vector<LimbSplitPart> parts;
        std::vector<anim::LimbInfo> limbs;
        uint32_t capLoops{0};
    };

    // The wound stump placed across every cut: a short tube whose upward-facing faces are the
    // wound and whose sides are torn flesh, normalised so its axis is +Y, the wound face is at
    // y = 0 (flaps may reach above it, the base below) and its widest radius is 1.
    // vertices: positions, normals, uvs and tangents.
    // indices:  triangle list.
    struct WoundTemplate
    {
        std::vector<render::PbrVertex> vertices;
        std::vector<uint32_t> indices;
    };

    // Name of the material wound caps are drawn with.
    inline constexpr const char* k_woundMaterialName{"Wound_Cap"};

    // Splits a humanoid skinned model into limb regions for dismemberment: every triangle goes to
    // the limb its vertices' joint weights favour (joints are told apart by Mixamo-style names:
    // Neck, LeftArm, LeftForeArm, LeftUpLeg, LeftLeg and the Right ones), each source part is cut
    // into one part per limb (keeping its material), and every boundary loop between a limb (with
    // the limbs hanging from it) and the rest of the body is closed twice: a slightly recessed
    // stump cap facing out of the body, and a piece cap facing out of the limb (their UVs map
    // to the dark disc at the top left of the wound texture). With a wound template, each side
    // also gets a wound stump: the template across the cut, along the limb's bone, scaled to the
    // cut's radius and standing a little out of it, skinned to that side. A limb whose
    // mesh is not joined to the body (a separate head mesh) gets its caps on the open borders
    // nearest its root joint instead, one on each side.
    // model:        the skinned model.
    // materials:    per source part, index into the level's materials.
    // names:        per source part, the material name.
    // capMaterial:  index into the level's materials of the wound material.
    // wound:        the wound stump template, or null for caps alone.
    // Returns:      the split, or InvalidArgument if the skeleton lacks a limb's root joint.
    [[nodiscard]] Result<LimbSplit> splitLimbs(const assets::GltfSkinnedModel& model, std::span<const uint32_t> materials,
        std::span<const std::string> names, uint32_t capMaterial, const WoundTemplate* wound);
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_LIMB_SPLIT_HPP
