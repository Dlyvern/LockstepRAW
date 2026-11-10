#ifndef LOCKSTEP_GAME_INTRO_INTRO_CARDS_HPP
#define LOCKSTEP_GAME_INTRO_INTRO_CARDS_HPP

#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <cstdint>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace lockstep::assets
{
    class FileSystem;
} //namespace lockstep::assets

namespace lockstep::ui
{
    class UiContext;
} //namespace lockstep::ui

namespace game
{
    // When the body-cam title card cuts to black, in seconds from its start.
    inline constexpr float k_titleCutSeconds{3.0f};

    // One logo image: its texture and its pixel size (for the aspect ratio).
    struct IntroLogo
    {
        lockstep::render::TextureHandle texture{};
        uint32_t width{0};
        uint32_t height{0};
    };

    // Textures the intro's 2D cards draw with.
    // noise: grey static for the body-cam burst.
    // dot:   a round dot (the REC light).
    // fmod:  the official FMOD logo, prepared for a dark card (Data/intro/fmod_logo.rgba).
    // physx: the official NVIDIA PhysX logo, prepared likewise (Data/intro/physx_logo.rgba).
    //        A logo that failed to load has no texture; the card then shows its name as text.
    struct IntroCardTextures
    {
        lockstep::render::TextureHandle noise{};
        lockstep::render::TextureHandle dot{};
        IntroLogo fmod{};
        IntroLogo physx{};
    };

    // Creates the cards' textures and loads the logos (raw straight-alpha RGBA files written by
    // Content/source/scripts/make_intro_logos.py: "RGBA", little-endian u32 width and height, pixels).
    // renderer: the renderer.
    // files:    the file system the logos load from; a missing or malformed logo is logged and left out.
    // Returns:  the textures, or the renderer's error.
    [[nodiscard]] lockstep::Result<IntroCardTextures> createIntroCardTextures(lockstep::render::Renderer& renderer,
        const lockstep::assets::FileSystem& files);

    // Releases the cards' textures.
    // textures: the textures.
    // renderer: the renderer that created them.
    void destroyIntroCardTextures(IntroCardTextures& textures, lockstep::render::Renderer& renderer);

    // Draws a full-screen black backdrop (the 2D cards show nothing of the 3D scene).
    // ui: the UI frame.
    void drawIntroBackdrop(lockstep::ui::UiContext& ui);

    // Draws the content warning card's text.
    // ui: the UI frame, its opacity set to the card's fade.
    void drawWarningCard(lockstep::ui::UiContext& ui);

    // Draws the "made with" caption above the dust wordmark of the Vela card.
    // ui: the UI frame, its opacity set to the caption's fade.
    void drawVelaCaption(lockstep::ui::UiContext& ui);

    // Draws the FMOD and PhysX card: both logos side by side and the FMOD attribution line.
    // ui:       the UI frame, its opacity set to the card's fade.
    // textures: the logos.
    void drawCreditsCard(lockstep::ui::UiContext& ui, const IntroCardTextures& textures);

    // Draws the body-cam "Lockstep" title card: a static burst, the word flickering in, the REC
    // light and camera readouts, then black after k_titleCutSeconds.
    // ui:       the UI frame.
    // textures: the static and dot textures.
    // seconds:  time since the card started.
    // frame:    a counter that changes every presented frame (drives the static and flicker).
    void drawTitleCard(lockstep::ui::UiContext& ui, const IntroCardTextures& textures, float seconds, uint64_t frame);
} //namespace game

#endif //LOCKSTEP_GAME_INTRO_INTRO_CARDS_HPP
