#include "Lockstep/Assets/FileSystem.hpp"

#include "Lockstep/Core/Hash.hpp"

#include "FileSystemMounts.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

namespace lockstep::assets
{
    namespace
    {
        constexpr std::string_view k_contentFlag{"--content"};

        Error notFound(std::string_view path)
        {
            return Error{ErrorCode::AssetLoadFailed, "FileSystem: no mount has '" + std::string(path) + "'"};
        }

        bool isBelow(std::string_view path, std::string_view directory)
        {
            if (directory.empty())
            {
                return true;
            }

            return path.size() > directory.size() && path.starts_with(directory) && path[directory.size()] == '/';
        }
    } //namespace

    Result<std::string> normalizePath(std::string_view path)
    {
        std::vector<std::string_view> parts;
        std::size_t start = 0;

        while (start <= path.size())
        {
            std::size_t end = path.find_first_of("/\\", start);

            if (end == std::string_view::npos)
            {
                end = path.size();
            }

            const std::string_view part = path.substr(start, end - start);
            start = end + 1;

            if (part.empty() || part == ".")
            {
                continue;
            }

            if (part == "..")
            {
                if (parts.empty())
                {
                    return Error{ErrorCode::InvalidArgument, "FileSystem: '" + std::string(path) + "' leaves the root"};
                }

                parts.pop_back();
                continue;
            }

            parts.push_back(part);
        }

        if (parts.empty())
        {
            return Error{ErrorCode::InvalidArgument, "FileSystem: '" + std::string(path) + "' names no file"};
        }

        std::string normalized;

        for (const std::string_view part : parts)
        {
            if (!normalized.empty())
            {
                normalized += '/';
            }

            normalized += part;
        }

        return normalized;
    }

    uint64_t pathKey(std::string_view path) noexcept
    {
        return hash64(path);
    }

    FileSystem::FileSystem()
        : m_state(std::make_unique<FileSystemState>())
    {
    }

    FileSystem::~FileSystem() = default;

    FileSystem::FileSystem(FileSystem&& other) noexcept
        : m_state(std::exchange(other.m_state, std::make_unique<FileSystemState>()))
    {
    }

    FileSystem& FileSystem::operator=(FileSystem&& other) noexcept
    {
        if (this != &other)
        {
            m_state = std::exchange(other.m_state, std::make_unique<FileSystemState>());
        }

        return *this;
    }

    Status FileSystem::mountDirectory(const std::filesystem::path& directory)
    {
        std::error_code error;

        if (!std::filesystem::is_directory(directory, error))
        {
            return Error{ErrorCode::AssetLoadFailed, "FileSystem: '" + directory.string() + "' is not a folder"};
        }

        m_state->mounts.push_back(createDirectoryMount(directory));
        m_state->infos.push_back(MountInfo{.source = directory, .kind = MountKind::Directory});

        return {};
    }

    Status FileSystem::mountPak(const std::filesystem::path& pak)
    {
        MountInfo info{.source = pak, .kind = MountKind::Pak};
        auto mount = openPakMount(pak, info);

        if (!mount)
        {
            return mount.error();
        }

        m_state->mounts.push_back(std::move(mount).value());
        m_state->infos.push_back(std::move(info));

        return {};
    }

    Status FileSystem::mount(const std::filesystem::path& source)
    {
        std::error_code error;

        if (std::filesystem::is_directory(source, error))
        {
            return mountDirectory(source);
        }

        if (!std::filesystem::is_regular_file(source, error))
        {
            return Error{ErrorCode::AssetLoadFailed, "FileSystem: '" + source.string() + "' does not exist"};
        }

        return mountPak(source);
    }

    bool FileSystem::exists(std::string_view path) const
    {
        const auto normalized = normalizePath(path);

        if (!normalized)
        {
            return false;
        }

        return std::any_of(m_state->mounts.begin(), m_state->mounts.end(), [&normalized](const auto& mount)
        {
            return mount->exists(normalized.value());
        });
    }

    Result<std::vector<std::byte>> FileSystem::read(std::string_view path) const
    {
        auto normalized = normalizePath(path);

        if (!normalized)
        {
            return normalized.error();
        }

        for (const std::unique_ptr<Mount>& mount : m_state->mounts)
        {
            if (mount->exists(normalized.value()))
            {
                return mount->read(normalized.value());
            }
        }

        return notFound(normalized.value());
    }

    std::vector<std::string> FileSystem::list(std::string_view directory) const
    {
        std::string folder;

        if (const auto normalized = normalizePath(directory))
        {
            folder = normalized.value();
        }

        std::vector<std::string> paths;

        for (const std::unique_ptr<Mount>& mount : m_state->mounts)
        {
            mount->list(folder, paths);
        }

        std::erase_if(paths, [&folder](const std::string& path) { return !isBelow(path, folder); });
        std::sort(paths.begin(), paths.end());
        paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

        return paths;
    }

    std::span<const MountInfo> FileSystem::mounts() const noexcept
    {
        return m_state->infos;
    }

    Result<std::vector<std::filesystem::path>> parseContentMounts(std::span<const char* const> arguments)
    {
        std::vector<std::filesystem::path> sources;

        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            if (arguments[index] == nullptr || std::string_view(arguments[index]) != k_contentFlag)
            {
                continue;
            }

            if (index + 1 >= arguments.size() || arguments[index + 1] == nullptr)
            {
                return Error{ErrorCode::InvalidArgument, "--content needs a folder or .pak"};
            }

            sources.emplace_back(arguments[++index]);
        }

        return sources;
    }
} //namespace lockstep::assets
