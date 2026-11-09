add_library(LockstepEngine STATIC
    src/App/Run.cpp
    src/Core/FixedTimestep.cpp
    src/Core/Result.cpp
    src/Core/Transform.cpp
    src/Core/VelaError.cpp
    src/Net/Network.cpp
    src/Net/Replication.cpp
    src/Net/Session.cpp
    src/Net/SessionConnection.cpp
    src/Net/SessionLaunch.cpp
    src/Net/SnapshotBuffer.cpp
    src/Net/SnapshotFormat.cpp
    src/Net/WireFormat.cpp
    src/Platform/Input.cpp
    src/Platform/VelaInput.cpp
    src/Platform/Window.cpp
    src/Render/Renderer.cpp
    src/Render/RendererResources.cpp
    src/Render/RenderPassAdapter.cpp
    src/Render/ShaderLoader.cpp
    src/Render/Shapes.cpp
    src/Render/VelaConversions.cpp
    src/World/Entities.cpp
    src/World/TimeDomains.cpp
)
add_library(Lockstep::Engine ALIAS LockstepEngine)

target_include_directories(LockstepEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(LockstepEngine PUBLIC cxx_std_20)

# Dev tools (Engine/Debug). The DevTools API exists in every build; without LOCKSTEP_DEV_TOOLS it
# is a no-op and ImGui is not linked. LOCKSTEP_DEV_TOOLS is public (0 or 1) so Game can wrap its
# own ImGui panels in `#if LOCKSTEP_DEV_TOOLS`, and imgui is public so those panels can include it.
if(LOCKSTEP_DEV_TOOLS)
    target_sources(LockstepEngine PRIVATE
        src/Debug/DevTools.cpp
        src/Debug/EnginePanel.cpp
        src/Debug/ImGuiInput.cpp
        src/Debug/ImGuiOverlayPass.cpp
        src/Debug/NetPanel.cpp
    )
    target_link_libraries(LockstepEngine PUBLIC imgui)
    target_compile_definitions(LockstepEngine PUBLIC LOCKSTEP_DEV_TOOLS=1)
else()
    target_sources(LockstepEngine PRIVATE src/Debug/DevToolsDisabled.cpp)
    target_compile_definitions(LockstepEngine PUBLIC LOCKSTEP_DEV_TOOLS=0)
endif()

if(MSVC)
    target_compile_options(LockstepEngine PRIVATE /W4)
else()
    target_compile_options(LockstepEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Vela's math (header-only) is public: Lockstep/Core/Math.hpp re-exports its types to Game.
# The rest of Vela stays private. Vela::Builtins must come before Vela::Vela (Builtins uses the
# backend without declaring it).
target_link_libraries(LockstepEngine
    PUBLIC
        Vela::Math
    PRIVATE
        Vela::Builtins Vela::Assets Vela::Vela
)

# GameNetworkingSockets stays private: only Engine/src/Net includes it.
target_link_libraries(LockstepEngine PRIVATE GameNetworkingSockets::static)

# Shaders the Engine loads at runtime from "<exe dir>/shaders". A static library has no runtime
# directory of its own, so each executable that links the Engine stages them for itself.
set(ENGINE_SHADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/unlit.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/unlit.frag
)

if(LOCKSTEP_DEV_TOOLS)
    list(APPEND ENGINE_SHADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/imgui.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/imgui.frag
    )
endif()

set(LOCKSTEP_ENGINE_SHADERS ${ENGINE_SHADERS} CACHE INTERNAL "GLSL sources of the Engine's built-in shaders")

# Compiles the Engine's shaders and stages the .spv files next to TARGET's executable.
function(lockstep_stage_engine_shaders TARGET)
    set(RELATIVE_SOURCES "")

    foreach(SHADER ${LOCKSTEP_ENGINE_SHADERS})
        file(RELATIVE_PATH RELATIVE_SHADER ${CMAKE_CURRENT_SOURCE_DIR} ${SHADER})
        list(APPEND RELATIVE_SOURCES ${RELATIVE_SHADER})
    endforeach()

    vela_compile_shaders(TARGET ${TARGET} SOURCES ${RELATIVE_SOURCES})
endfunction()
