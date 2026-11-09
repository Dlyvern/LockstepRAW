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
        for (const SceneNode& node : scene.nodes)
        {
            if (matchesName(node.name, name))
            {
                return node.transform;
            }
        }

        return std::nullopt;
    }

    void submitScene(render::Renderer& renderer, const Scene& scene)
    {
        for (const SceneInstance& instance : scene.instances)
        {
            const SceneMesh& mesh = scene.meshes[instance.mesh];
            renderer.submit(mesh.mesh, mesh.material, instance.transform);
        }
    }
} //namespace lockstep::assets
