#include "Effects/MuzzleFlashes.hpp"

#include "Effects/EffectTextures.hpp"

#include <Lockstep/Render/Lighting.hpp>
#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;

        constexpr math::Vec3 k_flashColour{1.0f, 0.68f, 0.36f};
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_coreAhead{0.04f};
        constexpr float k_sideStretch{0.028f};
        constexpr float k_tracerLife{0.09f};
        constexpr float k_tracerStretch{0.03f};
        constexpr float k_minTracerDistance{1.0f};
        constexpr float k_fadedLight{0.5f};
        constexpr std::size_t k_maxLights{8};
        constexpr float k_smokeAhead{0.03f};

        uint32_t nextRandom(MuzzleFlashes& muzzle)
        {
            muzzle.random ^= muzzle.random << 13;
            muzzle.random ^= muzzle.random >> 17;
            muzzle.random ^= muzzle.random << 5;
            return muzzle.random;
        }

        float randomRange(MuzzleFlashes& muzzle, float low, float high)
        {
            return low + (high - low) * static_cast<float>(nextRandom(muzzle) % 10000) / 10000.0f;
        }

        render::ParticleEffectDescription atlasEffect(render::TextureHandle atlas, uint32_t firstFrame, uint32_t frameCount)
        {
            render::ParticleEffectDescription effect{};
            effect.texture = atlas;
            effect.atlasColumns = k_effectAtlasColumns;
            effect.atlasRows = k_effectAtlasRows;
            effect.firstFrame = firstFrame;
            effect.frameCount = frameCount;
            effect.blend = render::ParticleBlend::Additive;
            effect.lighting = render::ParticleLighting::Unlit;

            return effect;
        }

        render::ParticleCurve<math::Vec4> flameColour()
        {
            return {{16.0f, 10.0f, 5.5f, 1.0f}, {9.0f, 4.5f, 1.6f, 0.8f}, {3.0f, 1.0f, 0.25f, 0.0f}, 0.35f};
        }

        render::ParticleEffectDescription flashCore(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_flashFrame, k_flashFrames);
            effect.burstCount = 1;
            effect.lifetime = {0.02f, 0.035f};
            effect.size = {0.13f, 0.16f, 0.1f, 0.3f};
            effect.sizeScale = {0.7f, 1.35f};
            effect.color = flameColour();
            effect.softness = 0.02f;

            return effect;
        }

        render::ParticleEffectDescription flashSide(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_flashSideFrame, 1);
            effect.shape = render::ParticleShape::Stretched;
            effect.burstCount = 3;
            effect.lifetime = {0.02f, 0.035f};
            effect.speed = {5.0f, 9.0f};
            effect.coneAngle = 0.22f;
            effect.drag = 12.0f;
            effect.size = {0.06f, 0.07f, 0.04f, 0.4f};
            effect.sizeScale = {0.7f, 1.3f};
            effect.color = flameColour();
            effect.stretch = k_sideStretch;
            effect.softness = 0.02f;

            return effect;
        }

        render::ParticleEffectDescription flashSparks(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_sparkFrame, 1);
            effect.shape = render::ParticleShape::Stretched;
            effect.burstCount = 4;
            effect.lifetime = {0.05f, 0.16f};
            effect.speed = {5.0f, 14.0f};
            effect.coneAngle = 0.4f;
            effect.acceleration = {0.0f, -4.0f, 0.0f};
            effect.drag = 3.0f;
            effect.size = {0.006f, 0.005f, 0.003f, 0.5f};
            effect.color = {{10.0f, 5.0f, 1.4f, 1.0f}, {6.0f, 2.4f, 0.5f, 1.0f}, {2.0f, 0.5f, 0.1f, 0.0f}, 0.5f};
            effect.stretch = 0.012f;
            effect.softness = 0.0f;

            return effect;
        }

        render::ParticleEffectDescription tracer(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_sparkFrame, 1);
            effect.shape = render::ParticleShape::Stretched;
            effect.burstCount = 1;
            effect.lifetime = {k_tracerLife, k_tracerLife};
            effect.size = {0.03f, 0.03f, 0.025f, 0.5f};
            effect.color = {{14.0f, 7.0f, 2.2f, 1.0f}, {12.0f, 5.5f, 1.6f, 1.0f}, {7.0f, 2.5f, 0.6f, 0.5f}, 0.5f};
            effect.stretch = k_tracerStretch;
            effect.softness = 0.0f;

            return effect;
        }

        render::ParticleEffectDescription barrelSmoke(render::TextureHandle atlas)
        {
            render::ParticleEffectDescription effect = atlasEffect(atlas, k_smokeFrame, k_smokeFrames);
            effect.blend = render::ParticleBlend::Alpha;
            effect.lighting = render::ParticleLighting::Lit;
            effect.burstCount = 1;
            effect.lifetime = {1.4f, 2.8f};
            effect.speed = {0.02f, 0.1f};
            effect.coneAngle = 0.5f;
            effect.acceleration = {0.0f, 0.22f, 0.0f};
            effect.drag = 1.0f;
            effect.jitter = 0.12f;
            effect.size = {0.02f, 0.08f, 0.17f, 0.4f};
            effect.sizeScale = {0.7f, 1.3f};
            effect.color = {{0.82f, 0.82f, 0.82f, 0.0f}, {0.82f, 0.82f, 0.82f, 0.13f}, {0.82f, 0.82f, 0.82f, 0.0f}, 0.25f};
            effect.spin = {0.1f, 0.6f};
            effect.softness = 0.05f;
            effect.scattering = 0.45f;

            return effect;
        }
    } //namespace

    bool createMuzzleEffects(MuzzleFlashes& muzzle, render::ParticleSystem& particles, render::TextureHandle atlas)
    {
        if (muzzle.isTried)
        {
            return muzzle.barrelSmoke.isValid();
        }

        muzzle.isTried = true;
        const std::pair<render::ParticleEffectId*, render::ParticleEffectDescription> effects[]{
            {&muzzle.flashCore, flashCore(atlas)},
            {&muzzle.flashSide, flashSide(atlas)},
            {&muzzle.flashSparks, flashSparks(atlas)},
            {&muzzle.tracer, tracer(atlas)},
            {&muzzle.barrelSmoke, barrelSmoke(atlas)}};

        for (const auto& [id, description] : effects)
        {
            auto created = particles.createEffect(description);

            if (!created)
            {
                muzzle.barrelSmoke = render::ParticleEffectId{};
                return false;
            }

            *id = created.value();
        }

        return true;
    }

    void flashMuzzle(MuzzleFlashes& muzzle, render::ParticleSystem* particles, const WeaponSocket& socket,
        uint32_t shooter, render::ParticleEffectId smoke, bool isOwn, float strength, float size)
    {
        const MuzzleFlashTuning& tuning = muzzle.tuning;
        BarrelHeat& barrel = muzzle.barrels[shooter];
        barrel.heat = std::min(barrel.heat + tuning.heatPerShot, tuning.maxHeat);

        if (tuning.lights)
        {
            if (muzzle.lights.size() >= k_maxLights)
            {
                muzzle.lights.erase(muzzle.lights.begin());
            }

            muzzle.lights.push_back(MuzzleLight{
                .position = socket.muzzle + socket.forward * (isOwn ? tuning.ownLightAhead : k_coreAhead),
                .volumetric = isOwn ? tuning.ownLightVolumetric : tuning.lightVolumetric,
                .strength = randomRange(muzzle, 0.75f, 1.15f) * strength * (isOwn ? tuning.ownLightScale : 1.0f)});
        }

        if (particles == nullptr || !muzzle.barrelSmoke.isValid())
        {
            return;
        }

        if (tuning.flashes && !isOwn)
        {
            const float flashSize = randomRange(muzzle, 0.75f, 1.25f) * tuning.flashSize * size;
            const math::Vec4 tint{tuning.flashIntensity, tuning.flashIntensity, tuning.flashIntensity, 1.0f};
            static_cast<void>(particles->burst(muzzle.flashCore, render::ParticleBurst{
                .position = socket.muzzle + socket.forward * (k_coreAhead * flashSize),
                .direction = socket.forward,
                .sizeScale = flashSize,
                .tint = tint}));
            static_cast<void>(particles->burst(muzzle.flashSide, render::ParticleBurst{
                .position = socket.muzzle + socket.forward * (k_sideStretch * 4.0f * tuning.flashSize),
                .direction = socket.forward,
                .countScale = randomRange(muzzle, 0.6f, 1.4f),
                .sizeScale = flashSize,
                .tint = tint}));

            if (nextRandom(muzzle) % 3 == 0)
            {
                static_cast<void>(particles->burst(muzzle.flashSparks, render::ParticleBurst{.position = socket.muzzle,
                    .direction = socket.forward,
                    .tint = tint}));
            }
        }

        if (smoke.isValid())
        {
            const float heat = barrel.heat / std::max(tuning.maxHeat, 1.0f);
            const float own = isOwn ? tuning.ownSmoke : 1.0f;
            static_cast<void>(particles->burst(smoke, render::ParticleBurst{
                .position = socket.muzzle + socket.forward * k_smokeAhead,
                .direction = math::normalize(socket.forward + k_up * 0.3f),
                .countScale = (0.5f + 2.5f * heat) * own,
                .sizeScale = (0.8f + 0.8f * heat) * (isOwn ? 0.5f : 1.0f),
                .tint = {1.0f, 1.0f, 1.0f, isOwn ? 0.4f : 1.0f},
                .velocity = isOwn ? socket.forward * 0.8f : math::Vec3{0.0f, 0.0f, 0.0f}}));
        }
    }

    void fireTracer(MuzzleFlashes& muzzle, render::ParticleSystem& particles, const math::Vec3& from,
        const math::Vec3& to)
    {
        const math::Vec3 path = to - from;
        const float distance = math::length(path);

        if (!muzzle.tuning.tracers || !muzzle.tracer.isValid() || distance < k_minTracerDistance)
        {
            return;
        }

        const math::Vec3 direction = path / distance;
        const float speed = distance / (k_tracerLife + k_tracerStretch);
        static_cast<void>(particles.burst(muzzle.tracer, render::ParticleBurst{
            .position = from + direction * (speed * k_tracerStretch),
            .direction = direction,
            .velocity = direction * speed}));
    }

    void smokeBarrel(MuzzleFlashes& muzzle, render::ParticleSystem& particles, uint32_t shooter,
        const WeaponSocket& socket, bool isOwn, float seconds)
    {
        const auto found = muzzle.barrels.find(shooter);

        if (found == muzzle.barrels.end() || !muzzle.barrelSmoke.isValid())
        {
            return;
        }

        BarrelHeat& barrel = found->second;
        const float over = barrel.heat - muzzle.tuning.smokeHeat;

        if (over <= 0.0f)
        {
            barrel.carry = 0.0f;
            return;
        }

        barrel.carry += over * muzzle.tuning.smokeRate * seconds * (isOwn ? muzzle.tuning.ownSmoke : 1.0f);

        while (barrel.carry >= 1.0f)
        {
            barrel.carry -= 1.0f;
            static_cast<void>(particles.burst(muzzle.barrelSmoke, render::ParticleBurst{
                .position = socket.muzzle - socket.forward * randomRange(muzzle, 0.0f, 0.08f),
                .direction = k_up,
                .sizeScale = isOwn ? 0.35f : 1.0f,
                .tint = {1.0f, 1.0f, 1.0f, isOwn ? 0.3f : 1.0f}}));
        }
    }

    void updateMuzzleFlashes(MuzzleFlashes& muzzle, render::Renderer& renderer, float seconds)
    {
        const MuzzleFlashTuning& tuning = muzzle.tuning;

        for (auto barrel = muzzle.barrels.begin(); barrel != muzzle.barrels.end();)
        {
            barrel->second.heat -= barrel->second.heat * std::min(tuning.heatDecay * seconds, 1.0f);
            barrel = barrel->second.heat < 0.05f ? muzzle.barrels.erase(barrel) : std::next(barrel);
        }

        for (MuzzleLight& light : muzzle.lights)
        {
            const float faded = light.frames == 0 ? 1.0f : k_fadedLight;
            renderer.submitLight(render::PointLight{.position = light.position,
                .color = k_flashColour,
                .intensity = tuning.lightIntensity * light.strength * faded,
                .range = tuning.lightRange,
                .castsShadows = false,
                .volumetric = light.volumetric});
            ++light.frames;
            light.seconds += seconds;
        }

        std::erase_if(muzzle.lights, [&tuning](const MuzzleLight& light)
        {
            return light.frames >= tuning.lightFrames && light.seconds >= tuning.lightSeconds;
        });
    }
} //namespace game
