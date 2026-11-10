#include "Gore/BloodBursts.hpp"

#include "Effects/EffectTextures.hpp"

#include <algorithm>
#include <iostream>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

namespace game
{
    namespace
    {
        namespace render = lockstep::render;

        constexpr math::Vec3 k_gravity{0.0f, -9.81f, 0.0f};
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_minStrength{0.3f};
        constexpr float k_maxStrength{2.0f};
        constexpr float k_corpseStrength{0.6f};
        constexpr float k_entryLift{0.15f};
        constexpr float k_entryDroplets{0.6f};
        constexpr float k_entryMist{0.7f};
        constexpr float k_exitDroplets{1.4f};
        constexpr float k_exitMist{1.2f};
        constexpr float k_exitPush{2.5f};
        constexpr float k_headshotFragments{1.0f};
        constexpr math::Vec4 k_dropletTint{1.0f, 1.0f, 1.0f, 1.0f};

        render::ParticleEffectDescription dropletDescription(render::TextureHandle atlas)
        {
            return render::ParticleEffectDescription{.texture = atlas,
                .atlasColumns = k_effectAtlasColumns,
                .atlasRows = k_effectAtlasRows,
                .firstFrame = k_dotFrame,
                .frameCount = 1,
                .shape = render::ParticleShape::Stretched,
                .burstCount = 36,
                .lifetime = {0.35f, 0.8f},
                .speed = {1.5f, 5.5f},
                .coneAngle = 0.55f,
                .spawnExtent = {0.03f, 0.03f, 0.03f},
                .acceleration = k_gravity,
                .drag = 0.8f,
                .size = {0.014f, 0.011f, 0.007f, 0.5f},
                .sizeScale = {0.6f, 1.7f},
                .color = {{0.30f, 0.012f, 0.012f, 1.0f}, {0.24f, 0.01f, 0.01f, 0.95f}, {0.18f, 0.008f, 0.008f, 0.0f}, 0.7f},
                .stretch = 0.035f,
                .softness = 0.02f};
        }

        render::ParticleEffectDescription mistDescription(render::TextureHandle atlas)
        {
            return render::ParticleEffectDescription{.texture = atlas,
                .atlasColumns = k_effectAtlasColumns,
                .atlasRows = k_effectAtlasRows,
                .firstFrame = k_smokeFrame,
                .frameCount = k_smokeFrames,
                .burstCount = 5,
                .lifetime = {0.22f, 0.45f},
                .speed = {0.4f, 1.6f},
                .coneAngle = 0.9f,
                .spawnExtent = {0.04f, 0.04f, 0.04f},
                .acceleration = {0.0f, -0.6f, 0.0f},
                .drag = 4.0f,
                .size = {0.1f, 0.32f, 0.48f, 0.35f},
                .sizeScale = {0.7f, 1.3f},
                .color = {{0.42f, 0.03f, 0.03f, 0.6f}, {0.36f, 0.025f, 0.025f, 0.35f}, {0.3f, 0.02f, 0.02f, 0.0f}, 0.3f},
                .spin = {-2.0f, 2.0f},
                .softness = 0.15f};
        }

        render::ParticleEffectDescription fragmentDescription(render::TextureHandle atlas)
        {
            return render::ParticleEffectDescription{.texture = atlas,
                .atlasColumns = k_effectAtlasColumns,
                .atlasRows = k_effectAtlasRows,
                .firstFrame = k_chunkFrame,
                .frameCount = k_chunkFrames,
                .burstCount = 10,
                .lifetime = {0.5f, 1.1f},
                .speed = {2.0f, 5.0f},
                .coneAngle = 0.7f,
                .acceleration = k_gravity,
                .drag = 0.3f,
                .size = {0.022f, 0.02f, 0.016f, 0.5f},
                .sizeScale = {0.6f, 1.5f},
                .color = {{0.32f, 0.05f, 0.045f, 1.0f}, {0.28f, 0.04f, 0.035f, 1.0f}, {0.22f, 0.03f, 0.03f, 0.0f}, 0.8f},
                .spin = {-15.0f, 15.0f},
                .softness = 0.01f};
        }

