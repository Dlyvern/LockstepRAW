add_executable(LockstepGame
    main.cpp
    Game.cpp
    DevPanel.cpp
    Characters/CharacterModel.cpp
    Characters/Locomotion.cpp
    Characters/PawnCharacters.cpp
    Characters/ViewModel.cpp
    Echo/EchoRecording.cpp
    Enemies/Enemy.cpp
    Interaction/Interactables.cpp
    Interaction/NodeMotion.cpp
    Interaction/ToyPhysics.cpp
    Interaction/Toys.cpp
    Level/ApartmentRoom.cpp
    Level/ApartmentRules.cpp
    Level/Arena.cpp
    Level/MorningRoutine.cpp
    Level/RoomDevPanel.cpp
    Level/RoomState.cpp
    Level/SceneLevel.cpp
    Player/LocalPlayer.cpp
    Player/Pawns.cpp
    Player/PlayerCommand.cpp
    Player/PlayerStatus.cpp
    Player/Prediction.cpp
    Presentation/FlyCamera.cpp
    Presentation/Frame.cpp
    Presentation/Hud.cpp
    Presentation/Lighting.cpp
    Presentation/MaterialShowcase.cpp
    Presentation/SceneCharacters.cpp
    Presentation/Mirrors.cpp
    Presentation/Visuals.cpp
    Simulation/HostSimulation.cpp
    Simulation/RoomSimulation.cpp
    Weapons/Hitscan.cpp
    Weapons/LagCompensation.cpp
    Weapons/WeaponStance.cpp
)

target_include_directories(LockstepGame PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(LockstepGame PRIVATE /W4)
else()
    target_compile_options(LockstepGame PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(LockstepGame PRIVATE Lockstep::Engine)

lockstep_stage_engine_shaders(LockstepGame)
