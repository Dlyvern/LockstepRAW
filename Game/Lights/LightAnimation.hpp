#ifndef LOCKSTEP_GAME_LIGHTS_LIGHT_ANIMATION_HPP
#define LOCKSTEP_GAME_LIGHTS_LIGHT_ANIMATION_HPP

#include "Lights/LightBuzz.hpp"
#include "Lights/LightPatterns.hpp"
#include "Presentation/LevelLights.hpp"

#include <Lockstep/Core/Math.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lockstep::app
{
    struct EngineContext;
} //namespace lockstep::app

namespace lockstep::assets
{
    struct Scene;
} //namespace lockstep::assets

namespace game
{
    // One authored light that animates.
    // index:   its index in Scene::lights.
    // pattern: what it does.
    // seed:    its seed; siren lights of one car (within a few metres of each other) share one.
    // level:   its brightness this frame, 0-1.
    struct AnimatedLight
    {
        uint32_t index{0};
        LightPattern pattern{LightPattern::None};
        uint32_t seed{0};
        float level{1.0f};
    };

    // An emissive level mesh (a lamp) that dims with an animated light: the nearest point or spot
    // light within k_lampReach of its bounds is animated.
    // instance: index into Scene::instances.
    // light:    index into LightAnimation::lights.
    struct LampLink
    {
        uint32_t instance{0};
        uint32_t light{0};
    };

    // How far (metres) an emissive mesh's bounds may be from a light for the light to drive it.
    inline constexpr float k_lampReach{1.0f};

    // Dev settings of the light animation.
    // isEnabled:   animate (off: every light steady, no buzz).
    // dimLamps:    scale the emissive light of lamp meshes with their animated light.
    // testPattern: dev test: give every untagged point and spot light this pattern (None: off).
    //              Sirens alternate red and blue by light.
    // patterns:    pattern timing.
    struct LightAnimationSettings
    {
        bool isEnabled{true};
        bool dimLamps{true};
        LightPattern testPattern{LightPattern::None};
        LightPatternTuning patterns{};
    };

    // Animated level lights (flicker, siren, pulse) and the fluorescent buzz. Presentation only:
    // never replicated, so the two machines may differ. Runs on World time, so bullet-time slows it.
    // settings:    dev settings.
    // buzz:        the fluorescent sounds.
    // scene, sceneLightCount, builtTestPattern: what `lights` was built from.
    // seconds:     World seconds since the lights started animating.
    // lights:      the level's animated lights.
    // lamps:       emissive meshes driven by them.
    // overrides:   per scene light, this frame's change, handed to submitLevelLights().
    struct LightAnimation
    {
        LightAnimationSettings settings{};
        LightBuzz buzz{};
        const lockstep::assets::Scene* scene{nullptr};
        std::size_t sceneLightCount{0};
        LightPattern builtTestPattern{LightPattern::None};
        double seconds{0.0};
        std::vector<AnimatedLight> lights{};
        std::vector<LampLink> lamps{};
        std::vector<LightOverride> overrides{};
    };

    // Animates the level's tagged lights, submits the level lights (see submitLevelLights()), sets
    // the emissive scale of their lamp meshes (SceneInstance::emissiveScale, drawn by the next
    // submitScene()) and keeps the fluorescent buzz going. Call once per frame; with no level it
    // only stops the buzz.
    // animation:    the light animation state.
    // engine:       Engine services (renderer, audio, content for the first load of the sounds).
    // scene:        the current level, or null.
    // eye:          the camera position (light selection and the buzz listener).
    // worldSeconds: World-domain seconds since the previous frame.
    void presentLevelLights(LightAnimation& animation, lockstep::app::EngineContext& engine,
        lockstep::assets::Scene* scene, const math::Vec3& eye, float worldSeconds);

#if LOCKSTEP_DEV_TOOLS
    // Draws the "Light animation" section of the Game dev panel.
    // animation: the light animation state.
    void lightAnimationDevSection(LightAnimation& animation);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_LIGHTS_LIGHT_ANIMATION_HPP