        bool registerEffects(BloodBurstEffects& effects, render::ParticleSystem& particles, render::TextureHandle atlas)
        {
            effects.isTried = true;
            auto droplets = particles.createEffect(dropletDescription(atlas));
            auto mist = particles.createEffect(mistDescription(atlas));
            auto fragments = particles.createEffect(fragmentDescription(atlas));

            if (!droplets || !mist || !fragments)
            {
                const lockstep::Error& error = !droplets ? droplets.error() : !mist ? mist.error() : fragments.error();
                std::clog << "Game: blood particles rejected: " << error.message << '\n';
                return false;
            }

            effects.droplets = droplets.value();
            effects.mist = mist.value();
            effects.fragments = fragments.value();
            effects.isReady = true;
            return true;
        }

        void spawn(render::ParticleSystem& particles, render::ParticleEffectId effect, const math::Vec3& point,
            const math::Vec3& direction, float count, float size, const math::Vec3& velocity)
        {
            if (count <= 0.0f)
            {
                return;
            }

            static_cast<void>(particles.burst(effect, render::ParticleBurst{.position = point,
                .direction = direction,
                .countScale = count,
                .sizeScale = size,
                .tint = k_dropletTint,
                .velocity = velocity}));
        }
    } //namespace

    float bloodStrength(const BloodBurstTuning& tuning, int32_t damage)
    {
        const float hit = damage > 0 ? static_cast<float>(damage) : tuning.referenceDamage;
        return std::clamp(hit / std::max(tuning.referenceDamage, 1.0f), k_minStrength, k_maxStrength);
    }

    void spawnBloodBurst(BloodBurstEffects& effects, render::ParticleSystem* particles, render::TextureHandle atlas,
        const BloodBurstTuning& tuning, const BloodBurstHit& hit)
    {
        if (particles == nullptr || !tuning.isEnabled || !atlas.isValid())
        {
            return;
        }

        if (!effects.isTried && !registerEffects(effects, *particles, atlas))
        {
            return;
        }

        if (!effects.isReady)
        {
            return;
        }

        const float strength = bloodStrength(tuning, hit.damage) * (hit.isCorpse ? k_corpseStrength : 1.0f);
        const float amount = tuning.amount * strength;
        const float mistSize = tuning.size * (0.7f + 0.3f * strength) * (hit.isHeadshot ? tuning.headshotScale : 1.0f);
        const float mistCount = hit.isHeadshot ? tuning.headshotScale : 1.0f;
        const math::Vec3 back = math::normalize(-hit.direction + k_up * k_entryLift);
        const math::Vec3 exit = hit.point + hit.direction * tuning.bodyDepth;
        const float exitAmount = amount * tuning.exitShare;

        spawn(*particles, effects.droplets, hit.point, back, amount * k_entryDroplets, tuning.size, {});
        spawn(*particles, effects.mist, hit.point, back, mistCount * k_entryMist * std::min(amount, 1.5f), mistSize, {});
        spawn(*particles, effects.droplets, exit, hit.direction, exitAmount * k_exitDroplets, tuning.size,
            hit.direction * (k_exitPush * strength));
        spawn(*particles, effects.mist, exit, hit.direction, mistCount * k_exitMist * std::min(exitAmount, 1.5f),
            mistSize * 1.2f, hit.direction * strength);

        if (hit.isHeadshot)
        {
            spawn(*particles, effects.fragments, exit, hit.direction, k_headshotFragments * exitAmount, tuning.size,
                hit.direction * strength);
        }
    }

#if LOCKSTEP_DEV_TOOLS
    void bloodBurstDevSection(BloodBurstTuning& tuning)
    {
        if (!ImGui::TreeNode("Blood bursts"))
        {
            return;
        }

        ImGui::Checkbox("Enabled##bursts", &tuning.isEnabled);
        ImGui::SliderFloat("Amount", &tuning.amount, 0.0f, 4.0f);
        ImGui::SliderFloat("Size", &tuning.size, 0.2f, 3.0f);
        ImGui::SliderFloat("Exit spray", &tuning.exitShare, 0.0f, 3.0f);
        ImGui::SliderFloat("Headshot mist", &tuning.headshotScale, 1.0f, 4.0f);
        ImGui::SliderFloat("Reference damage##bursts", &tuning.referenceDamage, 1.0f, 150.0f);
        ImGui::SliderFloat("Body depth (m)", &tuning.bodyDepth, 0.0f, 0.6f);
        ImGui::TreePop();
    }
#endif
} //namespace game
