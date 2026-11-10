#include "Effects/GlassShatter.hpp"

#include "Effects/EffectTextures.hpp"
#include "GameState.hpp"

#include <Lockstep/Audio/AudioSystem.hpp>
#include <Lockstep/Physics/PhysicsWorld.hpp>
#include <Lockstep/Render/PbrMaterial.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>

namespace game
{
    namespace
    {
        namespace audio = lockstep::audio;
        namespace render = lockstep::render;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr float k_gravity{-9.81f};
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr math::Vec4 k_glassTint{0.72f, 0.82f, 0.8f, 0.8f};
        constexpr float k_pointsPerSquareMetre{6.0f};
        constexpr int k_minPoints{2};
        constexpr int k_maxPoints{12};
        constexpr float k_debrisPerSquareMetre{30.0f};
        constexpr int k_minDebris{6};
        constexpr int k_maxDebrisPerPane{48};
        constexpr float k_throughShare{0.7f};
        constexpr float k_maxScatter{1.1f};
        constexpr float k_floorProbe{4.0f};
        constexpr float k_floorLift{0.002f};
        constexpr float k_minFloorNormal{0.7f};
        constexpr float k_breakVolume{0.9f};
        constexpr float k_breakMinDistance{2.0f};
        constexpr float k_breakMaxDistance{45.0f};

        uint32_t nextRandom(GlassShatter& shatter)
        {
            shatter.random ^= shatter.random << 13;
            shatter.random ^= shatter.random >> 17;
            shatter.random ^= shatter.random << 5;

            return shatter.random;
        }

        float randomRange(GlassShatter& shatter, float low, float high)
        {
            return low + (high - low) * static_cast<float>(nextRandom(shatter) & 0xFFFFu) / 65535.0f;
        }

        render::ParticleEffectDescription atlasEffect(render::TextureHandle atlas, uint32_t firstFrame, uint32_t frameCount)
        {
            render::ParticleEffectDescription effect{};
            effect.texture = atlas;
            effect.atlasColumns = k_effectAtlasColumns;
            effect.atlasRows = k_effectAtlasRows;
            effect.firstFrame = firstFrame;
            effect.frameCount = frameCount;

            return effect;
        }

        render::ParticleEffectDescription shards(render::TextureHandle atlas, math::Vec2 speed, float cone)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_chunkFrame, k_chunkFrames);
            effect.burstCount = 8;
            effect.lifetime = {0.7f, 1.5f};
            effect.speed = speed;
            effect.coneAngle = cone;
            effect.spawnExtent = {0.05f, 0.05f, 0.05f};
            effect.acceleration = {0.0f, k_gravity, 0.0f};
            effect.drag = 0.25f;
            effect.size = {0.022f, 0.022f, 0.02f, 0.5f};
            effect.sizeScale = {0.4f, 1.4f};
            effect.color = {k_glassTint, k_glassTint, {k_glassTint.x, k_glassTint.y, k_glassTint.z, 0.0f}, 0.85f};
            effect.spin = {6.0f, 20.0f};
            effect.softness = 0.0f;
            effect.scattering = 0.5f;

