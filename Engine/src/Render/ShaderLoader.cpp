#include "ShaderLoader.hpp"

#include "VelaConversions.hpp"

#include <Vela/Assets/Resources.hpp>
#include <Vela/Assets/Shader.hpp>

#include <filesystem>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_shaderDirectory{"shaders"};
    } //namespace

    Result<std::vector<uint32_t>> loadShader(std::string_view fileName)
    {
        const std::filesystem::path directory = vela::assets::resources::executableDirectory();

        if (directory.empty())
        {
            return Error{ErrorCode::PlatformFailure, "Could not resolve the executable directory"};
        }

        const std::filesystem::path path = directory / k_shaderDirectory / fileName;

        auto spirv = vela::assets::loadSpirv(path.string());

        if (!spirv)
        {
            return toError(spirv.error());
        }

        return std::move(spirv).value();
    }
} //namespace lockstep::render
