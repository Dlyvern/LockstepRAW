cmake_minimum_required(VERSION 3.25)
project(Lockstep VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Every executable lands in one folder, next to its staged shaders and content.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ImGui dev tools (inspectors, time-scale sliders, net stats). Off for shipping builds.
option(LOCKSTEP_DEV_TOOLS "Build the in-game ImGui dev tools" ON)

add_subdirectory(external)

# Dependency rule: Game -> Engine -> external. Never the other way.
add_subdirectory(Engine)
add_subdirectory(Game)
