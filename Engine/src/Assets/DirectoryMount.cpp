#include "FileSystemMounts.hpp"

#include <fstream>
#include <system_error>

namespace lockstep::assets
{
    namespace
    {
        class DirectoryMount final : public Mount
        {
        public:
            explicit DirectoryMount(std::filesystem::path root)
                : m_root(std::move(root))
            {
            }

            bool exists(const std::string& path) const override
            {
                std::error_code error;
                return std::filesystem::is_regular_file(m_root / std::filesystem::path(path), error);
            }

            Result<std::vector<std::byte>> read(const std::string& path) const override
            {
                return readOsFile(m_root / std::filesystem::path(path));
            }

            void list(std::string_view directory, std::vector<std::string>& paths) const override
            {
                std::error_code error;
                const std::filesystem::path start = m_root / std::filesystem::path(directory);

                if (!std::filesystem::is_directory(start, error))
                {
                    return;
                }

                for (std::filesystem::recursive_directory_iterator entry(start, error), end;
                     !error && entry != end; entry.increment(error))
                {
                    if (entry->is_regular_file(error))
                    {
                        paths.push_back(entry->path().lexically_relative(m_root).generic_string());
                    }
                }
            }

        private:
            std::filesystem::path m_root;
        };
    } //namespace

    std::unique_ptr<Mount> createDirectoryMount(const std::filesystem::path& directory)
    {
        return std::make_unique<DirectoryMount>(directory);
    }

    Result<std::vector<std::byte>> readOsFile(const std::filesystem::path& path)
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
} //namespace lockstep::assets
