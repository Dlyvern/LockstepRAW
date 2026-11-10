# LockstepCooker: offline content cooker (glTF -> cooked files -> .pak). It links the Engine for
# its asset formats, glTF reading and PhysX cooking, and reads Engine/src private headers
# (cooked and pak formats, glTF helpers); the encoders (bc7enc, zstd compression) live only here,
# so the game never links them.
add_executable(LockstepCooker
    main.cpp
    BlockEncoders.cpp
    CookerArguments.cpp
    CookerOutput.cpp
    LimbCaps.cpp
    LimbSplit.cpp
    PakWriter.cpp
    SceneCook.cpp
    SceneSource.cpp
    SurfaceRules.cpp
    TextureCaps.cpp
    TextureCook.cpp
)

target_include_directories(LockstepCooker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/Engine/src
)

if(MSVC)
    target_compile_options(LockstepCooker PRIVATE /W4)
else()
    target_compile_options(LockstepCooker PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(LockstepCooker PRIVATE Lockstep::Engine bc7enc cgltf ktx_read zstd physx)

lockstep_stage_engine_runtime(LockstepCooker)
