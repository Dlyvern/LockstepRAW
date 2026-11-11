#include "Effects/Drips.hpp"

#include "Effects/EffectTextures.hpp"
#include "GameState.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Audio/AudioSystem.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/Render/SurfaceEffects.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string_view>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;
        namespace audio = lockstep::audio;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr std::string_view k_dripPrefix{"Fx_Drip_"};
        constexpr float k_gravity{9.81f};
        constexpr float k_maxFall{12.0f};
        constexpr float k_floorLift{0.005f};
        constexpr float k_dropDrawSeconds{1.6f};
        constexpr uint32_t k_soundVariants{4};
        constexpr uint32_t k_sampleRate{44100};
        constexpr float k_soundSeconds{0.12f};
        constexpr float k_firstDropSpread{3.0f};

        float randomUnit(uint32_t& state)
        {
            state ^= state << 13u;
            state ^= state >> 17u;
            state ^= state << 5u;

            return static_cast<float>(state & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
        }

        float nextInterval(Drips& drips)
        {
            const DripTuning& tuning = drips.tuning;
            const float jitter = std::clamp(tuning.jitter, 0.0f, 1.0f);

            return std::max(tuning.interval * (1.0f + jitter * (randomUnit(drips.random) * 2.0f - 1.0f)), 0.1f);
        }

        render::ParticleEffectDescription dropEffect(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect{};
            effect.texture = atlas;
            effect.atlasColumns = k_effectAtlasColumns;
            effect.atlasRows = k_effectAtlasRows;
            effect.firstFrame = k_dotFrame;
            effect.blend = render::ParticleBlend::Additive;
            effect.shape = render::ParticleShape::Stretched;
            effect.burstCount = 1;
            effect.lifetime = {k_dropDrawSeconds, k_dropDrawSeconds};
            effect.acceleration = {0.0f, -k_gravity, 0.0f};
            effect.size = {0.006f, 0.006f, 0.006f, 0.5f};
            effect.color = {{0.7f, 0.75f, 0.8f, 0.0f}, {0.7f, 0.75f, 0.8f, 0.8f}, {0.7f, 0.75f, 0.8f, 0.8f}, 0.05f};
            effect.stretch = 0.025f;
            effect.softness = 0.0f;
            effect.scattering = 0.7f;

            return effect;
        }

        render::ParticleEffectDescription splashEffect(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect{};
            effect.texture = atlas;
            effect.atlasColumns = k_effectAtlasColumns;
            effect.atlasRows = k_effectAtlasRows;
            effect.firstFrame = k_dotFrame;
            effect.blend = render::ParticleBlend::Additive;
            effect.burstCount = 7;
            effect.lifetime = {0.15f, 0.35f};
            effect.speed = {0.4f, 1.1f};
            effect.coneAngle = 0.6f;
            effect.acceleration = {0.0f, -k_gravity, 0.0f};
            effect.size = {0.008f, 0.006f, 0.004f, 0.5f};
            effect.sizeScale = {0.6f, 1.4f};
            effect.color = {{0.6f, 0.65f, 0.7f, 0.8f}, {0.6f, 0.65f, 0.7f, 0.6f}, {0.6f, 0.65f, 0.7f, 0.0f}, 0.5f};
            effect.softness = 0.005f;
            effect.scattering = 0.6f;

            return effect;
        }

        std::vector<float> plinkSamples(uint32_t variant)
        {
            const auto count = static_cast<std::size_t>(k_soundSeconds * static_cast<float>(k_sampleRate));
            std::vector<float> samples(count);
            const float startPitch = 700.0f + 140.0f * static_cast<float>(variant);
            uint32_t noise = 0x9E3779B9u + variant * 7919u;
            float phase = 0.0f;

            for (std::size_t index = 0; index < count; ++index)
            {
                const float time = static_cast<float>(index) / static_cast<float>(k_sampleRate);
                const float pitch = startPitch * (1.0f + 1.6f * (1.0f - std::exp(-time * 60.0f)));
                phase += 6.2831853f * pitch / static_cast<float>(k_sampleRate);
                const float tone = std::sin(phase) * std::exp(-time * 38.0f) * std::min(time * 2000.0f, 1.0f);
                const float tick = (randomUnit(noise) * 2.0f - 1.0f) * std::exp(-time * 900.0f);
                samples[index] = 0.55f * tone + 0.25f * tick;
            }

            return samples;
        }

        void tryCreate(Drips& drips, GameState& state, EngineContext& engine)
        {
            drips.isTried = true;

            if (state.effects.particles && state.effects.atlas.isValid())
            {
                render::ParticleSystem& particles = *state.effects.particles;
                auto drop = particles.createEffect(dropEffect(state.effects.atlas));
                auto splash = particles.createEffect(splashEffect(state.effects.atlas));

                if (drop && splash)
                {
                    drips.drop = drop.value();
                    drips.splash = splash.value();
                }
                else
                {
                    std::clog << "Game: drip particle effects rejected\n";
                }
            }

            for (uint32_t variant = 0; variant < k_soundVariants; ++variant)
            {
                const std::vector<float> samples = plinkSamples(variant);
                auto sound = engine.audio.createSoundFromSamples(samples, k_sampleRate, 1,
                    audio::SoundDescription{.is3d = true, .minDistance = 1.0f, .maxDistance = 14.0f});

                if (sound)
                {
                    drips.sounds.push_back(sound.value());
                }
            }
        }

        void findSites(Drips& drips, const lockstep::assets::Scene* scene, const lockstep::physics::PhysicsWorld& physics)
        {
            drips.sites.clear();
            drips.scene = scene;

            if (scene == nullptr)
            {
                return;
            }

            for (const lockstep::assets::SceneNode& node : scene->nodes)
            {
                if (!std::string_view(node.name).starts_with(k_dripPrefix))
                {
                    continue;
                }

                const math::Vec3 source = node.transform.position;
                const std::optional<lockstep::physics::RaycastHit> hit =
                    physics.raycast(source, math::Vec3{0.0f, -1.0f, 0.0f}, k_maxFall);

                if (!hit)
                {
                    continue;
                }

                const float height = std::max(hit->distance, 0.05f);
                drips.sites.push_back(DripSite{.source = source,
                    .floor = hit->point + math::Vec3{0.0f, k_floorLift, 0.0f},
                    .fallSeconds = std::sqrt(2.0f * height / k_gravity),
                    .untilDrop = randomUnit(drips.random) * k_firstDropSpread,
                    .interval = drips.tuning.interval});
            }

            if (!drips.sites.empty())
            {
                std::clog << "Game: " << drips.sites.size() << " Fx_Drip_* leaks\n";
            }
        }

        void land(Drips& drips, DripSite& site, render::ParticleSystem* particles, audio::AudioSystem& audioSystem)
        {
            site.dropAge = -site.untilLanding;
            site.untilLanding = -1.0f;

            if (particles != nullptr && drips.splash.isValid())
            {
                static_cast<void>(particles->burst(drips.splash, render::ParticleBurst{.position = site.floor}));
            }

            if (!drips.sounds.empty())
            {
                const auto pick = static_cast<std::size_t>(randomUnit(drips.random) * static_cast<float>(drips.sounds.size()))
                    % drips.sounds.size();
                static_cast<void>(audioSystem.play(drips.sounds[pick], audio::PlayDescription{.bus = audio::Bus::World,
                    .volume = drips.tuning.volume * (0.7f + 0.3f * randomUnit(drips.random)),
                    .pitch = 0.9f + 0.2f * randomUnit(drips.random),
                    .position = site.floor}));
            }
        }

        void updateSite(Drips& drips, DripSite& site, render::ParticleSystem* particles, audio::AudioSystem& audioSystem,
            float seconds)
        {
            site.dropAge += seconds;
            site.untilDrop -= seconds;

            if (site.untilLanding >= 0.0f)
            {
                site.untilLanding -= seconds;

                if (site.untilLanding < 0.0f)
                {
                    land(drips, site, particles, audioSystem);
                }
            }

            if (site.untilDrop > 0.0f)
            {
                return;
            }

            const float interval = nextInterval(drips);
            site.interval = std::clamp(interval, 0.3f, 4.0f);
            site.untilDrop += interval;
            site.untilLanding = site.fallSeconds;

            if (particles != nullptr && drips.drop.isValid())
            {
                static_cast<void>(particles->burst(drips.drop, render::ParticleBurst{.position = site.source,
                    .direction = math::Vec3{0.0f, -1.0f, 0.0f}}));
            }
        }
    } //namespace

    void presentDrips(Drips& drips, GameState& state, EngineContext& engine, const math::Vec3& eye, float worldSeconds)
    {
        const lockstep::assets::Scene* scene = state.level ? &state.level->scene : nullptr;
        const bool isAllowed = drips.tuning.enabled && engine.renderer.graphicsQuality().waterRipples;
        engine.renderer.settings().surfaceEffects.dripPuddles = isAllowed;

        if (!drips.isTried)
        {
            tryCreate(drips, state, engine);
        }

        if (scene != drips.scene)
        {
            findSites(drips, scene, engine.physics);
        }

        if (!isAllowed)
        {
            return;
        }

        const bool hasParticles = state.effects.particles.has_value() && state.effects.tuning.enabled;
        render::ParticleSystem* particles = hasParticles ? &*state.effects.particles : nullptr;
        const float seconds = std::max(worldSeconds, 0.0f);

        for (DripSite& site : drips.sites)
        {
            if (math::length(site.floor - eye) > drips.tuning.reach)
            {
                continue;
            }

            updateSite(drips, site, particles, engine.audio, seconds);
            engine.renderer.submitDripPuddle(render::DripPuddle{.position = site.floor,
                .radius = drips.tuning.puddleRadius,
                .dropAge = site.dropAge,
                .interval = site.interval,
                .strength = 1.0f});
        }
    }

#if LOCKSTEP_DEV_TOOLS
    void dripsDevSection(Drips& drips)
    {
        if (!ImGui::TreeNode("Drips"))
        {
            return;
        }

        DripTuning& tuning = drips.tuning;
        ImGui::Checkbox("Dripping leaks", &tuning.enabled);
        ImGui::Text("%zu Fx_Drip_* leaks", drips.sites.size());
        ImGui::SliderFloat("Interval (s)", &tuning.interval, 0.2f, 6.0f, "%.2f");
        ImGui::SliderFloat("Interval jitter", &tuning.jitter, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Puddle radius (m)", &tuning.puddleRadius, 0.05f, 1.5f, "%.2f");
        ImGui::SliderFloat("Reach (m)", &tuning.reach, 2.0f, 40.0f, "%.0f");
        ImGui::SliderFloat("Drop volume", &tuning.volume, 0.0f, 1.0f, "%.2f");
        ImGui::TreePop();
    }
#endif
} //namespace game
