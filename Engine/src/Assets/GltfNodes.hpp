#ifndef LOCKSTEP_ASSETS_GLTF_NODES_HPP
#define LOCKSTEP_ASSETS_GLTF_NODES_HPP

#include "Lockstep/Assets/Scene.hpp"
#include "Lockstep/Core/Transform.hpp"

#include "ImportWarnings.hpp"

#include <cgltf.h>

#include <vector>

namespace lockstep::assets
{
    // A node of the scene tree with its world placement.
    // node:      the glTF node.
    // transform: world transform, import scale included.
    struct PlacedNode
    {
        const cgltf_node* node{nullptr};
        math::Transform transform{};
    };

    // Walks the default scene's node tree depth-first, children in order, and places every node.
    // Nodes whose world matrix has a zero scale are left out (with a warning if they hold a
    // mesh); skinned nodes are kept in bind pose with a warning.
    // document: the glTF document.
    // scale:    uniform scale applied around the origin.
    // warnings: receives what was skipped or approximated.
    // Returns:  the placed nodes, in visit order.
    [[nodiscard]] std::vector<PlacedNode> placeNodes(const cgltf_data& document, float scale, ImportWarnings& warnings);

    // Converts a KHR_lights_punctual light to the scene's description.
    // light:   the light.
    // placed:  the node holding it.
    // Returns: the light; named after the light, or the node when the light has no name.
    [[nodiscard]] SceneLight toSceneLight(const cgltf_light& light, const PlacedNode& placed);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_GLTF_NODES_HPP
