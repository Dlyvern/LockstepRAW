#ifndef LOCKSTEP_GAME_SHELL_MENU_BACKDROP_HPP
#define LOCKSTEP_GAME_SHELL_MENU_BACKDROP_HPP

#include "Weather/Weather.hpp"

#include <Lockstep/Audio/AudioTypes.hpp>
#include <Lockstep/Core/Math.hpp>

#include <optional>

namespace lockstep::app
{
    struct EngineContext;
    struct GameState;
} //namespace lockstep::app

namespace game
{
    namespace math = lockstep::math;

    // The main menu's live background: the hospital A alley at sunset in the rain (police strobes,
    // fog, NPC placeholders), seen from a slowly drifting camera near the player start; no pawn,
    // no AI, no HUD. Rain, distant sirens and the street play as ambience. Without the level (its
    // pak not mounted) the menu shows a dark gradient instead.
    // isLoaded:     the level is loaded into GameState::level.
    // anchor:       the camera path's centre (above PlayerStart_1).
    // forward:      the horizontal direction the camera looks (the start's facing).
    // seconds:      real seconds along the camera path.
    // rain, siren, street: the ambience voices while the menu shows.
    // savedWeather: the `--weather` choice to restore when a game starts.
    struct MenuBackdrop
    {
        bool isLoaded{false};
        math::Vec3 anchor{0.0f, 1.7f, 0.0f};
        math::Vec3 forward{0.0f, 0.0f, -1.0f};
        float seconds{0.0f};
        std::optional<lockstep::audio::VoiceHandle> rain{};
        std::optional<lockstep::audio::VoiceHandle> siren{};
        std::optional<lockstep::audio::VoiceHandle> street{};
        std::optional<WeatherKind> savedWeather{};
    };

    // Loads the background level (if mounted), forces sunset and rain, starts the ambience.
    // state:  the game state (its level must be unloaded).
    // engine: Engine services.
    void loadMenuBackdrop(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Unloads the background level and stops the ambience; restores the weather choice.
    // state:  the game state.
    // engine: Engine services.
    void unloadMenuBackdrop(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Draws the background: the level, its lights, weather and effects, the placeholders and the
    // camera. Without the level only the camera is set (the caller draws the gradient).
    // state:        the game state.
    // engine:       Engine services.
    // frameSeconds: real seconds since the last frame.
    void presentMenuBackdrop(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        float frameSeconds);
} //namespace game

#endif //LOCKSTEP_GAME_SHELL_MENU_BACKDROP_HPP
