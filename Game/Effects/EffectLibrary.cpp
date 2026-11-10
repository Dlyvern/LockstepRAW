#include "Effects/EffectLibrary.hpp"

#include "Effects/EffectTextures.hpp"

#include <utility>

namespace game
{
    namespace
    {
        namespace math = lockstep::math;
        namespace render = lockstep::render;

        constexpr float k_gravity{-9.81f};
        constexpr float k_rainStretch{0.03f};

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

        render::ParticleEffectDescription dustMotes(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_dotFrame, 1);
            effect.blend = render::ParticleBlend::Additive;
            effect.spawnRate = 70.0f;
            effect.burstCount = 500;
            effect.lifetime = {6.0f, 11.0f};
            effect.speed = {0.0f, 0.03f};
            effect.coneAngle = math::k_pi;
            effect.spawnExtent = {4.0f, 2.0f, 4.0f};
            effect.acceleration = {0.0f, -0.004f, 0.0f};
            effect.drag = 0.6f;
            effect.jitter = 0.04f;
            effect.size = {0.008f, 0.008f, 0.008f, 0.5f};
            effect.sizeScale = {0.6f, 1.4f};
            effect.color = {{0.5f, 0.48f, 0.45f, 0.0f}, {0.5f, 0.48f, 0.45f, 0.9f}, {0.5f, 0.48f, 0.45f, 0.0f}, 0.5f};
            effect.softness = 0.02f;
            effect.scattering = 0.7f;

            return effect;
        }

        render::ParticleEffectDescription impactPuff(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_smokeFrame, k_smokeFrames);
            effect.burstCount = 6;
            effect.lifetime = {1.2f, 2.6f};
            effect.speed = {0.4f, 1.8f};
            effect.coneAngle = 0.6f;
            effect.spawnExtent = {0.03f, 0.03f, 0.03f};
            effect.acceleration = {0.0f, 0.05f, 0.0f};
            effect.drag = 3.0f;
            effect.jitter = 0.2f;
            effect.size = {0.1f, 0.4f, 0.7f, 0.3f};
            effect.sizeScale = {0.7f, 1.3f};
            effect.color = {{0.62f, 0.58f, 0.52f, 0.6f}, {0.62f, 0.58f, 0.52f, 0.35f}, {0.62f, 0.58f, 0.52f, 0.0f}, 0.3f};
            effect.spin = {0.2f, 0.9f};
            effect.softness = 0.15f;
            effect.scattering = 0.3f;

