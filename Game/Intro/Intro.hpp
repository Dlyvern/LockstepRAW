#ifndef LOCKSTEP_GAME_INTRO_INTRO_HPP
#define LOCKSTEP_GAME_INTRO_INTRO_HPP

#include "Intro/DustLogoScene.hpp"
#include "Intro/IntroCards.hpp"
#include "Intro/IntroLaunch.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Audio/AudioTypes.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Render/RenderSettings.hpp>
#include <Lockstep/UI/UiContext.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace game
{
    // The cards of the startup intro, in the order they play.
    // Warning: the content warning (photosensitivity, headphones).
    // Vela:    "Made with Vela", the real-time dust and light scene.
    // Credits: the FMOD and PhysX card.
    // Title:   the body-cam "Lockstep" title.
    enum class IntroCard : uint8_t
    {
        Warning,
        Vela,
        Credits,
        Title
    };

    // The intro's sounds (Data/sounds/intro/). Any may be missing (invalid): the intro then plays
    // without it.
    struct IntroSounds
    {
        lockstep::audio::SoundHandle drone{};
        lockstep::audio::SoundHandle dark{};
        lockstep::audio::SoundHandle gust{};
        lockstep::audio::SoundHandle swell{};
        lockstep::audio::SoundHandle burst{};
        lockstep::audio::SoundHandle cut{};
    };

    // A voice of a card that has ended, fading out.
    // voice:     the voice.
    // volume:    its volume now.
    // remaining: seconds until it stops.
    struct FadingVoice
    {
        lockstep::audio::VoiceHandle voice{};
        float volume{1.0f};
        float remaining{0.0f};
    };

    // Dev captures of the intro by intro time (`--dev-capture-intro <seconds> <prefix>`, dev builds
    // only), since its frame rate varies too much for frame-numbered captures. Files are
    // <prefix>_<card>_<milliseconds into the card>.png.
    // step:    seconds between captures; 0 for none.
    // prefix:  path prefix of the files.
    // elapsed: intro seconds so far.
    // next:    intro seconds of the next capture.
    struct IntroCapture
    {
        float step{0.0f};
        std::string prefix;
        float elapsed{0.0f};
        float next{0.0f};
    };

    // The startup intro, played before the main menu: a sequence of cards, each fading in and out.
    // Any key skips the current card, Esc skips the rest (neither within a card's first
    // unskippable seconds). Presentation only: it runs from GameApi::frame on wall time.
    // cards:          the cards to play.
    // current:        index of the playing card.
    // seconds:        time since the playing card started.
    // skippedAt:      card time when a skip was asked for: the card fades out from there.
    // isSkippingAll:  Esc was pressed: no card follows the playing one.
    // firedCues:      bit per cue of the playing card already played.
    // voices:         the playing card's voices with their volumes.
    // fading:         voices of ended cards fading out.
    // frame:          frames presented by the intro (drives flicker and static).
    // scene:          the Vela card's 3D scene (created when that card starts).
    // textures:       the 2D cards' textures.
    // sounds:         the intro's sounds.
    // ui:             immediate-mode UI the cards draw with.
    // savedSettings:  the render settings to restore when the intro ends.
    // settingsFile:   the user settings file (the warning is remembered there).
    // capture:        dev captures by intro time.
    // ambienceVolume: the Ambience bus volume to restore (the bus is muted while the intro plays,
    //                 so the menu backdrop's ambience waits behind it).
    struct Intro
    {
        std::vector<IntroCard> cards;
        std::size_t current{0};
        float seconds{0.0f};
        std::optional<float> skippedAt{};
        bool isSkippingAll{false};
        uint32_t firedCues{0};
        std::vector<FadingVoice> voices;
        std::vector<FadingVoice> fading;
        uint64_t frame{0};
        std::optional<DustLogoScene> scene{};
        IntroCardTextures textures{};
        IntroSounds sounds{};
        lockstep::ui::UiContext ui{};
        lockstep::render::RenderSettings savedSettings{};
        std::filesystem::path settingsFile;
        float ambienceVolume{1.0f};
        IntroCapture capture{};
    };

    // Starts the intro if this launch plays one (see planIntro()): loads its sounds and textures,
    // switches the renderer to the intro's look and mutes the Ambience bus until it ends. Call
    // after the menu has set its own look, which the intro restores when it ends.
    // engine:  Engine services (arguments, renderer, audio, files).
    // Returns: the intro, nothing when this launch plays none, or the renderer's error.
    [[nodiscard]] lockstep::Result<std::optional<Intro>> startIntro(lockstep::app::EngineContext& engine);

    // Plays one frame of the intro: input, card changes, sounds, the 3D scene or 2D card. When the
    // last card ends it frees what the intro created and restores the render settings.
    // intro:   the intro.
    // engine:  Engine services.
    // frame:   this frame's input and wall time.
    // Returns: true while the intro still plays (the game draws nothing else this frame), false
    //          once it has ended (the intro may then be destroyed).
    [[nodiscard]] bool presentIntro(Intro& intro, lockstep::app::EngineContext& engine,
        const lockstep::app::FrameInfo& frame);
} //namespace game

#endif //LOCKSTEP_GAME_INTRO_INTRO_HPP
