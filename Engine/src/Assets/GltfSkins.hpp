#ifndef LOCKSTEP_ASSETS_GLTF_SKINS_HPP
#define LOCKSTEP_ASSETS_GLTF_SKINS_HPP

#include "Lockstep/Anim/AnimationClip.hpp"
#include "Lockstep/Anim/Skeleton.hpp"
#include "Lockstep/Core/Bounds.hpp"
#include "Lockstep/Core/Transform.hpp"
#include "Lockstep/Render/PbrMaterial.hpp"

#include "GltfNodes.hpp"
#include "ImportWarnings.hpp"

#include <cgltf.h>

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace lockstep::assets
{
    // Maps a glTF joint node to its index in the Skeleton built from its skin.
    using JointLookup = std::unordered_map<const cgltf_node*, uint32_t>;

    // CPU geometry of one skinned glTF primitive, ready for upload.
    // vertices: bind-pose vertices with joint indices (into the Skeleton) and normalized weights.
    // indices:  triangle list.
    // bounds:   bounds of the bind-pose positions.
    struct SkinnedGeometry
    {
        std::vector<render::SkinnedPbrVertex> vertices;
        std::vector<uint32_t> indices;
        math::Aabb bounds{};
    };

    // One primitive of a skinned model and the glTF material it is drawn with (null for the
    // default material).
    struct SkinnedPart
    {
        SkinnedGeometry geometry;
        const cgltf_material* material{nullptr};
    };

    // A glTF skin with every primitive it deforms and the animations of its joints: the CPU side
    // of an anim::SkinnedModel.
    // name:       the skin's name, or its first mesh node's.
    // skeleton:   joints ordered parents first; model space is the glTF scene's (unscaled).
    // clips:      animations with at least one channel on the skeleton.
    // jointRadii: per joint, the reach of the vertices it moves (anim::poseBounds()).
    // bindBounds: model-space bounds of the parts in the rest pose.
    // parts:      the skinned primitives.
    // transform:  where the model goes in the scene: the import scale.
    // node:       Scene::nodes index of the first mesh node using the skin (or its nearest named
    //             ancestor), or k_noNode.
    struct GltfSkinnedModel
    {
        std::string name;
        anim::Skeleton skeleton;
        std::vector<anim::AnimationClip> clips;
        std::vector<float> jointRadii;
        math::Aabb bindBounds{};
        std::vector<SkinnedPart> parts;
        math::Transform transform{};
        uint32_t node{k_noNode};
    };

    // Reads every skin used by a placed mesh node: one model per skin, holding the primitives of
    // every node that uses it. Skins with more than anim::k_maxJoints joints, and primitives
    // without JOINTS_0 / WEIGHTS_0, are skipped with a warning.
    // document: the glTF document.
    // placed:   the placed nodes from placeNodes().
    // scale:    the import scale.
    // warnings: receives what was skipped or approximated.
    // Returns:  the models, in order of their skins' first use.
    [[nodiscard]] std::vector<GltfSkinnedModel> readSkinnedModels(const cgltf_data& document,
        std::span<const PlacedNode> placed, float scale, ImportWarnings& warnings);

    // Reads the animation channels that target a skeleton's joints, one clip per glTF animation
    // that has any. Morph-target weight channels are skipped with a warning.
    // document: the glTF document.
    // joints:   the skeleton's joint nodes.
    // warnings: receives what was skipped.
    // Returns:  the clips, in document order.
    [[nodiscard]] std::vector<anim::AnimationClip> readClips(const cgltf_data& document, const JointLookup& joints,
        ImportWarnings& warnings);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_GLTF_SKINS_HPP
