#include "Presentation/Lighting.hpp"

#include "Player/Pawns.hpp"

#include <Lockstep/Render/Lighting.hpp>
#include <Lockstep/Render/RenderSettings.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <array>
#include <cmath>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;

        enum class Fixture
        {
            Steady,
            Flicker,
            Pulse
        };

        struct ArenaFixture
        {
            render::PointLight light;
            Fixture behaviour;
        };

        constexpr math::Vec3 k_worldUp{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_pawnForward{0.0f, 0.0f, -1.0f};
        constexpr math::Vec3 k_flashlightColour{1.0f, 0.93f, 0.82f};
        constexpr float k_flashlightIntensity{60.0f};
        constexpr float k_flashlightRange{30.0f};
        constexpr float k_flashlightInner{math::radians(9.0f)};
        constexpr float k_flashlightOuter{math::radians(24.0f)};
        constexpr float k_flashlightRight{0.22f};
        constexpr float k_flashlightDown{0.18f};
        constexpr float k_remoteFlashlightAhead{0.45f};
        constexpr float k_remoteFlashlightPitch{-0.12f};

        constexpr float k_flickerRate{23.0f};
        constexpr float k_flickerDropout{0.86f};
        constexpr float k_flickerLow{0.08f};
        constexpr float k_pulseRate{1.6f};
        constexpr float k_pulseFloor{0.15f};

        const std::array<ArenaFixture, 5> k_fixtures{{
            {{.position = {0.0f, 2.7f, 1.5f}, .color = {1.0f, 0.62f, 0.32f}, .intensity = 7.0f, .range = 9.0f},
                Fixture::Steady},
            {{.position = {-12.0f, 2.8f, -10.0f}, .color = {0.72f, 0.9f, 1.0f}, .intensity = 6.0f, .range = 10.0f},
                Fixture::Flicker},
            {{.position = {14.0f, 2.6f, 12.0f}, .color = {1.0f, 0.08f, 0.04f}, .intensity = 6.0f, .range = 9.0f},
                Fixture::Pulse},
            {{.position = {10.0f, 2.8f, -13.0f}, .color = {0.9f, 0.85f, 0.7f}, .intensity = 3.5f, .range = 8.0f},
                Fixture::Steady},
            {{.position = {-13.0f, 2.8f, 12.0f}, .color = {0.6f, 1.0f, 0.75f}, .intensity = 3.0f, .range = 8.0f},
                Fixture::Flicker}}};

        float hash(float value)
        {
            const float scaled = std::sin(value * 12.9898f) * 43758.5453f;
            return scaled - std::floor(scaled);
        }

        float fixtureLevel(Fixture behaviour, float seconds, float seed)
        {
            switch (behaviour)
            {
                case Fixture::Flicker:
                {
                    const float step = std::floor(seconds * k_flickerRate);
                    return hash(step + seed) > k_flickerDropout ? k_flickerLow : 1.0f;
                }
                case Fixture::Pulse:
                {
                    const float wave = 0.5f + 0.5f * std::sin(seconds * k_pulseRate * 2.0f * math::k_pi);
                    return k_pulseFloor + (1.0f - k_pulseFloor) * wave * wave;
                }
                case Fixture::Steady:
                {
                    break;
                }
            }

            return 1.0f;
        }

        render::SpotLight flashlight(const math::Vec3& position, const math::Vec3& direction, bool castsShadows)
        {
            return render::SpotLight{.position = position,
                .direction = direction,
                .color = k_flashlightColour,
                .intensity = k_flashlightIntensity,
                .range = k_flashlightRange,
                .innerAngle = k_flashlightInner,
                .outerAngle = k_flashlightOuter,
                .castsShadows = castsShadows};
        }
    } //namespace

    void applyArenaLook(render::Renderer& renderer)
    {
        render::RenderSettings& settings = renderer.settings();

        settings.lighting.ambientColor = {0.55f, 0.62f, 0.8f};
        settings.lighting.ambientIntensity = 0.045f;
        settings.fog = {.enabled = true, .color = {0.006f, 0.007f, 0.01f}, .density = 0.07f, .start = 2.0f,
            .maxAmount = 0.97f};
        settings.tonemap = {.enabled = true, .tonemapper = render::Tonemapper::Aces, .exposure = 1.3f, .dither = true};
    }

    void updateLighting(LightingState& state, const lockstep::platform::Input& input, float frameSeconds)
    {
        state.seconds += frameSeconds;

        if (input.wasKeyPressed(lockstep::platform::Key::F))
        {
            state.isFlashlightOn = !state.isFlashlightOn;
        }
    }

    void submitArenaLights(render::Renderer& renderer, const LightingState& state)
    {
        renderer.submitLight(render::DirectionalLight{.direction = math::normalize(math::Vec3{-0.3f, -1.0f, -0.2f}),
            .color = {0.45f, 0.55f, 0.85f},
            .intensity = 0.05f});

        float seed = 0.0f;

        for (const ArenaFixture& fixture : k_fixtures)
        {
            render::PointLight light = fixture.light;
            light.intensity *= fixtureLevel(fixture.behaviour, state.seconds, seed);
            renderer.submitLight(light);
            seed += 1.0f;
        }
    }

    void submitLocalFlashlight(render::Renderer& renderer, const LightingState& state, const math::Vec3& eye,
        const math::Vec3& direction)
    {
        if (!state.isFlashlightOn)
        {
            return;
        }

        const math::Vec3 right = math::normalize(math::cross(direction, k_worldUp));
        const math::Vec3 up = math::cross(right, direction);
        const math::Vec3 position = eye + right * k_flashlightRight - up * k_flashlightDown;

        renderer.submitLight(flashlight(position, direction, true));
    }

    void submitRemoteFlashlight(render::Renderer& renderer, const math::Transform& pawn)
    {
        const math::Vec3 facing = pawn.rotation * k_pawnForward;
        const math::Vec3 direction = math::normalize(facing + k_worldUp * k_remoteFlashlightPitch);
        const math::Vec3 position = eyePosition(pawn) + facing * k_remoteFlashlightAhead;

        renderer.submitLight(flashlight(position, direction, false));
    }
} //namespace game
