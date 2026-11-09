#include "ShaderLoader.hpp"

#include "VelaConversions.hpp"

#include <Vela/Assets/Shader.hpp>

#include <filesystem>
#include <string>
#include <system_error>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#endif

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_shaderDirectory{"shaders"};

        Result<std::filesystem::path> executableDirectory()
        {
#if defined(_WIN32)
            wchar_t buffer[MAX_PATH];
            const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);

            if (length == 0 || length == MAX_PATH)
            {
                return Error{ErrorCode::PlatformFailure, "Could not resolve the executable path"};
            }

            return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
            char buffer[4096];
            uint32_t size = sizeof(buffer);

            if (_NSGetExecutablePath(buffer, &size) != 0)
            {
                return Error{ErrorCode::PlatformFailure, "Could not resolve the executable path"};
            }

            std::error_code errorCode;
            std::filesystem::path executable = std::filesystem::canonical(buffer, errorCode);

            if (errorCode)
            {
                return Error{ErrorCode::PlatformFailure, "Could not resolve the executable path: " + errorCode.message()};
            }

            return executable.parent_path();
#else
            std::error_code errorCode;
            std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", errorCode);

            if (errorCode)
            {
                return Error{ErrorCode::PlatformFailure, "Could not resolve the executable path: " + errorCode.message()};
            }

            return executable.parent_path();
#endif
        }
    } //namespace

    Result<std::vector<uint32_t>> loadShader(std::string_view fileName)
    {
        auto directory = executableDirectory();

        if (!directory)
        {
            return directory.error();
        }

        const std::filesystem::path path = directory.value() / k_shaderDirectory / fileName;

        auto spirv = vela::assets::loadSpirv(path.string());

        if (!spirv)
        {
            return toError(spirv.error());
        }

        return std::move(spirv).value();
    }
} //namespace lockstep::render
