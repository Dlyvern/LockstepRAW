#include "Presentation/Lighting.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Render/Lighting.hpp>
#include <Lockstep/Render/RenderSettings.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <array>
#include <iostream>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;
        namespace math = lockstep::math;

        const std::array<render::PointLight, 2> k_lamps{{
            {.position = {-6.0f, 2.2f, -13.5f}, .color = {1.0f, 0.72f, 0.45f}, .intensity = 6.0f, .range = 8.0f},
            {.position = {6.0f, 2.2f, -13.5f}, .color = {1.0f, 0.72f, 0.45f}, .intensity = 6.0f, .range = 8.0f}}};
    } //namespace

    void applyArenaLook(render::Renderer& renderer)
    {
        render::RenderSettings& settings = renderer.settings();

        settings.sky.enabled = true;
        settings.sky.sunAzimuth = math::radians(145.0f);
        settings.sky.sunElevation = math::radians(38.0f);
        settings.lighting.ambientIntensity = 0.0f;
        settings.fog.enabled = false;
        settings.tonemap = {.enabled = true, .tonemapper = render::Tonemapper::Aces, .exposure = 0.85f, .dither = true};
    }

    void updateLighting(LightingState& state, const lockstep::platform::Input& input, float frameSeconds)
    {
        static_cast<void>(input);
        state.seconds += frameSeconds;
    }

    void submitArenaLights(render::Renderer& renderer, LightingState& state)
    {
        for (const render::PointLight& lamp : k_lamps)
        {
            renderer.submitLight(lamp);
        }

        if (!state.showcase && !state.showcaseFailed)
        {
            auto showcase = createMaterialShowcase(renderer);

            if (!showcase)
            {
                std::clog << "Game: material showcase: " << showcase.error().message << '\n';
                state.showcaseFailed = true;
                return;
            }

            state.showcase = std::move(showcase).value();
        }

        if (state.showcase)
        {
            drawMaterialShowcase(renderer, *state.showcase, state.seconds);
        }
    }
} //namespace game
