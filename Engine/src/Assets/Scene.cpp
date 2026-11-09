#include "Lockstep/Assets/Scene.hpp"

#include "Lockstep/Render/Renderer.hpp"

#include <cctype>

namespace lockstep::assets
{
    namespace
    {
        bool equalsIgnoringCase(char left, char right)
        {
            return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
        }

        bool matchesName(std::string_view nodeName, std::string_view name)
        {
            if (nodeName.size() < name.size())
            {
                return false;
            }

            for (std::size_t index = 0; index < name.size(); ++index)
            {
                if (!equalsIgnoringCase(nodeName[index], name[index]))
                {
                    return false;
                }
            }

            return nodeName.size() == name.size() || nodeName[name.size()] == '.' || nodeName[name.size()] == '_';
        }
    } //namespace

    std::optional<math::Transform> findNode(const Scene& scene, std::string_view name)
    {
        if (const std::optional<uint32_t> index = findNodeIndex(scene, name))
        {
            return scene.nodes[*index].transform;
        }

        return std::nullopt;
    }

    std::optional<uint32_t> findNodeIndex(const Scene& scene, std::string_view name)
    {
        for (std::size_t index = 0; index < scene.nodes.size(); ++index)
        {
            if (matchesName(scene.nodes[index].name, name))
            {
                return static_cast<uint32_t>(index);
            }
        }

        return std::nullopt;
    }

    bool isNodeInSubtree(const Scene& scene, uint32_t node, uint32_t root)
    {
        while (node < scene.nodes.size())
        {
            if (node == root)
            {
                return true;
            }

            const uint32_t parent = scene.nodes[node].parent;
            node = parent < node ? parent : k_noNode;
        }

        return false;
    }

    std::vector<uint32_t> subtreeInstances(const Scene& scene, uint32_t root)
    {
        std::vector<uint32_t> instances;

        for (std::size_t index = 0; index < scene.instances.size(); ++index)
        {
            if (isNodeInSubtree(scene, scene.instances[index].node, root))
            {
                instances.push_back(static_cast<uint32_t>(index));
            }
        }

        return instances;
    }

    void releaseScene(render::Renderer& renderer, Scene& scene)
    {
        for (const render::MeshHandle mesh : scene.resources.meshes)
        {
            renderer.destroyMesh(mesh);
        }

        for (const render::MaterialHandle material : scene.resources.materials)
        {
            renderer.destroyMaterial(material);
        }

        for (const render::TextureHandle texture : scene.resources.textures)
        {
            renderer.destroyTexture(texture);
        }

        scene = Scene{};
    }

    void submitScene(render::Renderer& renderer, const Scene& scene)
    {
        for (const SceneInstance& instance : scene.instances)
        {
            if (!instance.isVisible)
            {
                continue;
            }

            const SceneMesh& mesh = scene.meshes[instance.mesh];
            renderer.submit(mesh.mesh, mesh.material, instance.transform);
        }
    }
} //namespace lockstep::assets
