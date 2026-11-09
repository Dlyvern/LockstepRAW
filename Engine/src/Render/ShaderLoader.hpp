#ifndef LOCKSTEP_RENDER_SHADER_LOADER_HPP
#define LOCKSTEP_RENDER_SHADER_LOADER_HPP

#include "Lockstep/Core/Result.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace lockstep::render
{
    // Loads a compiled Engine shader staged by CMake into `<executable dir>/shaders/`.
    // fileName: the .spv file name, e.g. "unlit.vert.spv".
    // Returns:  the SPIR-V words, or an error if the file is missing or not valid SPIR-V.
    [[nodiscard]] Result<std::vector<uint32_t>> loadShader(std::string_view fileName);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_SHADER_LOADER_HPP
