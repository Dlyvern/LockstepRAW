add_library(LockstepEngine STATIC
    src/Anim/AnimationClip.cpp
    src/Anim/Animator.cpp
    src/Anim/BoneMask.cpp
    src/Anim/DrivenPose.cpp
    src/Anim/Inertialization.cpp
    src/Anim/JointEdits.cpp
    src/Anim/Limbs.cpp
    src/Anim/LocomotionPose.cpp
    src/Anim/MotionClips.cpp
    src/Anim/MotionDatabase.cpp
    src/Anim/MotionMatching.cpp
    src/Anim/RootMotion.cpp
    src/Anim/Skeleton.cpp
    src/Anim/SkinnedModel.cpp
    src/App/Run.cpp
    src/Assets/BlockCompression.cpp
    src/Assets/CookedFormat.cpp
    src/Assets/CookedIrradiance.cpp
    src/Assets/CookedLevelFile.cpp
    src/Assets/CookedModelFile.cpp
    src/Assets/CookedSceneLoad.cpp
    src/Assets/DirectoryMount.cpp
    src/Assets/FileSystem.cpp
    src/Assets/GltfAnimations.cpp
    src/Assets/GltfDocument.cpp
    src/Assets/GltfImport.cpp
    src/Assets/GltfMaterials.cpp
    src/Assets/GltfMeshes.cpp
    src/Assets/GltfNodes.cpp
    src/Assets/GltfSkins.cpp
    src/Assets/ImageDecode.cpp
    src/Assets/MeshProcessing.cpp
    src/Assets/MipChain.cpp
    src/Assets/PakFormat.cpp
    src/Assets/PakMount.cpp
    src/Assets/Scene.cpp
    src/Assets/SceneCollision.cpp
    src/Assets/SceneLights.cpp
    src/Assets/TextureImport.cpp
    src/Core/Bounds.cpp
    src/Core/FixedTimestep.cpp
    src/Core/Hash.cpp
    src/Core/Log.cpp
    src/Core/Result.cpp
    src/Core/Rotation.cpp
    src/Core/SettingsFile.cpp
    src/Core/Spring.cpp
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
    src/Physics/PhysicsRagdolls.cpp
    src/Physics/PhysicsWorld.cpp
    src/Physics/PhysXErrorReporter.cpp
    src/Physics/RagdollSkeleton.cpp
    src/Physics/RagdollState.cpp
    src/Platform/Input.cpp
    src/Platform/VelaInput.cpp
    src/Platform/Window.cpp
    src/Render/Atmosphere.cpp
    src/Render/CloudNoise.cpp
    src/Render/EnvironmentResources.cpp
    src/Render/IrradianceVolume.cpp
    src/Render/LightProfiles.cpp
    src/Render/FontAtlas.cpp
    src/Render/FrameCapture.cpp
    src/Render/ExposurePasses.cpp
    src/Render/FrameCulling.cpp
    src/Render/FrameLighting.cpp
    src/Render/LensFlare.cpp
    src/Render/SurfaceEffectsPacking.cpp
    src/Render/LensPasses.cpp
    src/Render/LensRain.cpp
    src/Render/LensRainPass.cpp
    src/Render/FrameGraph.cpp
    src/Render/FramePasses.cpp
    src/Render/GraphicsConfig.cpp
    src/Render/GraphicsQuality.cpp
    src/Render/IndirectLighting.cpp
    src/Render/ParticlePass.cpp
    src/Render/ParticleSystem.cpp
    src/Render/MaterialShading.cpp
    src/Render/PbrResources.cpp
    src/Render/PlanarReflections.cpp
    src/Render/PngWriter.cpp
    src/Render/PostPasses.cpp
    src/Render/PresentMode.cpp
    src/Render/Renderer.cpp
    src/Render/RendererResources.cpp
    src/Render/RendererState.cpp
    src/Render/RenderScale.cpp
    src/Render/ResourceLifetime.cpp
    src/Render/RenderPassAdapter.cpp
    src/Render/ScreenPass.cpp
    src/Render/ScopeView.cpp
    src/Render/ScreenSpaceReflections.cpp
    src/Render/ShaderLoader.cpp
    src/Render/ShadowCache.cpp
    src/Render/ShadowCascades.cpp
    src/Render/ShadowMaterials.cpp
    src/Render/ShadowPass.cpp
    src/Render/Shapes.cpp
    src/Render/Skinning.cpp
    src/Render/SkyModel.cpp
    src/Render/SurfaceTextures.cpp
    src/Render/TemporalAntiAliasing.cpp
    src/Render/OverlayPass.cpp
    src/Render/TextLayout.cpp
    src/Render/TextureLayout.cpp
    src/Render/VelaConversions.cpp
    src/UI/UiContext.cpp
    src/UI/UiTypes.cpp
    src/UI/UiWidgets.cpp
    src/Render/ViewModel.cpp
    src/Render/VolumetricFog.cpp
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
        src/Debug/AtmospherePanel.cpp
        src/Debug/DevTools.cpp
        src/Debug/EnginePanel.cpp
        src/Debug/FrameTimeHistory.cpp
        src/Debug/GpuTimingLog.cpp
        src/Debug/GraphicsLookPanel.cpp
        src/Debug/DetailLensPanel.cpp
        src/Debug/GraphicsPanel.cpp
        src/Debug/ImGuiInput.cpp
        src/Debug/ImGuiOverlayPass.cpp
        src/Debug/IndirectLightingPanel.cpp
        src/Debug/LightingFeaturesPanel.cpp
        src/Debug/NetPanel.cpp
        src/Debug/QualityPanel.cpp
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

# Audio (Engine/src/Audio). FMOD stays private: only Engine/src/Audio includes it. Without
# LOCKSTEP_WITH_FMOD the silent backend is built instead. The dev tools' Audio panel lives in
# Engine/src/Debug.
target_sources(LockstepEngine PRIVATE
    src/Audio/AudioMix.cpp
    src/Audio/AudioSystem.cpp
)

if(LOCKSTEP_WITH_FMOD)
    target_sources(LockstepEngine PRIVATE src/Audio/FmodBackend.cpp src/Audio/FmodPlayback.cpp)
    target_link_libraries(LockstepEngine PRIVATE fmod)
else()
    target_sources(LockstepEngine PRIVATE src/Audio/SilentBackend.cpp)
endif()

if(LOCKSTEP_DEV_TOOLS)
    target_sources(LockstepEngine PRIVATE src/Debug/AudioPanel.cpp)
endif()

# glTF import and pak reading stay private: only Engine/src/Assets includes cgltf, libktx (KTX2 and
# Basis Universal transcoding), zstd (pak decompression) and stb (stb_image's implementation is compiled into Vela::Assets;
# stb_dxt's into Engine/src/Assets/BlockCompression.cpp). stb's headers are treated as system
# headers so their warnings don't count as ours.
set_target_properties(stb_image PROPERTIES SYSTEM ON)
target_link_libraries(LockstepEngine PRIVATE cgltf ktx_read stb_image zstd)

# Shaders the Engine loads at runtime from "<exe dir>/shaders". They are compiled once for the
# Engine; a static library has no runtime directory of its own, so each executable that links
# the Engine stages them for itself (lockstep_stage_engine_shaders).
set(ENGINE_SHADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/unlit.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/unlit.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbr_skinned.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow_mask.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow_mask.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow_skinned.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow_mask_skinned.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sky.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sky.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/environment.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/volumetric_fog.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/volumetric_fog_temporal.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/volumetric_fog_composite.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/clouds.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/clouds_composite.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lens_rain.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lens_rain.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gtao.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/occlusion_blur.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ssr_trace.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ssr_resolve.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ssr_source.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/indirect_composite.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lens_flare.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/bloom_up_flare.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/bloom_prefilter.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/bloom_down.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/bloom_up.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/luminance.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/luminance_reduce.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/exposure.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tonemap.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/taa.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fxaa.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/overlay.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/overlay.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.frag
)

if(LOCKSTEP_DEV_TOOLS)
    list(APPEND ENGINE_SHADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/imgui.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/imgui.frag
    )
endif()

set(ENGINE_SHADER_SOURCES "")

foreach(SHADER ${ENGINE_SHADERS})
    file(RELATIVE_PATH RELATIVE_SHADER ${CMAKE_CURRENT_SOURCE_DIR} ${SHADER})
    list(APPEND ENGINE_SHADER_SOURCES ${RELATIVE_SHADER})
endforeach()

vela_compile_shaders(TARGET LockstepEngine SOURCES ${ENGINE_SHADER_SOURCES})

# Fonts the Engine loads at runtime from "<exe dir>/fonts" (screen text), with their licences,
# which the SIL Open Font License requires to ship alongside.
set(LOCKSTEP_ENGINE_FONTS
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/NotoSans-Regular.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/OFL.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/Cousine-Regular.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/Cousine-OFL.txt
    CACHE INTERNAL "Fonts the Engine stages next to each executable"
)

# Stages the Engine's compiled shaders and the Engine fonts next to TARGET's executable.
function(lockstep_stage_engine_shaders TARGET)
    vela_stage_shaders(TARGET ${TARGET} FROM LockstepEngine)

    foreach(FONT IN LISTS LOCKSTEP_ENGINE_FONTS)
        get_filename_component(FONT_NAME "${FONT}" NAME)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET}>/fonts"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FONT}" "$<TARGET_FILE_DIR:${TARGET}>/fonts/${FONT_NAME}"
            COMMENT "Staging ${FONT_NAME} for ${TARGET}"
        )
    endforeach()

    lockstep_stage_engine_runtime(${TARGET})
