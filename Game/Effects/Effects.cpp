#include "Effects/Effects.hpp"

#include "Audio/Surfaces.hpp"
#include "Effects/EffectTextures.hpp"
#include "GameState.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Core/Bounds.hpp>
#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/World/TimeDomains.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>
#include <iostream>
#include <span>
#include <string_view>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr std::string_view k_steamPrefix{"Fx_Steam_"};
        constexpr std::string_view k_smokePrefix{"Fx_Smoke_"};
        constexpr std::string_view k_dustPrefix{"Fx_Dust_"};
        constexpr float k_impactLift{0.02f};
        constexpr float k_metalPuffScale{0.5f};
        constexpr float k_moteFill{0.6f};

        struct SurfaceLook
        {
            math::Vec4 dust{1.0f, 1.0f, 1.0f, 1.0f};
            math::Vec4 chips{1.0f, 1.0f, 1.0f, 1.0f};
            bool hasSparks{false};
            float puff{1.0f};
        };

        SurfaceLook surfaceLook(Surface surface)
        {
            switch (surface)
            {
                case Surface::Metal:
                {
                    return SurfaceLook{.dust = {0.7f, 0.7f, 0.72f, 0.6f}, .chips = {0.5f, 0.5f, 0.52f, 1.0f},
                        .hasSparks = true, .puff = k_metalPuffScale};
                }
                case Surface::Wood:
                {
                    return SurfaceLook{.dust = {0.9f, 0.75f, 0.55f, 0.8f}, .chips = {0.75f, 0.5f, 0.3f, 1.0f}};
                }
                case Surface::Grass:
                {
                    return SurfaceLook{.dust = {0.7f, 0.65f, 0.45f, 0.7f}, .chips = {0.35f, 0.45f, 0.2f, 1.0f}};
                }
                case Surface::Asphalt:
                {
                    return SurfaceLook{.dust = {0.6f, 0.6f, 0.6f, 0.8f}, .chips = {0.35f, 0.35f, 0.35f, 1.0f}};
                }
                case Surface::Tile:
                {
                    return SurfaceLook{.dust = {1.0f, 1.0f, 1.0f, 0.8f}, .chips = {1.1f, 1.1f, 1.1f, 1.0f}};
                }
                case Surface::Concrete:
                case Surface::Count:
                {
                    return SurfaceLook{};
                }
            }

            return SurfaceLook{};
        }

        math::Transform worldTransform(const lockstep::assets::Scene& scene, uint32_t node)
        {
            math::Transform world = scene.nodes[node].transform;
            uint32_t parent = scene.nodes[node].parent;

            while (parent != lockstep::assets::k_noNode && parent < scene.nodes.size())
            {
                const math::Transform& local = scene.nodes[parent].transform;
                world.position = local.position + local.rotation * (local.scale * world.position);
                world.rotation = local.rotation * world.rotation;
                world.scale = local.scale * world.scale;
                parent = scene.nodes[parent].parent;
            }

            return world;
        }

        void placeMarkers(Effects& effects, const lockstep::assets::Scene* scene)
        {
            effects.particles->clear();
            effects.markers.clear();
            effects.motes.reset();
            effects.rain.streaks.reset();
            effects.wasIndoors = false;
            effects.scene = scene;

            if (scene == nullptr)
            {
                return;
            }

            for (uint32_t node = 0; node < scene->nodes.size(); ++node)
            {
                const std::string_view name = scene->nodes[node].name;
                const render::ParticleEffectId effect = name.starts_with(k_steamPrefix) ? effects.ids.steam
                    : name.starts_with(k_smokePrefix) ? effects.ids.smoke
                    : name.starts_with(k_dustPrefix) ? effects.ids.dustFall
                    : render::ParticleEffectId{};

                if (!effect.isValid())
                {
                    continue;
                }

                math::Transform transform = worldTransform(*scene, node);
                transform.scale = {1.0f, 1.0f, 1.0f};

                if (auto emitter = effects.particles->createEmitter(effect, transform))
                {
                    effects.markers.push_back(emitter.value());
                }
            }

            if (!effects.markers.empty())
            {
                std::clog << "Game: " << effects.markers.size() << " Fx_* particle markers\n";
            }
        }

        bool tryCreate(Effects& effects, render::Renderer& renderer)
        {
            effects.isTried = true;
            const std::vector<std::byte> pixels = effectAtlasPixels();
            auto texture = renderer.createTexture(render::TextureDescription{.pixels = pixels,
                .width = k_effectAtlasSize,
                .height = k_effectAtlasSize,
                .format = render::TextureFormat::Rgba8Srgb,
                .generateMips = true});

            if (!texture)
            {
                std::clog << "Game: particle atlas not created: " << texture.error().message << '\n';
                return false;
            }

            auto particles = render::ParticleSystem::create(render::ParticleSystemDescription{});

            if (!particles)
            {
                std::clog << "Game: particles not available: " << particles.error().message << '\n';
                return false;
            }

            auto ids = createEffects(particles.value(), texture.value());

            if (!ids)
            {
                std::clog << "Game: particle effects rejected: " << ids.error().message << '\n';
                return false;
            }

            effects.atlas = texture.value();
            effects.particles.emplace(std::move(particles).value());
            effects.ids = ids.value();

            return true;
        }

        bool isInside(std::span<const InteriorBox> boxes, const math::Vec3& point)
        {
            return std::any_of(boxes.begin(), boxes.end(), [&point](const InteriorBox& box)
            {
                return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y && point.y <= box.max.y
                    && point.z >= box.min.z && point.z <= box.max.z;
            });
        }

        void updateMotes(Effects& effects, std::span<const InteriorBox> interiors, const math::Vec3& eye)
        {
            render::ParticleSystem& particles = *effects.particles;
            const bool isIndoors = effects.tuning.dustMotes && isInside(interiors, eye);

            if (!effects.motes)
            {
                if (auto emitter = particles.createEmitter(effects.ids.dustMotes, math::Transform{.position = eye}))
                {
                    effects.motes = emitter.value();
                }
            }

            if (!effects.motes)
            {
                return;
            }

            particles.setEmitterTransform(*effects.motes, math::Transform{.position = eye});
            particles.setEmitterRate(*effects.motes, isIndoors ? effects.tuning.moteDensity : 0.0f);

            if (isIndoors && !effects.wasIndoors)
            {
                static_cast<void>(particles.burst(effects.ids.dustMotes, render::ParticleBurst{.position = eye,
                    .countScale = k_moteFill * effects.tuning.moteDensity}));
            }

            effects.wasIndoors = isIndoors;
        }

        void updateRain(GameState& state, EngineContext& engine, const math::Vec3& eye, float worldSeconds,
            float frameSeconds)
        {
            Effects& effects = state.effects;
            const WeatherTuning& tuning = state.weather.tuning;
            const float rain = rainAmount(state.weather);
            std::vector<math::Aabb> boxes;
            boxes.reserve(state.combatAudio.interiors.size());

            for (const InteriorBox& box : state.combatAudio.interiors)
            {
                boxes.push_back(math::Aabb{.min = box.min, .max = box.max});
            }

            effects.particles->setExclusionBoxes(boxes);
            updateRainParticles(effects.rain, *effects.particles, effects.ids, RainFrame{
                .streaks = tuning.particles ? rain * tuning.streakDensity : 0.0f,
                .splashes = tuning.splashes ? rain * tuning.splashRate : 0.0f,
                .physics = &engine.physics,
                .interiors = state.combatAudio.interiors,
                .eye = eye,
                .worldSeconds = worldSeconds,
                .frameSeconds = frameSeconds,
                .worldScale = engine.timeDomains.scale(lockstep::world::TimeDomain::World)});
        }

        void spawnImpact(Effects& effects, const SceneLevel* level, const CombatImpact& impact)
        {
            render::ParticleSystem& particles = *effects.particles;
            const SurfaceLook look = surfaceLook(surfaceOfBody(level, impact.body));
            const float scale = effects.tuning.impactScale;
            const math::Vec3 normal = math::normalize(impact.normal);
            const math::Vec3 origin = impact.point + normal * k_impactLift;

            static_cast<void>(particles.burst(effects.ids.impactPuff, render::ParticleBurst{.position = origin,
                .direction = normal, .countScale = scale * look.puff, .tint = look.dust}));
            static_cast<void>(particles.burst(effects.ids.impactGrit, render::ParticleBurst{.position = origin,
                .direction = normal, .countScale = scale * look.puff, .tint = look.dust}));
            static_cast<void>(particles.burst(effects.ids.impactChunks, render::ParticleBurst{.position = origin,
                .direction = normal, .countScale = scale, .tint = look.chips}));

            if (look.hasSparks)
            {
                static_cast<void>(particles.burst(effects.ids.sparks, render::ParticleBurst{.position = origin,
                    .direction = normal, .countScale = scale}));
            }
        }

        void spawnImpacts(Effects& effects, const GameState& state)
        {
            const SceneLevel* level = state.level ? &*state.level : nullptr;

            if (!effects.tuning.impacts)
            {
                return;
            }

            for (const CombatImpact& impact : state.combatAudio.impacts)
            {
                if (!impact.isFlesh)
                {
                    spawnImpact(effects, level, impact);
                }
            }
        }
    } //namespace

    void presentEffects(GameState& state, EngineContext& engine, const math::Vec3& eye, float worldSeconds,
        float frameSeconds)
    {
        Effects& effects = state.effects;
        effects.eye = eye;

        if (!effects.isTried && !tryCreate(effects, engine.renderer))
        {
            return;
        }

        if (!effects.particles)
        {
            return;
        }

        const lockstep::assets::Scene* scene = state.level ? &state.level->scene : nullptr;

        if (scene != effects.scene)
        {
            placeMarkers(effects, scene);
        }

        if (!effects.tuning.enabled)
        {
            return;
        }

        render::ParticleSystem& particles = *effects.particles;

        for (const render::ParticleEmitterId emitter : effects.markers)
        {
            particles.setEmitterRate(emitter, effects.tuning.markers ? 1.0f : 0.0f);
        }

        updateMotes(effects, state.combatAudio.interiors, eye);
        updateRain(state, engine, eye, worldSeconds, frameSeconds);
        spawnImpacts(effects, state);
        particles.update(worldSeconds);
        particles.submit(engine.renderer);
    }

