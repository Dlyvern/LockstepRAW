cmake_minimum_required(VERSION 3.25)
project(Lockstep VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Every executable lands in one folder, next to its staged shaders and content.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ImGui dev tools (inspectors, time-scale sliders, net stats). Off for shipping builds.
option(LOCKSTEP_DEV_TOOLS "Build the in-game ImGui dev tools" ON)

# Offline tools (Tools/Cooker). They link encoders the game never needs.
option(LOCKSTEP_BUILD_TOOLS "Build the offline tools (LockstepCooker)" ON)

# Static C runtime on MSVC (/MT, /MTd), as Velix does: PhysX's Windows preset is built with it,
# and every library linked into the game must use the same runtime.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# FMOD Core API for Engine/Audio. The SDK is proprietary and git-ignored (external/fmod/include,
# external/fmod/lib); without it the option defaults to OFF and Engine/Audio builds a silent
# backend, so the game still builds and runs.
if(EXISTS "${CMAKE_SOURCE_DIR}/external/fmod/include/fmod.hpp"
    AND (EXISTS "${CMAKE_SOURCE_DIR}/external/fmod/lib/linux/libfmod.so.14.8"
        OR EXISTS "${CMAKE_SOURCE_DIR}/external/fmod/lib/windows/fmod.dll"))
    set(LOCKSTEP_FMOD_SDK_FOUND ON)
else()
    set(LOCKSTEP_FMOD_SDK_FOUND OFF)
endif()

option(LOCKSTEP_WITH_FMOD "Build Engine/Audio on the FMOD Core API (OFF: silent audio backend)" ${LOCKSTEP_FMOD_SDK_FOUND})

if(LOCKSTEP_WITH_FMOD AND NOT LOCKSTEP_FMOD_SDK_FOUND)
    message(FATAL_ERROR "LOCKSTEP_WITH_FMOD is ON but the FMOD SDK is missing from external/fmod/include and external/fmod/lib")
endif()

if(NOT LOCKSTEP_WITH_FMOD)
    message(STATUS "Lockstep: FMOD off, Engine/Audio uses the silent backend")
endif()

add_subdirectory(external)

# Dependency rule: Game -> Engine -> external. Never the other way.
add_subdirectory(Engine)
add_subdirectory(Game)

if(LOCKSTEP_BUILD_TOOLS)
    add_subdirectory(Tools)
endif()