endfunction()

# Copies the runtime libraries the Engine needs next to TARGET's executable: the PhysX DLLs on
# Windows (PhysX_64, PhysXCommon_64, PhysXFoundation_64, PhysXCooking_64), as Velix does.
function(lockstep_stage_engine_runtime TARGET)
    lockstep_stage_audio_runtime(${TARGET})

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

# Copies the FMOD runtime next to TARGET's executable (fmod.dll on Windows; libfmod.so.14.8 with
# its libfmod.so.14 and libfmod.so links on Linux, as Velix does) and lets the Linux executable
# find it there (RPATH $ORIGIN). Does nothing without LOCKSTEP_WITH_FMOD.
function(lockstep_stage_audio_runtime TARGET)
    if(NOT LOCKSTEP_WITH_FMOD)
        return()
    endif()

    get_filename_component(FMOD_RUNTIME_NAME "${FMOD_RUNTIME_LIBRARY}" NAME)

    if(WIN32)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FMOD_RUNTIME_LIBRARY}" "$<TARGET_FILE_DIR:${TARGET}>/${FMOD_RUNTIME_NAME}"
            COMMENT "Staging ${FMOD_RUNTIME_NAME} for ${TARGET}"
        )
    else()
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FMOD_RUNTIME_LIBRARY}" "$<TARGET_FILE_DIR:${TARGET}>/${FMOD_RUNTIME_NAME}"
            COMMAND ${CMAKE_COMMAND} -E remove -f
                "$<TARGET_FILE_DIR:${TARGET}>/libfmod.so.14" "$<TARGET_FILE_DIR:${TARGET}>/libfmod.so"
            COMMAND ${CMAKE_COMMAND} -E create_symlink "${FMOD_RUNTIME_NAME}" "$<TARGET_FILE_DIR:${TARGET}>/libfmod.so.14"
            COMMAND ${CMAKE_COMMAND} -E create_symlink "libfmod.so.14" "$<TARGET_FILE_DIR:${TARGET}>/libfmod.so"
            COMMENT "Staging ${FMOD_RUNTIME_NAME} for ${TARGET}"
        )
        set_property(TARGET ${TARGET} APPEND PROPERTY BUILD_RPATH "$ORIGIN")
        set_property(TARGET ${TARGET} APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
    endif()
endfunction()