#if LOCKSTEP_DEV_TOOLS
    void effectsDevSection(Effects& effects)
    {
        if (!ImGui::TreeNode("Particles"))
        {
            return;
        }

        EffectsTuning& tuning = effects.tuning;
        ImGui::Checkbox("Enabled", &tuning.enabled);
        ImGui::Checkbox("Dust motes indoors", &tuning.dustMotes);
        ImGui::Checkbox("Bullet impacts", &tuning.impacts);
        ImGui::Checkbox("Muzzle smoke", &tuning.muzzleSmoke);
        ImGui::Checkbox("Fx_* markers", &tuning.markers);
        ImGui::SliderFloat("Mote density", &tuning.moteDensity, 0.0f, 4.0f);
        ImGui::SliderFloat("Impact amount", &tuning.impactScale, 0.0f, 4.0f);

        if (!effects.particles)
        {
            ImGui::TextDisabled("Particles not available");
            ImGui::TreePop();
            return;
        }

        const lockstep::render::ParticleStats stats = effects.particles->stats();
        ImGui::Text("%u alive, %u emitters (%zu markers), %llu dropped", stats.alive, stats.emitters,
            effects.markers.size(), static_cast<unsigned long long>(stats.dropped));
        ImGui::Text("CPU: update %.2f ms, submit %.2f ms", stats.updateMilliseconds, stats.submitMilliseconds);
        ImGui::SliderInt("Stress count", &tuning.stressCount, 1000, 100000);

        if (ImGui::Button("Stress test around the camera"))
        {
            static_cast<void>(effects.particles->burst(effects.ids.stress, lockstep::render::ParticleBurst{
                .position = effects.eye, .countScale = static_cast<float>(tuning.stressCount)}));
        }

        if (ImGui::Button("Clear particles"))
        {
            effects.particles->clear();
            effects.motes.reset();
            effects.rain.streaks.reset();
            effects.scene = nullptr;
        }

        ImGui::TreePop();
    }
#endif
} //namespace game
