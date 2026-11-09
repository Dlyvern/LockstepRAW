#define CGLTF_IMPLEMENTATION
#include "GltfDocument.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>

namespace lockstep::assets
{
    namespace
    {
        constexpr std::array<std::string_view, 3> k_supportedExtensions{
            "KHR_texture_basisu",
            "KHR_materials_emissive_strength",
            "KHR_mesh_quantization"};

        constexpr std::string_view k_dataUriPrefix{"data:"};
        constexpr std::string_view k_base64Marker{";base64,"};

        bool isSupported(std::string_view extension)
        {
            for (const std::string_view supported : k_supportedExtensions)
            {
                if (supported == extension)
                {
                    return true;
                }
            }

            return false;
        }

        Error loadError(const std::filesystem::path& path, std::string_view what, cgltf_result result)
        {
            return Error{ErrorCode::AssetLoadFailed, "importGltf: " + std::string(what) + " '" + path.string()
                + "' failed (cgltf error " + std::to_string(static_cast<int>(result)) + ")"};
        }

        Result<std::vector<std::byte>> readFile(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);

            if (!file)
            {
                return Error{ErrorCode::AssetLoadFailed, "cannot open '" + path.string() + "'"};
            }

            const std::streamsize size = file.tellg();
            std::vector<std::byte> bytes(static_cast<std::size_t>(size));
            file.seekg(0);

            if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
            {
                return Error{ErrorCode::AssetLoadFailed, "cannot read '" + path.string() + "'"};
            }

            return bytes;
        }

        Result<std::vector<std::byte>> decodeDataUri(std::string_view uri)
        {
            const std::size_t marker = uri.find(k_base64Marker);

            if (marker == std::string_view::npos)
            {
                return Error{ErrorCode::AssetLoadFailed, "image data URI is not base64"};
            }

            const std::string_view base64 = uri.substr(marker + k_base64Marker.size());
            std::size_t padding = 0;

            while (padding < base64.size() && padding < 2 && base64[base64.size() - 1 - padding] == '=')
            {
                ++padding;
            }

            const std::size_t size = base64.size() / 4 * 3 - padding;
            const std::string terminated(base64);
            const cgltf_options options{};
            void* decoded = nullptr;

            if (cgltf_load_buffer_base64(&options, size, terminated.c_str(), &decoded) != cgltf_result_success)
            {
                return Error{ErrorCode::AssetLoadFailed, "image data URI is not valid base64"};
            }

            const auto* begin = static_cast<const std::byte*>(decoded);
            std::vector<std::byte> bytes(begin, begin + size);
            std::free(decoded);

            return bytes;
        }
    } //namespace

    void GltfDeleter::operator()(cgltf_data* data) const noexcept
    {
        cgltf_free(data);
    }

    Result<GltfDocument> loadGltfDocument(const std::filesystem::path& path, ImportWarnings& warnings)
    {
        const std::string file = path.string();
        const cgltf_options options{};
        cgltf_data* raw = nullptr;

        if (const cgltf_result parsed = cgltf_parse_file(&options, file.c_str(), &raw); parsed != cgltf_result_success)
        {
            return loadError(path, "parsing", parsed);
        }

        GltfDocument document(raw);

        if (const cgltf_result loaded = cgltf_load_buffers(&options, document.get(), file.c_str());
            loaded != cgltf_result_success)
        {
            return loadError(path, "loading buffers of", loaded);
        }

        if (const cgltf_result valid = cgltf_validate(document.get()); valid != cgltf_result_success)
        {
            return loadError(path, "validating", valid);
        }

        for (cgltf_size index = 0; index < document->extensions_required_count; ++index)
        {
            const std::string_view extension = document->extensions_required[index];

            if (!isSupported(extension))
            {
                return Error{ErrorCode::AssetLoadFailed, "importGltf: '" + file + "' requires unsupported extension "
                    + std::string(extension)};
            }
        }

        for (cgltf_size index = 0; index < document->extensions_used_count; ++index)
        {
            const std::string_view extension = document->extensions_used[index];

            if (!isSupported(extension))
            {
                warnings.add("extension " + std::string(extension) + " is ignored");
            }
        }

        return document;
    }

    Result<std::vector<std::byte>> readImageBytes(const cgltf_image& image, const std::filesystem::path& directory)
    {
        if (image.buffer_view != nullptr)
        {
            const auto* begin = reinterpret_cast<const std::byte*>(cgltf_buffer_view_data(image.buffer_view));

            if (begin == nullptr)
            {
                return Error{ErrorCode::AssetLoadFailed, "image buffer view has no data"};
            }

            return std::vector<std::byte>(begin, begin + image.buffer_view->size);
        }

        if (image.uri == nullptr)
        {
            return Error{ErrorCode::AssetLoadFailed, "image has neither a URI nor a buffer view"};
        }

        const std::string_view uri = image.uri;

        if (uri.starts_with(k_dataUriPrefix))
        {
            return decodeDataUri(uri);
        }

        std::string relative(uri);
        relative.resize(cgltf_decode_uri(relative.data()));

        return readFile(directory / std::filesystem::path(relative));
    }
} //namespace lockstep::assets