            return effect;
        }

        render::ParticleEffectDescription glints(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_sparkFrame, 1);
            effect.blend = render::ParticleBlend::Additive;
            effect.shape = render::ParticleShape::Stretched;
            effect.burstCount = 6;
            effect.lifetime = {0.25f, 0.9f};
            effect.speed = {1.5f, 4.5f};
            effect.coneAngle = 0.9f;
            effect.spawnExtent = {0.05f, 0.05f, 0.05f};
            effect.acceleration = {0.0f, k_gravity, 0.0f};
            effect.drag = 0.4f;
            effect.size = {0.008f, 0.007f, 0.004f, 0.5f};
            effect.sizeScale = {0.6f, 1.4f};
            effect.color = {{2.2f, 2.4f, 2.5f, 1.0f}, {1.6f, 1.8f, 1.9f, 0.8f}, {1.0f, 1.1f, 1.2f, 0.0f}, 0.4f};
            effect.stretch = 0.012f;
            effect.softness = 0.0f;
            effect.scattering = 0.8f;

            return effect;
        }

        render::ParticleEffectDescription dust(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_smokeFrame, k_smokeFrames);
            effect.burstCount = 2;
            effect.lifetime = {0.6f, 1.2f};
            effect.speed = {0.2f, 0.8f};
            effect.coneAngle = 0.8f;
            effect.acceleration = {0.0f, -0.4f, 0.0f};
            effect.drag = 2.5f;
            effect.size = {0.08f, 0.25f, 0.4f, 0.4f};
            effect.sizeScale = {0.7f, 1.3f};
            effect.color = {{0.85f, 0.9f, 0.9f, 0.18f}, {0.85f, 0.9f, 0.9f, 0.1f}, {0.85f, 0.9f, 0.9f, 0.0f}, 0.4f};
            effect.spin = {0.2f, 0.8f};
            effect.softness = 0.1f;
            effect.scattering = 0.4f;

            return effect;
        }

        bool createParticleEffects(GlassShatter& shatter, render::ParticleSystem& particles, render::TextureHandle atlas)
        {
            const std::array<std::pair<render::ParticleEffectId*, render::ParticleEffectDescription>, 4> effects{{
                {&shatter.flying, shards(atlas, {1.5f, 4.5f}, 0.6f)},
                {&shatter.falling, shards(atlas, {0.1f, 0.9f}, math::k_pi)},
                {&shatter.glints, glints(atlas)},
                {&shatter.dust, dust(atlas)}}};

            for (const auto& [id, description] : effects)
            {
                auto created = particles.createEffect(description);

                if (!created)
                {
                    std::clog << "Game: glass shard particles not created: " << created.error().message << '\n';
                    return false;
                }

                *id = created.value();
            }

            return true;
        }

        lockstep::Result<render::MeshHandle> createShardMesh(render::Renderer& renderer, std::size_t shape)
        {
            constexpr std::array<std::array<math::Vec2, 4>, k_glassShardShapes> k_outlines{{
                {{{-0.5f, -0.35f}, {0.55f, -0.2f}, {0.1f, 0.5f}, {0.1f, 0.5f}}},
                {{{-0.3f, -0.5f}, {0.25f, -0.45f}, {0.45f, 0.35f}, {-0.4f, 0.2f}}},
                {{{-0.15f, -0.6f}, {0.2f, -0.3f}, {0.1f, 0.6f}, {-0.2f, 0.1f}}}}};
            std::vector<render::PbrVertex> vertices;

            for (const math::Vec2& corner : k_outlines[shape])
            {
                vertices.push_back(render::PbrVertex{{corner.x, 0.0f, corner.y}, {0.0f, 1.0f, 0.0f},
                    {corner.x + 0.5f, corner.y + 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}});
            }

            const std::vector<uint32_t> indices{0, 2, 1, 0, 3, 2};

            return renderer.createMesh(std::span<const render::PbrVertex>(vertices), std::span<const uint32_t>(indices));
        }

        bool createDebrisAssets(GlassShatter& shatter, render::Renderer& renderer)
        {
            for (std::size_t shape = 0; shape < shatter.shardMeshes.size(); ++shape)
            {
                auto mesh = createShardMesh(renderer, shape);

                if (!mesh)
                {
                    std::clog << "Game: glass shard mesh not created: " << mesh.error().message << '\n';
                    return false;
                }

                shatter.shardMeshes[shape] = mesh.value();
            }

            auto look = renderer.createMaterial(render::PbrMaterialDescription{
                .baseColorFactor = {0.78f, 0.88f, 0.86f, 0.55f},
                .metallicFactor = 0.0f,
                .roughnessFactor = 0.04f,
                .alphaMode = render::AlphaMode::Blend,
                .doubleSided = true});

            if (!look)
            {
                std::clog << "Game: glass shard material not created: " << look.error().message << '\n';
                return false;
            }

            shatter.shardLook = look.value();

            return true;
        }

        void tryCreate(GlassShatter& shatter, GameState& state, render::Renderer& renderer)
        {
            if (shatter.isTried || !state.effects.particles)
            {
                return;
            }

            shatter.isTried = true;

            if (!createParticleEffects(shatter, *state.effects.particles, state.effects.atlas))
            {
                shatter.flying = {};
            }

            shatter.hasDebrisAssets = createDebrisAssets(shatter, renderer);
        }

        math::Vec3 pointOnPane(GlassShatter& shatter, const GlassPane& pane)
        {
            return pane.centre + pane.halfU * randomRange(shatter, -1.0f, 1.0f) + pane.halfV * randomRange(shatter, -1.0f, 1.0f);
        }

        float paneArea(const GlassPane& pane)
        {
            return 4.0f * math::length(pane.halfU) * math::length(pane.halfV);
        }

        math::Vec3 throughSide(const GlassPane& pane, const math::Vec3& direction)
        {
            return math::dot(direction, pane.normal) >= 0.0f ? pane.normal : pane.normal * -1.0f;
        }

        void burstParticles(GlassShatter& shatter, render::ParticleSystem& particles, const GlassPane& pane,
            const GlassBreak& hit)
        {
            const math::Vec3 through = throughSide(pane, hit.direction);
            const math::Vec3 flight = math::normalize(hit.direction + through);
            const int points = std::clamp(static_cast<int>(paneArea(pane) * k_pointsPerSquareMetre), k_minPoints, k_maxPoints);

            particles.burst(shatter.flying, render::ParticleBurst{.position = hit.point, .direction = flight, .countScale = 1.5f});
            particles.burst(shatter.glints, render::ParticleBurst{.position = hit.point, .direction = flight, .countScale = 1.5f});
            particles.burst(shatter.dust, render::ParticleBurst{.position = hit.point, .direction = through});

            for (int point = 0; point < points; ++point)
            {
                const math::Vec3 position = pointOnPane(shatter, pane);
                particles.burst(shatter.falling, render::ParticleBurst{.position = position, .direction = through});
                particles.burst(shatter.flying, render::ParticleBurst{.position = position, .direction = flight,
                    .countScale = 0.4f});
                particles.burst(shatter.glints, render::ParticleBurst{.position = position, .direction = through,
                    .countScale = 0.5f});
            }
        }

        void addDebris(GlassShatter& shatter, const GlassDebris& piece)
        {
            if (shatter.debris.size() < k_maxGlassDebris)
            {
                shatter.debris.push_back(piece);
                return;
            }

            shatter.debris[shatter.nextDebris] = piece;
            shatter.nextDebris = (shatter.nextDebris + 1) % k_maxGlassDebris;
        }

        void scatterDebris(GlassShatter& shatter, const lockstep::physics::PhysicsWorld& physics, const GlassPane& pane,
            const GlassBreak& hit)
        {
            const math::Vec3 through = throughSide(pane, hit.direction);
            const int pieces = std::clamp(static_cast<int>(paneArea(pane) * k_debrisPerSquareMetre), k_minDebris,
                k_maxDebrisPerPane);

            for (int piece = 0; piece < pieces; ++piece)
            {
                const float side = randomRange(shatter, 0.0f, 1.0f) < k_throughShare ? 1.0f : -1.0f;
                const float reach = k_maxScatter * randomRange(shatter, 0.0f, 1.0f) * randomRange(shatter, 0.2f, 1.0f);
                math::Vec3 start = pointOnPane(shatter, pane) + through * (side * reach);
                start.y = pane.centre.y;

                const std::optional<lockstep::physics::RaycastHit> floor = physics.raycast(start, k_up * -1.0f, k_floorProbe);

                if (!floor || floor->normal.y < k_minFloorNormal)
                {
                    continue;
                }

                const float size = randomRange(shatter, 0.015f, 0.055f);
                addDebris(shatter, GlassDebris{.transform = math::Transform{.position = floor->point + floor->normal * k_floorLift,
                    .rotation = math::angleAxis(randomRange(shatter, 0.0f, 2.0f * math::k_pi), k_up),
                    .scale = {size, 1.0f, size}},
                    .shape = static_cast<uint8_t>(nextRandom(shatter) % k_glassShardShapes)});
            }
        }

        void playBreak(GlassShatter& shatter, audio::AudioSystem& audioSystem, const CombatSoundBank& bank,
            const math::Vec3& position)
        {
            if (bank.glassBreaks.empty())
            {
                return;
            }

            const audio::SoundHandle sound = bank.glassBreaks[nextRandom(shatter) % bank.glassBreaks.size()];

            if (auto played = audioSystem.play(sound, audio::PlayDescription{.bus = audio::Bus::World,
                    .volume = k_breakVolume * randomRange(shatter, 0.85f, 1.0f),
                    .pitch = randomRange(shatter, 0.93f, 1.07f),
                    .position = position}); !played)
            {
                std::clog << "Game: glass break sound not played: " << played.error().message << '\n';
            }
        }
    } //namespace

    void presentGlassShatter(GameState& state, EngineContext& engine)
    {
        GlassShatter& shatter = state.glassShatter;
        tryCreate(shatter, state, engine.renderer);
        const BreakableGlass& glass = state.glass;

        for (const GlassBreak& hit : glass.shown)
        {
            if (hit.pane >= glass.panes.size())
            {
                continue;
            }

            const GlassPane& pane = glass.panes[hit.pane];

            if (state.effects.particles && shatter.flying.isValid())
            {
                burstParticles(shatter, *state.effects.particles, pane, hit);
            }

            if (shatter.hasDebrisAssets)
            {
                scatterDebris(shatter, engine.physics, pane, hit);
            }

            playBreak(shatter, engine.audio, state.combatAudio.bank, hit.point);
        }

        state.glass.shown.clear();

        if (!shatter.hasDebrisAssets)
        {
            return;
        }

        for (const GlassDebris& piece : shatter.debris)
        {
            engine.renderer.submit(shatter.shardMeshes[piece.shape], shatter.shardLook, piece.transform);
        }
    }

    void clearGlassDebris(GlassShatter& shatter)
    {
        shatter.debris.clear();
        shatter.nextDebris = 0;
    }
} //namespace game
