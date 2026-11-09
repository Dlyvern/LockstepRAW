#include "CookerOutput.hpp"

#include <fstream>
#include <string>
#include <system_error>

namespace lockstep::cooker
{
    Status writeCookedFile(const std::filesystem::path& root, std::string_view path, std::span<const std::byte> bytes)
    {
        const std::filesystem::path target = root / std::filesystem::path(path);
        std::error_code error;
        std::filesystem::create_directories(target.parent_path(), error);
        std::ofstream file(target, std::ios::binary | std::ios::trunc);

        if (!file || !file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        {
            return Error{ErrorCode::AssetLoadFailed, "cannot write '" + target.string() + "'"};
        }

        return {};
    }
} //namespace lockstep::cooker
