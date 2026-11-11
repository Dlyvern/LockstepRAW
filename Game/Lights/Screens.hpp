#ifndef LOCKSTEP_GAME_LIGHTS_SCREENS_HPP
#define LOCKSTEP_GAME_LIGHTS_SCREENS_HPP

#include <Lockstep/Core/Math.hpp>

#include <cstdint>
#include <vector>

namespace lockstep::assets
{
    struct Scene;
} //namespace lockstep::assets

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    namespace math = lockstep::math;

    // How a screen behaves (name tags of its Screen_* marker).
    // Steady:  on, with a faint shimmer.
    // Flicker: a failing backlight: short dropouts and stutters, now and then dark for a moment
    //          (`_Flicker`).
    // Off:     dark: no glow, no light (`_Off`).
    enum class ScreenBehaviour : uint8_t
    {
        Steady = 0,
        Flicker,
        Off
    };

    // One glowing screen of the level, found from a marker node named "Screen_*" placed at the
    // middle of the display surface, facing the node's Blender -Y (Blender's Front view looks at the
    // screen). Its emissive meshes are those within k_screenReach of the marker (or under it) no
    // larger than 2.5 m (a lit facade around it is not part of it).
    // position:  the marker position (the screen's centre).
    // facing:    unit direction the screen faces.
    // color:     light colour (`_Blue`, `_Green`, `_Amber`, `_Red`, `_White`; cool white by default).
    // area:      lit area in square metres (from the emissive meshes, else the marker's X and Z scale).
    // behaviour: what it does.
    // scrolls:   its content scrolls (`_Scroll`).
    // isImage:   keep the authored emissive image (`_Image`); otherwise a display shows the
    //            Engine's procedural terminal image tinted by it.
    // isPanel:   a lit panel rather than a display (`_Panel`: a vending machine, a sign): it glows,
    //            flickers and lights, without scanlines or a rolling band.
    // instances: indices into Scene::instances of its emissive meshes.
    // seed:      animation seed.
    // level:     this frame's brightness, 0-1.
    struct LevelScreen
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Vec3 facing{0.0f, 0.0f, 1.0f};
        math::Vec3 color{0.62f, 0.75f, 1.0f};
        float area{0.1f};
        ScreenBehaviour behaviour{ScreenBehaviour::Steady};
        bool scrolls{false};
        bool isPanel{false};
        bool isImage{false};
        std::vector<uint32_t> instances{};
        uint32_t seed{0};
        float level{1.0f};
    };

    // How far (metres) an emissive mesh's bounds may be from a Screen_* marker to belong to it.
    inline constexpr float k_screenReach{0.35f};

    // Dev and quality settings of the screens.
    // enabled:        find and animate screens (off: emissive meshes stay as authored).
    // lights:         turn the nearest screens into lights.
    // maxLights:      most screen lights per frame (Game sets it from GraphicsQuality::screenLights).
    // brightness:     factor on the displays' authored emission (panels keep theirs).
    // lightIntensity: light per square metre of screen.
    // lightRange:     how far a screen's light reaches, metres.
    // reach:          screens farther than this from the camera give no light, metres.
    // effects:        strength of the screen shader effects (scanlines, rolling band), 0 to 2.
    // scrollSpeed:    how fast `_Scroll` screens scroll, UV units per second.
    struct ScreenTuning
    {
        bool enabled{true};
        bool lights{true};
        uint32_t maxLights{4};
        float brightness{0.22f};
        float lightIntensity{0.8f};
        float lightRange{3.5f};
        float reach{14.0f};
        float effects{1.0f};
        float scrollSpeed{0.04f};
    };

    // The level's glowing screens: they flicker per their tags and light their surroundings
    // through a cheap unshadowed spot light each (the nearest ones, within a budget that shares the
    // renderer's k_maxLocalLights with the level's lights). Presentation only, World time.
    // tuning:  settings.
    // screens: the screens of `scene`.
    // scene:   the scene they were found in.
    // seconds: World seconds animated.
    // lit:     screen lights submitted this frame.
    // appliedEffects, appliedScroll: the shader strength and scroll speed the screen materials were
    //          last given (-1: not yet).
    struct LevelScreens
    {
        ScreenTuning tuning{};
        std::vector<LevelScreen> screens{};
        const lockstep::assets::Scene* scene{nullptr};
        double seconds{0.0};
        uint32_t lit{0};
        float appliedEffects{-1.0f};
        float appliedScroll{-1.0f};
    };

    // Finds the screens when the scene changes, animates them (emissive scale of their meshes),
    // advances the screen shader clock (RenderSettings::surfaceEffects.time) and submits the
    // lights of the nearest ones. Call before the level lights are submitted, so the screen lights
    // keep their slots.
    // screens:      the screens.
    // renderer:     the renderer.
    // scene:        the current level, or null.
    // eye:          the camera position.
    // worldSeconds: World-domain seconds since the previous frame.
    // Returns:      how many lights were submitted.
    uint32_t presentScreens(LevelScreens& screens, lockstep::render::Renderer& renderer,
        lockstep::assets::Scene* scene, const math::Vec3& eye, float worldSeconds);

#if LOCKSTEP_DEV_TOOLS
    // Draws the "Screens" section of the Game dev panel.
    // screens: the screens.
    void screensDevSection(LevelScreens& screens);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_LIGHTS_SCREENS_HPP
