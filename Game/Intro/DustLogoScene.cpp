#include "Intro/DustLogoScene.hpp"

#include "Intro/Wordmark.hpp"

#include <Lockstep/Render/Camera.hpp>
#include <Lockstep/Render/Lighting.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;

        constexpr DustLogoTiming k_timing{};
        constexpr uint32_t k_wordmarkMotes{1900};
        constexpr uint32_t k_looseMotes{650};
        constexpr uint32_t k_spriteSize{32};
        constexpr uint32_t k_floorTextureSize{4};
        constexpr float k_floorHalfSize{12.0f};
        constexpr float k_moteSize{0.016f};
        constexpr math::Vec3 k_logoCentre{0.0f, 1.5f, 0.0f};
        constexpr math::Vec3 k_cameraPosition{0.0f, 1.42f, 4.3f};
        constexpr math::Vec3 k_cameraTarget{0.0f, 1.45f, 0.0f};
        constexpr float k_cameraFovDegrees{36.0f};
        constexpr float k_cameraDriftMetres{0.06f};
        constexpr math::Vec3 k_spotPosition{-0.9f, 5.6f, -1.8f};
        constexpr math::Vec3 k_spotTarget{0.15f, 1.0f, 0.4f};
        constexpr float k_spotIntensity{90.0f};
        constexpr math::Vec3 k_spotColour{1.0f, 0.93f, 0.82f};
        constexpr math::Vec3 k_dustColour{1.0f, 0.95f, 0.86f};
        constexpr math::Vec3 k_windAcceleration{7.0f, 1.4f, -2.2f};
        constexpr float k_windPushSeconds{0.45f};
        constexpr float k_gustSweepPerMetre{0.08f};
        constexpr float k_drag{1.1f};
        constexpr float k_turbulence{2.4f};
        constexpr float k_blownFadeDelay{0.5f};
        constexpr float k_blownFadeSeconds{2.0f};
        constexpr float k_holdShimmer{0.004f};
        constexpr float k_wanderMetres{0.12f};
        constexpr float k_maxDelta{1.0f / 30.0f};
        constexpr std::array<uint8_t, 3> k_floorColour{38, 36, 34};

        float smootherStep(float value)
        {
            const float t = std::clamp(value, 0.0f, 1.0f);
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        math::Vec3 wander(const DustMote& mote, float seconds)
        {
            return math::Vec3{std::sin(0.37f * seconds + mote.phase.x), std::sin(0.29f * seconds + mote.phase.y) * 0.6f,
                       std::sin(0.23f * seconds + mote.phase.z)}
                * k_wanderMetres;
        }

        math::Vec3 floating(const DustMote& mote, float seconds)
        {
            return mote.start + mote.drift * seconds + wander(mote, seconds);
        }

        math::Vec3 gathered(const DustMote& mote, float seconds)
        {
            const math::Vec3 free = floating(mote, seconds);

            if (!mote.joins)
            {
                return free;
            }

            const float progress = smootherStep((seconds - k_timing.gatherStart - mote.delay) / mote.gatherSeconds);
            const math::Vec3 shimmer = math::Vec3{std::sin(5.1f * seconds + mote.phase.x),
                                           std::sin(4.3f * seconds + mote.phase.y), 0.0f}
                * k_holdShimmer;
            const math::Vec3 settled = mote.target + shimmer;
            return math::lerp(free, settled, progress) + mote.swirl * std::sin(math::k_pi * progress);
        }

        float signedRandom(GoreRandom& random)
        {
            return random.range(-1.0f, 1.0f);
        }

        DustMote looseMote(GoreRandom& random)
        {
            DustMote mote;
            mote.start = k_logoCentre
                + math::Vec3{random.range(-4.5f, 4.5f), random.range(-1.4f, 2.4f), random.range(-3.0f, 1.8f)};
            mote.drift = math::Vec3{0.05f + 0.06f * signedRandom(random), -0.015f + 0.03f * signedRandom(random),
                0.04f * signedRandom(random)};
            mote.phase = math::Vec3{random.range(0.0f, 6.3f), random.range(0.0f, 6.3f), random.range(0.0f, 6.3f)};
            mote.size = random.range(0.5f, 1.5f);
            mote.brightness = random.range(0.35f, 1.0f);
            return mote;
        }

        std::vector<DustMote> placeMotes(GoreRandom& random)
        {
            const std::vector<math::Vec3> targets = sampleVelaWordmark({.points = k_wordmarkMotes});
            std::vector<DustMote> motes;
            motes.reserve(targets.size() + k_looseMotes);

            for (const math::Vec3& target : targets)
            {
                DustMote mote = looseMote(random);
                mote.joins = true;
                mote.target = k_logoCentre + target;
                mote.swirl = math::Vec3{signedRandom(random), signedRandom(random), signedRandom(random)} * 0.45f;
                mote.delay = random.range(0.0f, 1.3f);
                mote.gatherSeconds = random.range(1.6f, 2.1f);
                mote.size = random.range(0.7f, 1.2f);
                mote.brightness = random.range(0.75f, 1.0f);
                motes.push_back(mote);
            }

            float leftmost = 0.0f;

            for (const math::Vec3& target : targets)
            {
                leftmost = std::min(leftmost, target.x);
            }

            for (DustMote& mote : motes)
            {
                mote.gustDelay = (mote.target.x - k_logoCentre.x - leftmost) * k_gustSweepPerMetre;
            }

            for (uint32_t index = 0; index < k_looseMotes; ++index)
            {
                DustMote mote = looseMote(random);
                mote.gustDelay = std::max(0.0f, (mote.start.x + 2.0f) * k_gustSweepPerMetre);
                motes.push_back(mote);
            }

            return motes;
        }

        std::vector<std::byte> spritePixels()
        {
            std::vector<std::byte> pixels;
            pixels.reserve(static_cast<std::size_t>(k_spriteSize) * k_spriteSize * 4);
            const float half = static_cast<float>(k_spriteSize) * 0.5f;

            for (uint32_t y = 0; y < k_spriteSize; ++y)
            {
                for (uint32_t x = 0; x < k_spriteSize; ++x)
                {
                    const float dx = (static_cast<float>(x) + 0.5f - half) / half;
                    const float dy = (static_cast<float>(y) + 0.5f - half) / half;
                    const float falloff = std::clamp(1.0f - std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
                    const auto alpha = static_cast<uint8_t>(std::pow(falloff, 1.6f) * 255.0f);
                    pixels.insert(pixels.end(), {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{alpha}});
                }
            }

            return pixels;
        }

        lockstep::Status createFloor(DustLogoScene& scene, render::Renderer& renderer)
        {
            std::vector<std::byte> pixels;

            for (uint32_t index = 0; index < k_floorTextureSize * k_floorTextureSize; ++index)
            {
                pixels.insert(pixels.end(),
                    {std::byte{k_floorColour[0]}, std::byte{k_floorColour[1]}, std::byte{k_floorColour[2]}, std::byte{255}});
            }

            auto texture = renderer.createTexture(
                {.pixels = pixels, .width = k_floorTextureSize, .height = k_floorTextureSize});

            if (!texture)
            {
                return texture.error();
            }

            scene.floorTexture = texture.value();
            auto material = renderer.createMaterial(
                render::MaterialDescription{.albedo = scene.floorTexture, .specular = 0.08f, .shininess = 10.0f});

            if (!material)
            {
                return material.error();
            }

            scene.floorMaterial = material.value();
            const float s = k_floorHalfSize;
            const std::array<render::Vertex, 4> vertices{{
                {{-s, 0.0f, -s}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
                {{-s, 0.0f, s}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
                {{s, 0.0f, s}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
                {{s, 0.0f, -s}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            }};
            const std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
            auto mesh = renderer.createMesh(std::span<const render::Vertex>(vertices), indices);

            if (!mesh)
            {
                return mesh.error();
            }

            scene.floorMesh = mesh.value();
            return {};
        }

        lockstep::Status createSprites(DustLogoScene& scene, render::Renderer& renderer)
        {
            const std::vector<std::byte> pixels = spritePixels();
            auto sprite = renderer.createTexture({.pixels = pixels, .width = k_spriteSize, .height = k_spriteSize});

            if (!sprite)
            {
                return sprite.error();
            }

            scene.sprite = sprite.value();
            const math::Vec4 colour{k_dustColour.x, k_dustColour.y, k_dustColour.z, 1.0f};
            auto effect = scene.particles.createEffect({.texture = scene.sprite,
                .blend = render::ParticleBlend::Additive,
                .lighting = render::ParticleLighting::Lit,
                .burstCount = 1,
                .lifetime = {1000.0f, 1000.0f},
                .size = {k_moteSize, k_moteSize, k_moteSize, 0.5f},
                .color = {colour, colour, colour, 0.5f},
                .softness = 0.02f,
                .scattering = 0.7f});

            if (!effect)
            {
                return effect.error();
            }

            scene.effect = effect.value();
            return {};
        }

        void blowMote(DustMote& mote, GoreRandom& random, float seconds, float delta)
        {
            const float sinceGust = seconds - k_timing.gust - mote.gustDelay;

            if (sinceGust < 0.0f)
            {
                return;
            }

            if (!mote.isBlown)
            {
                mote.isBlown = true;
                mote.velocity = math::Vec3{random.range(0.0f, 0.6f), random.range(-0.2f, 0.4f), signedRandom(random) * 0.4f};
            }

            const float push = sinceGust < k_windPushSeconds ? random.range(0.6f, 1.4f) : 0.0f;
            const math::Vec3 swirl{std::sin(2.3f * mote.position.y + mote.phase.x + 3.1f * seconds),
                std::sin(1.9f * mote.position.z + mote.phase.y + 2.7f * seconds),
                std::sin(2.1f * mote.position.x + mote.phase.z + 2.3f * seconds)};
            mote.velocity += (k_windAcceleration * push + swirl * k_turbulence) * delta;
            mote.velocity *= std::exp(-k_drag * delta);
            mote.position += mote.velocity * delta;
        }

        float moteOpacity(const DustMote& mote, float seconds)
        {
            if (!mote.isBlown)
            {
                return mote.brightness;
            }

            const float fade = (seconds - k_timing.gust - mote.gustDelay - k_blownFadeDelay) / k_blownFadeSeconds;
            return mote.brightness * (1.0f - std::clamp(fade, 0.0f, 1.0f));
        }

        void submitView(render::Renderer& renderer, float seconds, float light)
        {
            const float drift = std::sin(seconds * 0.35f) * k_cameraDriftMetres;
            renderer.setCamera(render::Camera{.position = k_cameraPosition + math::Vec3{drift, drift * 0.3f, 0.0f},
                .target = k_cameraTarget,
                .verticalFov = math::radians(k_cameraFovDegrees),
                .nearPlane = 0.05f,
                .farPlane = 60.0f});

            const math::Vec3 direction = math::normalize(k_spotTarget - k_spotPosition);
            renderer.submitLight(render::SpotLight{.position = k_spotPosition,
                .direction = direction,
                .color = k_spotColour,
                .intensity = k_spotIntensity * light,
                .range = 12.0f,
                .innerAngle = math::radians(9.0f),
                .outerAngle = math::radians(20.0f),
                .castsShadows = false,
                .volumetric = 1.0f});
        }
    } //namespace

    lockstep::Result<DustLogoScene> createDustLogoScene(render::Renderer& renderer)
    {
        auto particles = render::ParticleSystem::create({.maxParticles = k_wordmarkMotes + k_looseMotes + 64});

        if (!particles)
        {
            return particles.error();
        }

        DustLogoScene scene{.particles = std::move(particles).value(),
            .effect = {},
            .sprite = {},
            .floorMesh = {},
            .floorMaterial = {},
            .floorTexture = {},
            .motes = {},
            .random = goreRandom(23)};

        if (auto created = createSprites(scene, renderer); !created)
        {
            destroyDustLogoScene(scene, renderer);
            return created.error();
        }

        if (auto created = createFloor(scene, renderer); !created)
        {
            destroyDustLogoScene(scene, renderer);
            return created.error();
        }

        scene.motes = placeMotes(scene.random);
        return scene;
    }

    void destroyDustLogoScene(DustLogoScene& scene, render::Renderer& renderer)
    {
        scene.particles.clear();
        renderer.destroyMesh(scene.floorMesh);
        renderer.destroyMaterial(scene.floorMaterial);
        renderer.destroyTexture(scene.floorTexture);
        renderer.destroyTexture(scene.sprite);
        scene.floorMesh = {};
        scene.floorMaterial = {};
        scene.floorTexture = {};
        scene.sprite = {};
    }

    void presentDustLogoScene(DustLogoScene& scene, render::Renderer& renderer, float seconds, float delta, float light)
    {
        const float step = std::clamp(delta, 0.0f, k_maxDelta);
        scene.particles.clear();

        for (DustMote& mote : scene.motes)
        {
            if (seconds < k_timing.gust + mote.gustDelay)
            {
                mote.position = gathered(mote, seconds);
            }
            else
            {
                blowMote(mote, scene.random, seconds, step);
            }

            const float opacity = moteOpacity(mote, seconds);

            if (opacity <= 0.0f)
            {
                continue;
            }

            scene.particles.burst(scene.effect, render::ParticleBurst{.position = mote.position,
                                                    .sizeScale = mote.size,
                                                    .tint = {1.0f, 1.0f, 1.0f, opacity}});
        }

        scene.particles.update(0.0f);
        scene.particles.submit(renderer);
        submitView(renderer, seconds, light);
        renderer.submit(scene.floorMesh, scene.floorMaterial, math::Transform{});
    }
} //namespace game
