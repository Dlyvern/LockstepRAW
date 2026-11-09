#ifndef LOCKSTEP_ASSETS_GLTF_NODES_HPP
#define LOCKSTEP_ASSETS_GLTF_NODES_HPP

#include "Lockstep/Assets/Scene.hpp"
#include "Lockstep/Core/Transform.hpp"

#include "ImportWarnings.hpp"

#include <cgltf.h>

#include <vector>

namespace lockstep::assets
{
    // A node of the scene tree with its world placement and where it lands in Scene::nodes.
    // Importers append a SceneNode for every placed node with a name (hasSceneNode()), in visit
    // order, so the indices below match Scene::nodes.
    // node:       the glTF node.
    // transform:  world transform, import scale included.
    // sceneNode:  its index in Scene::nodes, or k_noNode if it has no name.
    // parentNode: index in Scene::nodes of its nearest named placed ancestor, or k_noNode.
    struct PlacedNode
    {
        const cgltf_node* node{nullptr};
        math::Transform transform{};
        uint32_t sceneNode{k_noNode};
        uint32_t parentNode{k_noNode};

        // Returns: the node a mesh instance of this node belongs to: the node itself if named,
        //          else its nearest named ancestor.
        [[nodiscard]] uint32_t instanceNode() const noexcept
        {
            return sceneNode != k_noNode ? sceneNode : parentNode;
        }
    };

    // node:    a glTF node.
    // Returns: true if it has a non-empty name, so importers keep it as a SceneNode.
    [[nodiscard]] bool hasSceneNode(const cgltf_node& node) noexcept;

    // Walks the default scene's node tree depth-first, children in order, and places every node.
    // Nodes whose world matrix has a zero scale are left out (with a warning if they hold a
    // mesh). Skinned mesh nodes are placed like any other; importers draw them through
    // readSkinnedModels() (GltfSkins.hpp) instead of as static instances.
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
