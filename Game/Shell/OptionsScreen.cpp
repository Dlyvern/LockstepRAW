#include "Shell/ShellScreens.hpp"

#include "GameState.hpp"

#include <Lockstep/Platform/Window.hpp>
#include <Lockstep/Render/GraphicsConfig.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;
        namespace ui = lockstep::ui;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr float k_columnWidth{900.0f};
        constexpr float k_tabHeight{46.0f};
        constexpr float k_noticeSeconds{4.0f};
        constexpr std::array<std::pair<uint32_t, uint32_t>, 7> k_windowSizes{{{960, 540}, {1280, 720}, {1600, 900},
            {1920, 1080}, {2560, 1440}, {3440, 1440}, {3840, 2160}}};
        constexpr std::array<std::string_view, 4> k_tabNames{"GRAPHICS", "AUDIO", "CONTROLS", "ACCESSIBILITY"};
        constexpr std::array<render::GraphicsPreset, 4> k_presets{render::GraphicsPreset::Low,
            render::GraphicsPreset::Medium, render::GraphicsPreset::High, render::GraphicsPreset::Ultra};

        void notify(OptionsState& options, std::string text)
        {
            options.notice = std::move(text);
            options.noticeSeconds = k_noticeSeconds;
        }

        void saveUser(GameState& state, EngineContext& engine)
        {
            Shell& shell = state.shell;
            applyUserSettings(shell.settings, state, engine);

            if (auto saved = saveUserSettings(shell.settingsFile, shell.settings); !saved)
            {
                notify(shell.options, "NOT SAVED: " + saved.error().message);
            }
        }

        void saveQuality(Shell& shell, EngineContext& engine, bool isPresetPick)
        {
            render::GraphicsQuality& quality = shell.options.quality;

            if (!isPresetPick)
            {
                quality.preset = render::GraphicsPreset::Custom;
            }

            engine.renderer.setGraphicsQuality(quality);

            if (auto saved = render::saveGraphicsQuality(shell.settingsFile, quality); !saved)
            {
                notify(shell.options, "NOT SAVED: " + saved.error().message);
                return;
            }

            if (isPresetPick)
            {
                notify(shell.options, "TEXTURE DETAIL APPLIES WHEN THE NEXT LEVEL LOADS");
            }
        }

        std::string percent(float value)
        {
            return std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%";
        }

        void qualityToggle(GameState& state, EngineContext& engine, ui::Column& column, std::string_view id,
            std::string_view label, bool& field)
        {
            if (state.shell.ui.toggle(id, column.next(k_menuRowHeight), label, field))
            {
                saveQuality(state.shell, engine, false);
            }
        }

        void drawGraphics(GameState& state, EngineContext& engine, ui::Column& column)
        {
            Shell& shell = state.shell;
            ui::UiContext& context = shell.ui;
            render::GraphicsQuality& quality = shell.options.quality;
            const std::vector<std::string> presetNames{"LOW", "MEDIUM", "HIGH", "ULTRA", "CUSTOM"};
            std::size_t preset = static_cast<std::size_t>(quality.preset);

            if (context.cycler("opt.preset", column.next(k_menuRowHeight), "QUALITY PRESET", preset,
                    std::span<const std::string>(presetNames.data(), quality.preset == render::GraphicsPreset::Custom ? 5 : 4)))
            {
                const render::GraphicsPreset picked = k_presets[preset % k_presets.size()];
                quality = render::graphicsQualityPreset(picked);
                quality.preset = picked;
                saveQuality(shell, engine, true);
            }

            const std::vector<std::string> antiAliasing{"OFF", "FXAA", "MSAA 2X", "MSAA 4X", "TAA"};
            std::size_t mode = static_cast<std::size_t>(quality.antiAliasing);

            if (context.cycler("opt.aa", column.next(k_menuRowHeight), "ANTI-ALIASING", mode, antiAliasing))
            {
                quality.antiAliasing = static_cast<render::AntiAliasingMode>(mode);
                saveQuality(shell, engine, false);
            }

            if (context.slider("opt.scale", column.next(k_menuRowHeight), "RENDER SCALE", quality.renderScale,
                    render::k_minRenderScale, render::k_maxRenderScale, 0.05f, percent(quality.renderScale)))
            {
                saveQuality(shell, engine, false);
            }

            qualityToggle(state, engine, column, "opt.fog", "VOLUMETRIC FOG", quality.volumetricFog);
            qualityToggle(state, engine, column, "opt.ssr", "SCREEN-SPACE REFLECTIONS", quality.screenSpaceReflections);
            qualityToggle(state, engine, column, "opt.ao", "AMBIENT OCCLUSION", quality.ambientOcclusion);
            qualityToggle(state, engine, column, "opt.contact", "CONTACT SHADOWS", quality.contactShadows);
            qualityToggle(state, engine, column, "opt.bloom", "BLOOM", quality.bloom);
            qualityToggle(state, engine, column, "opt.blur", "MOTION BLUR", quality.motionBlur);
            qualityToggle(state, engine, column, "opt.dof", "DEPTH OF FIELD", quality.depthOfField);
            qualityToggle(state, engine, column, "opt.lens", "LENS EFFECTS", quality.lensEffects);
            qualityToggle(state, engine, column, "opt.mirrors", "MIRRORS", quality.planarReflections);
            qualityToggle(state, engine, column, "opt.parallax", "PARALLAX SURFACES", quality.parallax);

            std::vector<std::string> sizes;

            for (const auto& [width, height] : k_windowSizes)
            {
                sizes.push_back(std::to_string(width) + " X " + std::to_string(height));
            }

            if (context.cycler("opt.window", column.next(k_menuRowHeight), "WINDOW SIZE (RESTART)", shell.options.resolution,
                    sizes))
            {
                const auto [width, height] = k_windowSizes[shell.options.resolution];
                shell.settings.windowWidth = width;
                shell.settings.windowHeight = height;

                if (auto saved = render::saveWindowSize(shell.settingsFile, width, height); !saved)
                {
                    notify(shell.options, "NOT SAVED: " + saved.error().message);
                }
                else
                {
                    notify(shell.options, "WINDOW SIZE APPLIES AFTER RESTART");
                }
            }

            const std::vector<std::string> presentModes{"VSYNC", "LOW LATENCY", "UNCAPPED"};
            std::size_t present = static_cast<std::size_t>(shell.settings.presentMode);

            if (context.cycler("opt.present", column.next(k_menuRowHeight), "PRESENT MODE", present, presentModes))
            {
                shell.settings.presentMode = static_cast<render::PresentMode>(present);

                if (auto changed = engine.renderer.setPresentMode(shell.settings.presentMode); !changed)
                {
                    notify(shell.options, "PRESENT MODE: " + changed.error().message);
                }

                saveUser(state, engine);
            }

            if (context.toggle("opt.fullscreen", column.next(k_menuRowHeight), "FULLSCREEN", shell.settings.fullscreen))
            {
                saveUser(state, engine);
            }

            char fov[16]{};
            std::snprintf(fov, sizeof(fov), "%.0f DEG", static_cast<double>(shell.settings.fieldOfView));

            if (context.slider("opt.fov", column.next(k_menuRowHeight), "FIELD OF VIEW (VERTICAL)", shell.settings.fieldOfView,
                    k_minFieldOfView, k_maxFieldOfView, 1.0f, fov))
            {
                saveUser(state, engine);
            }
        }

        void drawAudio(GameState& state, EngineContext& engine, ui::Column& column)
        {
            UserSettings& settings = state.shell.settings;
            ui::UiContext& context = state.shell.ui;
            const std::array<std::tuple<std::string_view, std::string_view, float*>, 5> volumes{{
                {"opt.master", "MASTER VOLUME", &settings.masterVolume},
                {"opt.world", "WORLD", &settings.worldVolume},
                {"opt.player", "PLAYER (WEAPONS, STEPS)", &settings.playerVolume},
                {"opt.ambience", "AMBIENCE", &settings.ambienceVolume},
                {"opt.music", "MUSIC", &settings.musicVolume}}};

            for (const auto& [id, label, value] : volumes)
            {
                if (context.slider(id, column.next(k_menuRowHeight), label, *value, 0.0f, 1.0f, 0.05f, percent(*value)))
                {
                    saveUser(state, engine);
                }
            }
        }

        void drawControls(GameState& state, EngineContext& engine, ui::Column& column)
        {
            Shell& shell = state.shell;
            UserSettings& settings = shell.settings;
            ui::UiContext& context = shell.ui;
            char sensitivity[16]{};
            std::snprintf(sensitivity, sizeof(sensitivity), "%.2f", static_cast<double>(settings.mouseSensitivity * 1000.0f));

            if (context.slider("opt.sensitivity", column.next(k_menuRowHeight), "MOUSE SENSITIVITY",
                    settings.mouseSensitivity, 0.0005f, 0.008f, 0.00025f, sensitivity))
            {
                saveUser(state, engine);
            }

            if (context.toggle("opt.invert", column.next(k_menuRowHeight), "INVERT MOUSE Y", settings.invertY))
            {
                saveUser(state, engine);
            }

            for (std::size_t index = 0; index < k_actionCount; ++index)
            {
                const auto action = static_cast<Action>(index);
                ActionBinding& binding = settings.bindings.actions[index];
                std::string current = bindingName(binding.primary);

                if (binding.secondary.key || binding.secondary.button)
                {
                    current += "  /  " + bindingName(binding.secondary);
                }

                const std::string id = "opt.bind." + std::string(actionSettingsKey(action));

                if (const std::optional<ui::CapturedInput> captured = context.keyCapture(id, column.next(k_menuRowHeight),
                        actionLabel(action), current))
                {
                    binding.primary = Binding{.key = captured->key, .button = captured->button};
                    binding.secondary = Binding{};
                    notify(shell.options, std::string(actionLabel(action)) + " = " + bindingName(binding.primary));
                    saveUser(state, engine);
                }
            }

            if (context.button("opt.resetbind", column.next(k_menuRowHeight), "RESET CONTROLS TO DEFAULTS"))
            {
                settings.bindings = defaultKeyBindings();
                notify(shell.options, "CONTROLS RESET");
                saveUser(state, engine);
            }
        }

        void drawAccessibility(GameState& state, EngineContext& engine, ui::Column& column)
        {
            UserSettings& settings = state.shell.settings;
            ui::UiContext& context = state.shell.ui;
            bool changed = context.toggle("opt.subtitles", column.next(k_menuRowHeight), "SUBTITLES", settings.subtitles);
            changed = context.toggle("opt.bob", column.next(k_menuRowHeight), "REDUCE HEAD BOB", settings.reduceHeadBob)
                || changed;
            changed = context.toggle("opt.reduceblur", column.next(k_menuRowHeight), "REDUCE MOTION BLUR (OFF)",
                settings.reduceMotionBlur) || changed;
            changed = context.toggle("opt.intro", column.next(k_menuRowHeight), "SKIP INTRO", settings.skipIntro) || changed;

            if (changed)
            {
                saveUser(state, engine);
            }
        }

        float tabContentHeight(OptionsTab tab)
        {
            const auto rows = [](std::size_t count)
            {
                return static_cast<float>(count) * (k_menuRowHeight + k_menuRowGap);
            };

            switch (tab)
            {
                case OptionsTab::Graphics:
                {
                    return rows(17);
                }
                case OptionsTab::Audio:
                {
                    return rows(5);
                }
                case OptionsTab::Controls:
                {
                    return rows(k_actionCount + 3);
                }
                case OptionsTab::Accessibility:
                {
                    return rows(4);
                }
            }

            return rows(1);
        }
    } //namespace

    void openOptions(Shell& shell, EngineContext& engine, ShellScreen returnTo)
    {
        shell.options.quality = engine.renderer.graphicsQuality();
        shell.options.returnTo = returnTo;
        shell.options.notice.clear();
        shell.options.resolution = 1;

        for (std::size_t index = 0; index < k_windowSizes.size(); ++index)
        {
            if (k_windowSizes[index].first == shell.settings.windowWidth
                && k_windowSizes[index].second == shell.settings.windowHeight)
            {
                shell.options.resolution = index;
            }
        }

        goToScreen(shell, ShellScreen::Options);
    }

    void drawOptionsScreen(GameState& state, EngineContext& engine, float frameSeconds)
    {
        Shell& shell = state.shell;
        OptionsState& options = shell.options;
        ui::UiContext& context = shell.ui;
        const ScreenLayout layout = screenLayout(context, k_columnWidth);
        drawMenuTitle(context, {layout.title.x, layout.title.y - 40.0f}, "OPTIONS", "CHANGES APPLY AND SAVE AT ONCE");

        const float tabWidth = k_columnWidth / static_cast<float>(k_tabNames.size());
        const float tabTop = layout.column.min.y - 60.0f;

        for (std::size_t index = 0; index < k_tabNames.size(); ++index)
        {
            const ui::Rect tab = ui::rectAt({layout.column.min.x + tabWidth * static_cast<float>(index), tabTop},
                {tabWidth - 6.0f, k_tabHeight});
            const bool isOpen = static_cast<std::size_t>(options.tab) == index;

            if (context.button("opt.tab." + std::to_string(index), tab, k_tabNames[index]))
            {
                options.tab = static_cast<OptionsTab>(index);
            }

            if (isOpen)
            {
                context.fillRect(ui::rectAt({tab.min.x, tab.max.y - 3.0f}, {tab.width(), 3.0f}), k_menuAccent);
            }
        }

        const math::Vec2 canvas = context.canvasSize();
        const ui::Rect area{.min = {layout.column.min.x, tabTop + k_tabHeight + 14.0f},
            .max = {layout.column.max.x, canvas.y - 170.0f}};
        drawMenuPanel(context, ui::Rect{.min = {area.min.x - 10.0f, area.min.y - 6.0f}, .max = {area.max.x + 10.0f, area.max.y + 6.0f}});
        const float scroll = context.beginScroll("opt.scroll." + std::to_string(static_cast<int>(options.tab)), area,
            tabContentHeight(options.tab));
        ui::Column column(ui::Rect{.min = {area.min.x, area.min.y - scroll}, .max = {area.max.x - 12.0f, area.max.y + 4000.0f}},
            k_menuRowGap);

        switch (options.tab)
        {
            case OptionsTab::Graphics:
            {
                drawGraphics(state, engine, column);
                break;
            }
            case OptionsTab::Audio:
            {
                drawAudio(state, engine, column);
                break;
            }
            case OptionsTab::Controls:
            {
                drawControls(state, engine, column);
                break;
            }
            case OptionsTab::Accessibility:
            {
                drawAccessibility(state, engine, column);
                break;
            }
        }

        context.endScroll();
        ui::Column footer(ui::Rect{.min = {layout.column.min.x, canvas.y - 150.0f},
            .max = {layout.column.min.x + 300.0f, canvas.y - 90.0f}}, 0.0f);

        if (context.button("opt.back", footer.next(k_menuRowHeight), "BACK"))
        {
            goToScreen(shell, options.returnTo);
        }

        options.noticeSeconds -= frameSeconds;

        if (options.noticeSeconds > 0.0f)
        {
            context.text(options.notice, {layout.column.min.x + 330.0f, canvas.y - 136.0f}, 21.0f, k_menuAccent);
        }
    }
} //namespace game
