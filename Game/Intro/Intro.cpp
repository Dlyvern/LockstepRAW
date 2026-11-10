#include "Intro/Intro.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Audio/AudioSystem.hpp>
#include <Lockstep/Platform/Input.hpp>
#include <Lockstep/Render/GraphicsConfig.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <span>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace game
{
    namespace
    {
        namespace audio = lockstep::audio;
        namespace platform = lockstep::platform;
        namespace render = lockstep::render;
        namespace ui = lockstep::ui;

        using lockstep::app::EngineContext;
        using lockstep::app::FrameInfo;

        // How one card plays: its length and fades in seconds, and how long it ignores skips.
        struct CardTiming
        {
            float length{0.0f};
            float fadeIn{0.0f};
            float fadeOut{0.0f};
            float unskippable{0.0f};
        };

        // A sound a card starts at a time.
        struct Cue
        {
            IntroCard card{IntroCard::Warning};
            float seconds{0.0f};
            audio::SoundHandle IntroSounds::*sound{nullptr};
            audio::Bus bus{audio::Bus::Ui};
            float volume{1.0f};
        };

        constexpr DustLogoTiming k_vela{};
        constexpr CardTiming k_warningTiming{.length = 4.6f, .fadeIn = 0.7f, .fadeOut = 0.7f, .unskippable = 1.0f};
        constexpr CardTiming k_velaTiming{.length = k_vela.length, .fadeIn = 0.6f, .fadeOut = 0.9f, .unskippable = 0.4f};
        constexpr CardTiming k_creditsTiming{.length = 4.2f, .fadeIn = 0.7f, .fadeOut = 0.7f, .unskippable = 0.4f};
        constexpr CardTiming k_titleTiming{.length = k_titleCutSeconds + 0.45f, .unskippable = 0.3f};
        constexpr float k_skipFadeSeconds{0.35f};
        constexpr float k_voiceFadeSeconds{0.4f};
        constexpr float k_maxFrameSeconds{0.1f};
        constexpr float k_beamOnSeconds{0.9f};
        constexpr float k_beamOffStart{k_vela.gust + 1.3f};
        constexpr float k_beamOffEnd{k_vela.length - 0.3f};
        constexpr float k_captionIn{k_vela.holdStart - 0.5f};
        constexpr float k_captionOut{k_vela.gust - 0.1f};
        constexpr float k_captionFade{0.6f};
        constexpr std::string_view k_captureFlag{"--dev-capture-intro"};

        constexpr std::array k_cues{
            Cue{IntroCard::Warning, 0.0f, &IntroSounds::drone, audio::Bus::Music, 0.55f},
            Cue{IntroCard::Vela, 0.0f, &IntroSounds::dark, audio::Bus::Music, 0.7f},
            Cue{IntroCard::Vela, k_vela.gust - 0.25f, &IntroSounds::gust, audio::Bus::Ui, 0.8f},
            Cue{IntroCard::Credits, 0.0f, &IntroSounds::swell, audio::Bus::Music, 0.6f},
            Cue{IntroCard::Title, 0.0f, &IntroSounds::burst, audio::Bus::Ui, 0.8f},
            Cue{IntroCard::Title, k_titleCutSeconds, &IntroSounds::cut, audio::Bus::Ui, 0.8f},
        };

        constexpr std::array<std::pair<audio::SoundHandle IntroSounds::*, std::string_view>, 6> k_soundFiles{{
            {&IntroSounds::drone, "warning_drone"},
            {&IntroSounds::dark, "vela_dark"},
            {&IntroSounds::gust, "vela_gust"},
            {&IntroSounds::swell, "credits_swell"},
            {&IntroSounds::burst, "title_static"},
            {&IntroSounds::cut, "title_cut"},
        }};

        const CardTiming& timingOf(IntroCard card)
        {
            switch (card)
            {
            case IntroCard::Warning:
                return k_warningTiming;
            case IntroCard::Vela:
                return k_velaTiming;
            case IntroCard::Credits:
                return k_creditsTiming;
            case IntroCard::Title:
                break;
            }

            return k_titleTiming;
        }

        float ramp(float value, float from, float to)
        {
            if (to <= from)
            {
                return value >= from ? 1.0f : 0.0f;
            }

            return std::clamp((value - from) / (to - from), 0.0f, 1.0f);
        }

        IntroSounds loadSounds(EngineContext& engine)
        {
            IntroSounds sounds;

            for (const auto& [member, stem] : k_soundFiles)
            {
                const std::string path = "sounds/intro/" + std::string(stem) + ".ogg";
                auto sound = engine.audio.loadSound(engine.files, path, audio::SoundDescription{.is3d = false});

                if (!sound)
                {
                    std::clog << "Game: intro sound '" << path << "' not loaded: " << sound.error().message << '\n';
                    continue;
                }

                sounds.*member = sound.value();
            }

            return sounds;
        }

        void releaseSounds(IntroSounds& sounds, audio::AudioSystem& audio)
        {
            for (const auto& [member, stem] : k_soundFiles)
            {
                audio.destroySound(sounds.*member);
            }

            sounds = {};
        }

        void applyIntroLook(render::RenderSettings& settings)
        {
            settings.sky.enabled = false;
            settings.ibl.enabled = false;
            settings.interiorAmbient.enabled = false;
            settings.lighting.ambientIntensity = 0.0f;
            settings.fog.enabled = false;
            settings.ambientOcclusion.enabled = false;
            settings.contactShadows.enabled = false;
            settings.screenSpaceReflections.enabled = false;
            settings.autoExposure.enabled = false;
            settings.tonemap.exposure = 1.0f;
            settings.bloom.enabled = true;
            settings.bloom.strength = 0.06f;
            settings.colorGrading = render::ColorGradingSettings{};
            settings.vignette = render::VignetteSettings{.intensity = 0.35f};
            settings.filmGrain = render::FilmGrainSettings{.intensity = 0.04f};
            settings.depthOfField.enabled = false;
            settings.motionBlur.enabled = false;
            settings.chromaticAberration.enabled = false;
            settings.lensDirt.enabled = false;
            settings.wetness.enabled = false;
            settings.volumetricFog = render::VolumetricFogSettings{.enabled = true,
                .density = 0.045f,
                .albedo = {0.85f, 0.85f, 0.85f},
                .anisotropy = 0.55f,
                .heightFalloff = 0.12f,
                .ambientIntensity = 0.0f,
                .maxDistance = 16.0f,
                .steps = 32};
        }

        void startCard(Intro& intro, EngineContext& engine)
        {
            intro.seconds = 0.0f;
            intro.skippedAt.reset();
            intro.firedCues = 0;
            std::clog << "Game: intro card " << intro.current << " at frame " << intro.frame << '\n';

            if (intro.cards[intro.current] != IntroCard::Vela)
            {
                return;
            }

            auto scene = createDustLogoScene(engine.renderer);

            if (!scene)
            {
                std::clog << "Game: intro scene not created: " << scene.error().message << '\n';
                return;
            }

            intro.scene = std::move(scene).value();
        }

        void releaseCardVoices(Intro& intro)
        {
            for (const FadingVoice& voice : intro.voices)
            {
                intro.fading.push_back(FadingVoice{.voice = voice.voice, .volume = voice.volume,
                    .remaining = k_voiceFadeSeconds});
            }

            intro.voices.clear();
        }

        void endCard(Intro& intro, EngineContext& engine)
        {
            releaseCardVoices(intro);

            if (intro.scene)
            {
                destroyDustLogoScene(*intro.scene, engine.renderer);
                intro.scene.reset();
            }

            if (intro.cards[intro.current] == IntroCard::Warning)
            {
                if (auto saved = saveWarningSeen(intro.settingsFile); !saved)
                {
                    std::clog << "Game: could not remember the content warning: " << saved.error().message << '\n';
                }
            }
        }

        void finishIntro(Intro& intro, EngineContext& engine)
        {
            for (const FadingVoice& voice : intro.fading)
            {
                engine.audio.stop(voice.voice);
            }

            intro.fading.clear();
            releaseSounds(intro.sounds, engine.audio);
            destroyIntroCardTextures(intro.textures, engine.renderer);
            engine.renderer.settings() = intro.savedSettings;
            engine.audio.setBusVolume(audio::Bus::Ambience, intro.ambienceVolume);
            std::clog << "Game: intro finished\n";
        }

        bool anyKeyPressed(const platform::Input& input)
        {
            for (std::size_t key = 0; key < static_cast<std::size_t>(platform::Key::Count); ++key)
            {
                if (input.wasKeyPressed(static_cast<platform::Key>(key)))
                {
                    return true;
                }
            }

            for (std::size_t button = 0; button < static_cast<std::size_t>(platform::MouseButton::Count); ++button)
            {
                if (input.wasMouseButtonPressed(static_cast<platform::MouseButton>(button)))
                {
                    return true;
                }
            }

            return false;
        }

        void readSkips(Intro& intro, const platform::Input& input)
        {
            if (intro.skippedAt || intro.seconds < timingOf(intro.cards[intro.current]).unskippable)
            {
                return;
            }

            if (input.wasKeyPressed(platform::Key::Escape))
            {
                intro.skippedAt = intro.seconds;
                intro.isSkippingAll = true;
                std::clog << "Game: intro skipped\n";
            }
            else if (anyKeyPressed(input))
            {
                intro.skippedAt = intro.seconds;
            }
        }

        void playCues(Intro& intro, EngineContext& engine)
        {
            if (intro.skippedAt)
            {
                return;
            }

            for (std::size_t index = 0; index < k_cues.size(); ++index)
            {
                const Cue& cue = k_cues[index];
                const uint32_t bit = 1u << index;

                if (cue.card != intro.cards[intro.current] || intro.seconds < cue.seconds || (intro.firedCues & bit) != 0)
                {
                    continue;
                }

                intro.firedCues |= bit;
                const audio::SoundHandle sound = intro.sounds.*cue.sound;

                if (!sound.isValid())
                {
                    continue;
                }

                if (auto voice = engine.audio.play(sound, audio::PlayDescription{.bus = cue.bus, .volume = cue.volume}))
                {
                    intro.voices.push_back(FadingVoice{.voice = voice.value(), .volume = cue.volume});
                }
                else
                {
                    std::clog << "Game: intro sound not played: " << voice.error().message << '\n';
                }
            }
        }

        void updateVoices(Intro& intro, audio::AudioSystem& audio, float skipVolume, float seconds)
        {
            for (const FadingVoice& voice : intro.voices)
            {
                audio.setVoiceVolume(voice.voice, voice.volume * skipVolume);
            }

            for (FadingVoice& voice : intro.fading)
            {
                voice.remaining -= seconds;
                audio.setVoiceVolume(voice.voice, voice.volume * std::max(0.0f, voice.remaining / k_voiceFadeSeconds));

                if (voice.remaining <= 0.0f)
                {
                    audio.stop(voice.voice);
                }
            }

            std::erase_if(intro.fading, [](const FadingVoice& voice)
            {
                return voice.remaining <= 0.0f;
            });
        }

        float cardEnd(const Intro& intro)
        {
            const CardTiming& timing = timingOf(intro.cards[intro.current]);
            return intro.skippedAt ? std::min(timing.length, *intro.skippedAt + k_skipFadeSeconds) : timing.length;
        }

        float skipFade(const Intro& intro)
        {
            return intro.skippedAt ? 1.0f - ramp(intro.seconds, *intro.skippedAt, *intro.skippedAt + k_skipFadeSeconds)
                                   : 1.0f;
        }

        float cardOpacity(const Intro& intro)
        {
            const CardTiming& timing = timingOf(intro.cards[intro.current]);
            const float fadeIn = ramp(intro.seconds, 0.0f, timing.fadeIn);
            const float fadeOut = 1.0f - ramp(intro.seconds, timing.length - timing.fadeOut, timing.length);
            return std::min({fadeIn, fadeOut, skipFade(intro)});
        }

        void drawFadeOverlay(ui::UiContext& context, float opacity)
        {
            context.setOpacity(1.0f - opacity);
            drawIntroBackdrop(context);
            context.setOpacity(1.0f);
        }

        void drawVelaCard(Intro& intro, EngineContext& engine, float opacity, float delta)
        {
            if (intro.scene)
            {
                const float beam = ramp(intro.seconds, 0.0f, k_beamOnSeconds)
                    * (1.0f - ramp(intro.seconds, k_beamOffStart, k_beamOffEnd));
                presentDustLogoScene(*intro.scene, engine.renderer, intro.seconds, delta, beam);
            }

            const float caption = ramp(intro.seconds, k_captionIn, k_captionIn + k_captionFade)
                * (1.0f - ramp(intro.seconds, k_captionOut, k_captionOut + k_captionFade * 0.6f));
            intro.ui.setOpacity(caption * opacity);
            drawVelaCaption(intro.ui);
            drawFadeOverlay(intro.ui, opacity);
        }

        void drawCard(Intro& intro, EngineContext& engine, float delta)
        {
            const float opacity = cardOpacity(intro);

            switch (intro.cards[intro.current])
            {
            case IntroCard::Warning:
                drawIntroBackdrop(intro.ui);
                intro.ui.setOpacity(opacity);
                drawWarningCard(intro.ui);
                break;
            case IntroCard::Vela:
                drawVelaCard(intro, engine, opacity, delta);
                break;
            case IntroCard::Credits:
                drawIntroBackdrop(intro.ui);
                intro.ui.setOpacity(opacity);
                drawCreditsCard(intro.ui, intro.textures);
                break;
            case IntroCard::Title:
                drawTitleCard(intro.ui, intro.textures, intro.seconds, intro.frame);
                drawFadeOverlay(intro.ui, opacity);
                break;
            }

            intro.ui.setOpacity(1.0f);
        }

        IntroCapture parseCapture(std::span<const char* const> arguments)
        {
            IntroCapture capture;
#if LOCKSTEP_DEV_TOOLS
            for (std::size_t index = 0; index + 2 < arguments.size(); ++index)
            {
                if (arguments[index] == nullptr || std::string_view(arguments[index]) != k_captureFlag
                    || arguments[index + 1] == nullptr || arguments[index + 2] == nullptr)
                {
                    continue;
                }

                capture.step = std::max(0.0f, std::strtof(arguments[index + 1], nullptr));
                capture.prefix = arguments[index + 2];
            }
#else
            static_cast<void>(arguments);
#endif
            return capture;
        }

        void updateCapture(Intro& intro, render::Renderer& renderer, float delta)
        {
            IntroCapture& capture = intro.capture;

            if (capture.step > 0.0f && capture.elapsed >= capture.next)
            {
                const std::string path = capture.prefix + "_" + std::to_string(intro.current) + "_"
                    + std::to_string(static_cast<int>(intro.seconds * 1000.0f)) + ".png";

                if (renderer.captureNextFrame(path))
                {
                    capture.next += capture.step;
                }
            }

            capture.elapsed += delta;
        }

        std::vector<IntroCard> cardsOf(IntroPlan plan)
        {
            if (plan == IntroPlan::WarningOnly)
            {
                return {IntroCard::Warning};
            }

            return {IntroCard::Warning, IntroCard::Vela, IntroCard::Credits, IntroCard::Title};
        }
    } //namespace

    lockstep::Result<std::optional<Intro>> startIntro(EngineContext& engine)
    {
        const std::filesystem::path settingsFile = render::graphicsConfigPath(engine.arguments);
        const IntroPlan plan = planIntro(engine.arguments, loadIntroPreferences(settingsFile));

        if (plan == IntroPlan::None)
        {
            return std::optional<Intro>{};
        }

        auto textures = createIntroCardTextures(engine.renderer, engine.files);

        if (!textures)
        {
            return textures.error();
        }

        std::optional<Intro> intro(std::in_place);
        intro->cards = cardsOf(plan);
        intro->textures = textures.value();
        intro->sounds = loadSounds(engine);
        intro->settingsFile = settingsFile;
        intro->capture = parseCapture(engine.arguments);
        intro->savedSettings = engine.renderer.settings();
        applyIntroLook(engine.renderer.settings());
        intro->ambienceVolume = engine.audio.busVolume(audio::Bus::Ambience);
        engine.audio.setBusVolume(audio::Bus::Ambience, 0.0f);
        startCard(*intro, engine);
        std::clog << "Game: intro plays " << intro->cards.size() << " card(s)\n";
        return intro;
    }

    bool presentIntro(Intro& intro, EngineContext& engine, const FrameInfo& frame)
    {
        const float delta = std::clamp(frame.frameSeconds, 0.0f, k_maxFrameSeconds);
        ++intro.frame;
        readSkips(intro, frame.input);

        while (intro.seconds >= cardEnd(intro))
        {
            endCard(intro, engine);
            ++intro.current;

            if (intro.isSkippingAll || intro.current >= intro.cards.size())
            {
                finishIntro(intro, engine);
                return false;
            }

            startCard(intro, engine);
        }

        playCues(intro, engine);
        intro.ui.begin(engine.renderer, frame.input, frame.frameSeconds, false);
        drawCard(intro, engine, delta);
        intro.ui.end();
        updateVoices(intro, engine.audio, skipFade(intro), delta);
        updateCapture(intro, engine.renderer, delta);
        intro.seconds += delta;
        return true;
    }
} //namespace game
