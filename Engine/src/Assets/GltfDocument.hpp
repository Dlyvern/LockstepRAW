#ifndef LOCKSTEP_ASSETS_GLTF_DOCUMENT_HPP
#define LOCKSTEP_ASSETS_GLTF_DOCUMENT_HPP

#include "Lockstep/Core/Result.hpp"

#include "ImportWarnings.hpp"

#include <cgltf.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace lockstep::assets
{
    // Frees a parsed glTF document.
    struct GltfDeleter
    {
        void operator()(cgltf_data* data) const noexcept;
    };

    // A parsed glTF document with its buffers loaded.
    using GltfDocument = std::unique_ptr<cgltf_data, GltfDeleter>;

    // Parses a .gltf or .glb file, loads its buffers and validates it. Extensions the importer
    // does not implement are reported once each; a required one fails the load.
    // path:     the file.
    // warnings: receives the ignored extensions.
    // Returns:  the document, or an error if reading, parsing or validation failed or a required
    //           extension is unsupported.
    [[nodiscard]] Result<GltfDocument> loadGltfDocument(const std::filesystem::path& path, ImportWarnings& warnings);

    // Reads the encoded bytes of a glTF image: from a buffer view (.glb), a data URI, or a file
    // next to the document.
    // image:     the image.
    // directory: the folder the document is in.
    // Returns:   the bytes, or an error if the source is missing or unreadable.
    [[nodiscard]] Result<std::vector<std::byte>> readImageBytes(const cgltf_image& image,
        const std::filesystem::path& directory);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_GLTF_DOCUMENT_HPP
