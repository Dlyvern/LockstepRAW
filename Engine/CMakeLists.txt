add_library(LockstepEngine STATIC
    src/App/Run.cpp
    src/Assets/BlockCompression.cpp
    src/Assets/CookedFormat.cpp
    src/Assets/CookedLevelFile.cpp
    src/Assets/CookedSceneLoad.cpp
    src/Assets/DirectoryMount.cpp
    src/Assets/FileSystem.cpp
    src/Assets/GltfDocument.cpp
    src/Assets/GltfImport.cpp
    src/Assets/GltfMaterials.cpp
    src/Assets/GltfMeshes.cpp
    src/Assets/GltfNodes.cpp
    src/Assets/ImageDecode.cpp
    src/Assets/MeshProcessing.cpp
    src/Assets/MipChain.cpp
    src/Assets/PakFormat.cpp
    src/Assets/PakMount.cpp
    src/Assets/Scene.cpp
    src/Assets/SceneCollision.cpp
    src/Assets/TextureImport.cpp
    src/Core/Bounds.cpp
    src/Core/FixedTimestep.cpp
    src/Core/Hash.cpp
    src/Core/Log.cpp
    src/Core/Result.cpp
    src/Core/Transform.cpp
    src/Core/VelaError.cpp
    src/Core/VelaLog.cpp
    src/Net/InputBuffer.cpp
    src/Net/Network.cpp
    src/Net/Replication.cpp
    src/Net/ReplicationInputs.cpp
    src/Net/Session.cpp
    src/Net/SessionConnection.cpp
    src/Net/SessionLaunch.cpp
    src/Net/SnapshotBuffer.cpp
    src/Net/SnapshotFormat.cpp
    src/Net/WireFormat.cpp
    src/Physics/MeshCooker.cpp
    src/Physics/PhysicsBodies.cpp
    src/Physics/PhysicsControllers.cpp
    src/Physics/PhysicsMeshes.cpp
    src/Physics/PhysicsQueries.cpp
    src/Physics/PhysicsWorld.cpp
    src/Physics/PhysXErrorReporter.cpp
    src/Platform/Input.cpp
    src/Platform/VelaInput.cpp
    src/Platform/Window.cpp
    src/Render/EnvironmentResources.cpp
    src/Render/FrameCulling.cpp
    src/Render/FrameLighting.cpp
    src/Render/FramePasses.cpp
    src/Render/PbrResources.cpp
    src/Render/PostPasses.cpp
    src/Render/PresentMode.cpp
    src/Render/Renderer.cpp
    src/Render/RendererResources.cpp
    src/Render/RendererState.cpp
    src/Render/RenderPassAdapter.cpp
    src/Render/ShaderLoader.cpp
    src/Render/ShadowCascades.cpp
    src/Render/ShadowMaterials.cpp
    src/Render/Shapes.cpp
    src/Render/SkyModel.cpp
    src/Render/TextureLayout.cpp
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
        src/Debug/FrameTimeHistory.cpp
        src/Debug/GraphicsPanel.cpp
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
# The rest of Vela stays private.
target_link_libraries(LockstepEngine
    PUBLIC
        Vela::Math
    PRIVATE
        Vela::Vela Vela::Builtins Vela::Assets
)

# GameNetworkingSockets stays private: only Engine/src/Net includes it.
target_link_libraries(LockstepEngine PRIVATE GameNetworkingSockets::static)

# PhysX stays private: only Engine/src/Physics includes it.
target_link_libraries(LockstepEngine PRIVATE physx)

# glTF import and pak reading stay private: only Engine/src/Assets includes cgltf, libktx (KTX2 and
# Basis Universal transcoding), zstd (pak decompression) and stb (stb_image's implementation is compiled into Vela::Assets;
# stb_dxt's into Engine/src/Assets/BlockCompression.cpp). stb's headers are treated as system
# headers so their warnings don't count as ours.
set_target_properties(stb_image PROPERTIES SYSTEM ON)
target_link_libraries(LockstepEngine PRIVATE cgltf ktx_read stb_image zstd)

# Shaders the Engine loads at runtime from "<exe dir>/shaders". A static library has no runtime
# directory of its own, so each executable that links the Engine stages them for itself.
set(ENGINE_SHADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/unlit.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/unlit.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow_mask.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow_mask.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sky.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sky.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/environment.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ssao.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ssao_blur.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/bloom_prefilter.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/bloom_down.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/bloom_up.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tonemap.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fxaa.frag
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

    lockstep_stage_engine_runtime(${TARGET})
endfunction()

# Copies the runtime libraries the Engine needs next to TARGET's executable: the PhysX DLLs on
# Windows (PhysX_64, PhysXCommon_64, PhysXFoundation_64, PhysXCooking_64), as Velix does.
function(lockstep_stage_engine_runtime TARGET)
    if(WIN32 AND PHYSX_RUNTIME_DLLS)
        foreach(RUNTIME_DLL IN LISTS PHYSX_RUNTIME_DLLS)
            get_filename_component(RUNTIME_DLL_NAME "${RUNTIME_DLL}" NAME)
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${RUNTIME_DLL}" "$<TARGET_FILE_DIR:${TARGET}>/${RUNTIME_DLL_NAME}"
                COMMENT "Staging ${RUNTIME_DLL_NAME} for ${TARGET}"
            )
        endforeach()
    endif()
endfunction()
