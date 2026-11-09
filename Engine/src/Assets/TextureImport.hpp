#ifndef LOCKSTEP_ASSETS_TEXTURE_IMPORT_HPP
#define LOCKSTEP_ASSETS_TEXTURE_IMPORT_HPP

#include "Lockstep/Assets/FileSystem.hpp"
#include "Lockstep/Assets/Scene.hpp"
#include "Lockstep/Core/Result.hpp"

#include "GltfMaterials.hpp"
#include "ImportWarnings.hpp"

#include <cstdint>
#include <string_view>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace lockstep::assets
{
    // Decodes every texture in the table on worker threads and uploads each on the calling thread
    // as soon as it is ready, in table order. At most a few textures per worker wait decoded in
    // memory at a time. A texture that can't be read or decoded is reported and left invalid,
    // so its materials fall back to their factors.
    // renderer:  creates the textures.
    // textures:  the textures to create; their handles are filled in.
    // files:     the file system the document was loaded from.
    // directory: the virtual folder of the glTF document, for relative image URIs.
    // options:   texture compression and worker count.
    // stats:     texture count, bytes and compressed count are added.
    // warnings:  receives failed images.
    // Returns:   success, or the error of a failed GPU upload.
    [[nodiscard]] Status importTextures(render::Renderer& renderer, TextureTable& textures, const FileSystem& files,
        std::string_view directory, const SceneImportOptions& options, SceneStats& stats, ImportWarnings& warnings);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_TEXTURE_IMPORT_HPP
