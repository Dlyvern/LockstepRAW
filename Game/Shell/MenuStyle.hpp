#ifndef LOCKSTEP_GAME_SHELL_MENU_STYLE_HPP
#define LOCKSTEP_GAME_SHELL_MENU_STYLE_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/Resources.hpp>
#include <Lockstep/UI/UiContext.hpp>

#include <cstdint>
#include <string_view>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    namespace math = lockstep::math;

    // The menu's body-cam / police-terminal look: off-white monospaced text, one red accent, a
    // viewfinder frame, scanlines, film grain and a vignette.
    // grain:    a tiling grey noise texture.
    // vignette: a radial edge-darkening texture stretched over the screen.
    // dot:      a round dot (the REC light).
    // seconds:  real seconds the menu has been shown (blink, grain, the REC counter).
    // random:   the grain's per-frame random state.
    struct MenuStyle
    {
        lockstep::render::TextureHandle grain{};
        lockstep::render::TextureHandle vignette{};
        lockstep::render::TextureHandle dot{};
        float seconds{0.0f};
        uint32_t random{0x2545F491u};
    };

    // Menu colours (sRGB).
    inline constexpr math::Vec4 k_menuText{0.86f, 0.85f, 0.80f, 1.0f};
    inline constexpr math::Vec4 k_menuDim{0.55f, 0.55f, 0.52f, 1.0f};
    inline constexpr math::Vec4 k_menuAccent{0.80f, 0.12f, 0.10f, 1.0f};
    inline constexpr math::Vec4 k_menuPanel{0.02f, 0.02f, 0.025f, 0.62f};
    inline constexpr math::Vec4 k_menuLine{0.86f, 0.85f, 0.80f, 0.30f};

    // Creates the style's textures; a failure is logged and leaves that texture out.
    // renderer: the renderer.
    // Returns:  the style.
    [[nodiscard]] MenuStyle createMenuStyle(lockstep::render::Renderer& renderer);

    // Sets the UI theme to the menu look (monospaced, off-white, red focus marker).
    // ui: the UI context, between begin() and end().
    void applyMenuTheme(lockstep::ui::UiContext& ui);

    // Draws the dark backdrop used when no 3D background is loaded: a vertical gradient from a
    // cold dark grey to black.
    // ui: the UI context.
    void drawMenuGradient(lockstep::ui::UiContext& ui);

    // Draws the vignette and the viewfinder frame: corner brackets, the REC light and counter, a
    // camera label and a battery readout.
    // style:        the style (its clock advances by frameSeconds).
    // ui:           the UI context.
    // frameSeconds: real seconds since the last frame.
    // label:        the camera label shown top-left.
    void drawMenuFrame(MenuStyle& style, lockstep::ui::UiContext& ui, float frameSeconds, std::string_view label);

    // Draws scanlines and film grain over everything drawn before. Call last.
    // style:    the style.
    // ui:       the UI context.
    // renderer: the renderer (for the pixel size of the scanlines).
    void drawMenuNoise(MenuStyle& style, lockstep::ui::UiContext& ui, const lockstep::render::Renderer& renderer);

    // Draws a screen title: a big spaced heading with a thin rule and a dim subtitle under it.
    // ui:       the UI context.
    // position: top-left of the heading.
    // title:    the heading.
    // subtitle: the dim line under the rule, may be empty.
    void drawMenuTitle(lockstep::ui::UiContext& ui, math::Vec2 position, std::string_view title,
        std::string_view subtitle);

    // Draws a menu panel: a dark translucent box with a thin outline.
    // ui:   the UI context.
    // rect: the panel.
    void drawMenuPanel(lockstep::ui::UiContext& ui, const lockstep::ui::Rect& rect);
} //namespace game

#endif //LOCKSTEP_GAME_SHELL_MENU_STYLE_HPP