            return effect;
        }

        render::ParticleEffectDescription impactGrit(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_dotFrame, 1);
            effect.burstCount = 28;
            effect.lifetime = {0.8f, 2.2f};
            effect.speed = {0.3f, 1.6f};
            effect.coneAngle = 1.0f;
            effect.acceleration = {0.0f, -2.5f, 0.0f};
            effect.drag = 2.0f;
            effect.jitter = 0.3f;
            effect.size = {0.012f, 0.012f, 0.01f, 0.5f};
            effect.sizeScale = {0.6f, 1.6f};
            effect.color = {{0.7f, 0.66f, 0.6f, 0.9f}, {0.7f, 0.66f, 0.6f, 0.7f}, {0.7f, 0.66f, 0.6f, 0.0f}, 0.5f};
            effect.softness = 0.02f;
            effect.scattering = 0.3f;

            return effect;
        }

        render::ParticleEffectDescription impactChunks(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_chunkFrame, k_chunkFrames);
            effect.burstCount = 9;
            effect.lifetime = {0.6f, 1.3f};
            effect.speed = {1.5f, 4.5f};
            effect.coneAngle = 0.75f;
            effect.acceleration = {0.0f, k_gravity, 0.0f};
            effect.drag = 0.3f;
            effect.size = {0.03f, 0.03f, 0.03f, 0.5f};
            effect.sizeScale = {0.5f, 1.3f};
            effect.color = {{0.75f, 0.72f, 0.68f, 1.0f}, {0.75f, 0.72f, 0.68f, 1.0f}, {0.75f, 0.72f, 0.68f, 0.0f}, 0.85f};
            effect.spin = {5.0f, 16.0f};
            effect.softness = 0.0f;

            return effect;
        }

        render::ParticleEffectDescription sparks(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_sparkFrame, 1);
            effect.blend = render::ParticleBlend::Additive;
            effect.lighting = render::ParticleLighting::Unlit;
            effect.shape = render::ParticleShape::Stretched;
            effect.burstCount = 16;
            effect.lifetime = {0.12f, 0.45f};
            effect.speed = {3.0f, 9.0f};
            effect.coneAngle = 1.0f;
            effect.acceleration = {0.0f, k_gravity, 0.0f};
            effect.drag = 1.5f;
            effect.size = {0.014f, 0.012f, 0.008f, 0.5f};
            effect.color = {{9.0f, 4.5f, 1.4f, 1.0f}, {6.0f, 2.4f, 0.5f, 1.0f}, {2.0f, 0.5f, 0.1f, 0.0f}, 0.4f};
            effect.stretch = 0.025f;
            effect.softness = 0.0f;

            return effect;
        }

        render::ParticleEffectDescription muzzleSmoke(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_smokeFrame, k_smokeFrames);
            effect.burstCount = 3;
            effect.lifetime = {1.2f, 2.4f};
            effect.speed = {0.1f, 0.5f};
            effect.coneAngle = 0.5f;
            effect.acceleration = {0.0f, 0.25f, 0.0f};
            effect.drag = 1.5f;
            effect.jitter = 0.3f;
            effect.size = {0.05f, 0.2f, 0.4f, 0.4f};
            effect.color = {{0.8f, 0.8f, 0.8f, 0.22f}, {0.8f, 0.8f, 0.8f, 0.12f}, {0.8f, 0.8f, 0.8f, 0.0f}, 0.3f};
            effect.spin = {0.2f, 0.8f};
            effect.softness = 0.1f;
            effect.scattering = 0.4f;

            return effect;
        }

        render::ParticleEffectDescription steam(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_smokeFrame, k_smokeFrames);
            effect.spawnRate = 9.0f;
            effect.lifetime = {1.5f, 3.0f};
            effect.speed = {0.6f, 1.1f};
            effect.coneAngle = 0.25f;
            effect.spawnExtent = {0.05f, 0.0f, 0.05f};
            effect.acceleration = {0.0f, 0.4f, 0.0f};
            effect.drag = 0.6f;
            effect.jitter = 0.4f;
            effect.size = {0.1f, 0.5f, 0.9f, 0.4f};
            effect.color = {{0.9f, 0.9f, 0.9f, 0.3f}, {0.9f, 0.9f, 0.9f, 0.16f}, {0.9f, 0.9f, 0.9f, 0.0f}, 0.25f};
            effect.spin = {0.1f, 0.5f};
            effect.softness = 0.2f;
            effect.scattering = 0.5f;

            return effect;
        }

        render::ParticleEffectDescription smoke(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_smokeFrame, k_smokeFrames);
            effect.spawnRate = 5.0f;
            effect.lifetime = {4.0f, 7.0f};
            effect.speed = {0.3f, 0.6f};
            effect.coneAngle = 0.3f;
            effect.spawnExtent = {0.15f, 0.0f, 0.15f};
            effect.acceleration = {0.15f, 0.2f, 0.0f};
            effect.drag = 0.4f;
            effect.jitter = 0.2f;
            effect.size = {0.3f, 1.2f, 2.2f, 0.4f};
            effect.color = {{0.25f, 0.24f, 0.23f, 0.35f}, {0.3f, 0.3f, 0.3f, 0.22f}, {0.35f, 0.35f, 0.35f, 0.0f}, 0.3f};
            effect.spin = {0.05f, 0.3f};
            effect.softness = 0.4f;

            return effect;
        }

        render::ParticleEffectDescription dustFall(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_dotFrame, 1);
            effect.spawnRate = 30.0f;
            effect.lifetime = {2.0f, 4.0f};
            effect.speed = {0.0f, 0.1f};
            effect.coneAngle = math::k_pi;
            effect.spawnExtent = {0.3f, 0.02f, 0.3f};
            effect.acceleration = {0.0f, -0.6f, 0.0f};
            effect.drag = 0.8f;
            effect.jitter = 0.1f;
            effect.size = {0.008f, 0.008f, 0.008f, 0.5f};
            effect.sizeScale = {0.6f, 1.5f};
            effect.color = {{0.7f, 0.66f, 0.6f, 0.0f}, {0.7f, 0.66f, 0.6f, 0.8f}, {0.7f, 0.66f, 0.6f, 0.0f}, 0.2f};
            effect.softness = 0.02f;
            effect.scattering = 0.5f;

            return effect;
        }

        render::ParticleEffectDescription stress(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_smokeFrame, k_smokeFrames);
            effect.burstCount = 1;
            effect.lifetime = {6.0f, 9.0f};
            effect.speed = {0.0f, 0.2f};
            effect.coneAngle = math::k_pi;
            effect.spawnExtent = {6.0f, 2.0f, 6.0f};
            effect.jitter = 0.2f;
            effect.size = {0.3f, 0.3f, 0.3f, 0.5f};
            effect.color = {{0.7f, 0.7f, 0.7f, 0.0f}, {0.7f, 0.7f, 0.7f, 0.15f}, {0.7f, 0.7f, 0.7f, 0.0f}, 0.5f};
            effect.spin = {0.1f, 0.5f};
            effect.softness = 0.2f;

            return effect;
        }

        render::ParticleEffectDescription rainStreaks(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_dotFrame, 1);
            effect.blend = render::ParticleBlend::Additive;
            effect.shape = render::ParticleShape::Stretched;
            effect.spawnRate = 5000.0f;
            effect.lifetime = {1.4f, 1.8f};
            effect.speed = {8.5f, 10.0f};
            effect.coneAngle = 0.035f;
            effect.spawnExtent = {9.0f, 1.0f, 9.0f};
            effect.size = {0.014f, 0.014f, 0.014f, 0.5f};
            effect.sizeScale = {0.7f, 1.3f};
            effect.color = {{0.9f, 0.95f, 1.0f, 0.0f}, {0.9f, 0.95f, 1.0f, 0.6f}, {0.9f, 0.95f, 1.0f, 0.6f}, 0.08f};
            effect.stretch = k_rainStretch;
            effect.softness = 0.05f;
            effect.scattering = 0.75f;
            effect.diesInExclusionBoxes = true;

            return effect;
        }

        render::ParticleEffectDescription rainSplash(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_dotFrame, 1);
            effect.blend = render::ParticleBlend::Additive;
            effect.burstCount = 5;
            effect.lifetime = {0.12f, 0.3f};
            effect.speed = {0.5f, 1.4f};
            effect.coneAngle = 0.7f;
            effect.acceleration = {0.0f, k_gravity, 0.0f};
            effect.size = {0.012f, 0.01f, 0.006f, 0.5f};
            effect.sizeScale = {0.6f, 1.4f};
            effect.color = {{0.6f, 0.65f, 0.7f, 0.7f}, {0.6f, 0.65f, 0.7f, 0.5f}, {0.6f, 0.65f, 0.7f, 0.0f}, 0.5f};
            effect.softness = 0.01f;
            effect.scattering = 0.6f;

            return effect;
        }
    } //namespace

    float rainStreakStretch()
    {
        return k_rainStretch;
    }

    lockstep::Result<EffectIds> createEffects(render::ParticleSystem& particles, render::TextureHandle atlas)
    {
        EffectIds ids{};
        const std::pair<render::ParticleEffectId*, render::ParticleEffectDescription> effects[]{
            {&ids.dustMotes, dustMotes(atlas)},
            {&ids.impactPuff, impactPuff(atlas)},
            {&ids.impactGrit, impactGrit(atlas)},
            {&ids.impactChunks, impactChunks(atlas)},
            {&ids.sparks, sparks(atlas)},
            {&ids.muzzleSmoke, muzzleSmoke(atlas)},
            {&ids.steam, steam(atlas)},
            {&ids.smoke, smoke(atlas)},
            {&ids.dustFall, dustFall(atlas)},
            {&ids.stress, stress(atlas)},
            {&ids.rainStreaks, rainStreaks(atlas)},
            {&ids.rainSplash, rainSplash(atlas)}};

        for (const auto& [id, description] : effects)
        {
            auto created = particles.createEffect(description);

            if (!created)
            {
                return created.error();
            }

            *id = created.value();
        }

        return ids;
    }
} //namespace game
