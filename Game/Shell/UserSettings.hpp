#ifndef LOCKSTEP_GAME_SHELL_USER_SETTINGS_HPP
#define LOCKSTEP_GAME_SHELL_USER_SETTINGS_HPP

#include "Player/KeyBindings.hpp"

#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Net/Session.hpp>
#include <Lockstep/Render/PresentMode.hpp>

#include <cstdint>
#include <filesystem>
#include <string>

namespace lockstep::app
{
    struct EngineContext;
    struct GameState;
} //namespace lockstep::app

namespace game
{
    // Smallest and largest vertical field of view the options allow, in degrees.
    inline constexpr float k_minFieldOfView{50.0f};
    inline constexpr float k_maxFieldOfView{80.0f};

    // The player's options other than graphics quality (which GraphicsConfig keeps), stored in
    // the same settings file in the [audio], [controls], [accessibility], [display] and [player]
    // sections.
    // masterVolume .. musicVolume: bus volumes, 0 to 1.
    // mouseSensitivity: radians of look per pixel.
    // invertY:          mouse up looks down.
    // bindings:         every action's keys and buttons.
    // fieldOfView:      vertical field of view in degrees (k_minFieldOfView..k_maxFieldOfView).
    // subtitles:        dialogue subtitles on screen.
    // reduceHeadBob:    head bob, sway and roll cut to a third.
    // reduceMotionBlur: motion blur off.
    // skipIntro:        the startup intro is skipped ([accessibility] skip_intro, read by Intro).
    // fullscreen:       borderless fullscreen.
    // presentMode:      how frames reach the screen (VSync for players; `--present` wins).
    // windowWidth, windowHeight: the window size for the next start.
    // playerName:       the name shown in the lobby.
    // port:             the port Host listens on and Join connects to.
    // address:          the address Join connects to last.
    struct UserSettings
    {
        float masterVolume{1.0f};
        float worldVolume{1.0f};
        float playerVolume{1.0f};
        float ambienceVolume{1.0f};
        float musicVolume{1.0f};
        float mouseSensitivity{0.0025f};
        bool invertY{false};
        KeyBindings bindings{defaultKeyBindings()};
        float fieldOfView{60.0f};
        bool subtitles{true};
        bool reduceHeadBob{false};
        bool reduceMotionBlur{false};
        bool skipIntro{false};
        bool fullscreen{false};
        lockstep::render::PresentMode presentMode{lockstep::render::PresentMode::VSync};
        uint32_t windowWidth{1280};
        uint32_t windowHeight{720};
        std::string playerName{"Operator"};
        uint16_t port{lockstep::net::k_defaultPort};
        std::string address{"127.0.0.1"};
    };

    // Reads the options; missing keys keep their defaults, bad values are skipped.
    // file:    the settings file.
    // Returns: the options.
    [[nodiscard]] UserSettings loadUserSettings(const std::filesystem::path& file);

    // Writes the options into their sections, keeping everything else in the file (window size
    // through GraphicsConfig's saveWindowSize()).
    // file:     the settings file.
    // settings: the options.
    // Returns:  an error if the file could not be written.
    [[nodiscard]] lockstep::Status saveUserSettings(const std::filesystem::path& file, const UserSettings& settings);

    // Applies the options that take effect at once: bus volumes, look and bindings, field of view,
    // head motion, subtitles, fullscreen. (Motion blur is enforced each frame, see
    // enforceUserLook().)
    // settings: the options.
    // state:    the game state.
    // engine:   Engine services.
    void applyUserSettings(const UserSettings& settings, lockstep::app::GameState& state,
        lockstep::app::EngineContext& engine);

    // Keeps per-frame look options in force after level looks and lens effects changed the render
    // settings (motion blur off when reduced). Call at the end of each frame.
    // settings: the options.
    // engine:   Engine services.
    void enforceUserLook(const UserSettings& settings, lockstep::app::EngineContext& engine);
} //namespace game

#endif //LOCKSTEP_GAME_SHELL_USER_SETTINGS_HPP
