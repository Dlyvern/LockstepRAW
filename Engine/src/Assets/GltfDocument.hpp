#ifndef LOCKSTEP_ASSETS_GLTF_DOCUMENT_HPP
#define LOCKSTEP_ASSETS_GLTF_DOCUMENT_HPP

#include "Lockstep/Assets/FileSystem.hpp"
#include "Lockstep/Core/Result.hpp"

#include "ImportWarnings.hpp"

#include <cgltf.h>

#include <cstddef>
#include <memory>
#include <string_view>
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

    // Parses a .gltf or .glb file, loads its buffers and validates it, reading everything through
    // the file system. Extensions the importer does not implement are reported once each; a
    // required one fails the load.
    // files:    the file system; must outlive the call only.
    // path:     the file's virtual path; buffer URIs are resolved next to it.
    // warnings: receives the ignored extensions.
    // Returns:  the document, or an error if reading, parsing or validation failed or a required
    //           extension is unsupported.
    [[nodiscard]] Result<GltfDocument> loadGltfDocument(const FileSystem& files, std::string_view path,
        ImportWarnings& warnings);

    // Reads the encoded bytes of a glTF image: from a buffer view (.glb), a data URI, or a file
    // next to the document. Thread-safe.
    // image:     the image.
    // files:     the file system the document was loaded from.
    // directory: the virtual folder the document is in (empty for the root).
    // Returns:   the bytes, or an error if the source is missing or unreadable.
    [[nodiscard]] Result<std::vector<std::byte>> readImageBytes(const cgltf_image& image, const FileSystem& files,
        std::string_view directory);
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_GLTF_DOCUMENT_HPP
