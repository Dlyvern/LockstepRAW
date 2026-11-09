#include "Lockstep/Render/Shapes.hpp"

#include <Vela/Builtins/Shapes.hpp>

namespace lockstep::render::shapes
{
    MeshData cube()
    {
        const auto velaCube = vela::builtins::shapes3d::cube();

        MeshData mesh{};
        mesh.vertices.reserve(velaCube.vertices.size());
        mesh.indices = velaCube.indices;

        for (const vela::graphics::StaticVertex& source : velaCube.vertices)
        {
            mesh.vertices.push_back(Vertex{
                {source.position[0], source.position[1], source.position[2]},
                {source.uv[0], source.uv[1]},
                {source.normal[0], source.normal[1], source.normal[2]}});
        }

        return mesh;
    }
} //namespace lockstep::render::shapes
