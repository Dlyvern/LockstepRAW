add_executable(LockstepGame
    main.cpp
    Game.cpp
    DevPanel.cpp
    Enemies/Enemy.cpp
    Level/Arena.cpp
    Player/Pawns.cpp
    Player/PlayerCommand.cpp
    Player/PlayerStatus.cpp
    Player/Prediction.cpp
    Presentation/Lighting.cpp
    Presentation/Visuals.cpp
    Simulation/HostSimulation.cpp
    Weapons/Hitscan.cpp
    Weapons/LagCompensation.cpp
)

target_include_directories(LockstepGame PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(LockstepGame PRIVATE /W4)
else()
    target_compile_options(LockstepGame PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(LockstepGame PRIVATE Lockstep::Engine)

lockstep_stage_engine_shaders(LockstepGame)
