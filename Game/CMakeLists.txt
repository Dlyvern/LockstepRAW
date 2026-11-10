add_executable(LockstepGame
    main.cpp
    Game.cpp
    DevPanel.cpp
    Audio/Footsteps.cpp
    Audio/Surfaces.cpp
    Characters/CharacterModel.cpp
    Characters/GearLooks.cpp
    Characters/Locomotion.cpp
    Characters/NpcPlaceholders.cpp
    Characters/PawnCharacters.cpp
    Characters/ViewModel.cpp
    Dialogue/Dialogue.cpp
    Echo/EchoRecording.cpp
    Enemies/Enemy.cpp
    Hospital/HospitalDevPanel.cpp
    Hospital/HospitalMission.cpp
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
    Npcs/NavGrid.cpp
    Npcs/NpcBrain.cpp
    Npcs/NpcFiring.cpp
    Npcs/NpcFollow.cpp
    Npcs/NpcGroups.cpp
    Npcs/NpcMessages.cpp
    Npcs/NpcMovement.cpp
    Npcs/NpcPositions.cpp
    Npcs/NpcPresentation.cpp
    Npcs/NpcRoster.cpp
    Npcs/NpcSenses.cpp
    Npcs/NpcTactics.cpp
    Player/LocalPlayer.cpp
    Player/PawnHealth.cpp
    Player/PawnLean.cpp
    Player/PawnMantle.cpp
    Player/PawnMovement.cpp
    Player/PawnRays.cpp
    Player/Pawns.cpp
    Player/PlayerCommand.cpp
    Player/PlayerStatus.cpp
    Player/Prediction.cpp
    Presentation/CombatHud.cpp
    Presentation/FirstPerson.cpp
    Presentation/FlyCamera.cpp
    Presentation/Frame.cpp
    Presentation/HeadMotion.cpp
    Presentation/Hud.cpp
    Presentation/LevelAmbience.cpp
    Presentation/LevelLights.cpp
    Presentation/LevelLook.cpp
    Presentation/Lighting.cpp
    Presentation/MaterialShowcase.cpp
    Presentation/SceneCharacters.cpp
    Presentation/Mirrors.cpp
    Presentation/Visuals.cpp
    Simulation/HospitalSimulation.cpp
    Simulation/HostSimulation.cpp
    Simulation/LevelFlow.cpp
    Simulation/NpcSimulation.cpp
    Simulation/PlayerCombat.cpp
    Simulation/RoomSimulation.cpp
    Simulation/StationSimulation.cpp
    Station/StationDevPanel.cpp
    Station/StationMission.cpp
    Station/StationState.cpp
    Weapons/Ammo.cpp
    Weapons/Hitscan.cpp
    Weapons/LagCompensation.cpp
    Weapons/WeaponStance.cpp
)

target_include_directories(LockstepGame PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Game text data (dialogue) lives in the repository's Data/ folder and is mounted as the
# lowest-priority content by default (main.cpp; `--data <dir>` overrides the folder).
target_compile_definitions(LockstepGame PRIVATE LOCKSTEP_DATA_DIR="${CMAKE_SOURCE_DIR}/Data")

if(MSVC)
    target_compile_options(LockstepGame PRIVATE /W4)
else()
    target_compile_options(LockstepGame PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(LockstepGame PRIVATE Lockstep::Engine)

lockstep_stage_engine_shaders(LockstepGame)
