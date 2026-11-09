add_executable(LockstepGame
    main.cpp
    Game.cpp
)

if(MSVC)
    target_compile_options(LockstepGame PRIVATE /W4)
else()
    target_compile_options(LockstepGame PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(LockstepGame PRIVATE Lockstep::Engine)

lockstep_stage_engine_shaders(LockstepGame)
