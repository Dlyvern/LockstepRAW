#include "Lockstep/Render/Particles.hpp"

#include "FrameParticles.hpp"
#include "RendererState.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>
#include <vector>

namespace lockstep::render
{
    namespace
    {
        struct Particle
        {
            math::Vec3 position{0.0f, 0.0f, 0.0f};
            float age{0.0f};
            math::Vec3 velocity{0.0f, 0.0f, 0.0f};
            float lifetime{1.0f};
            math::Vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
            float sizeScale{1.0f};
            float rotation{0.0f};
            float spin{0.0f};
            uint32_t frame{0};
            uint32_t effect{0};
        };

        struct Emitter
        {
            uint32_t effect{0};
            math::Transform transform{};
            float rate{1.0f};
            float carry{0.0f};
            uint32_t generation{0};
            bool isAlive{false};
        };

        struct SpawnSource
        {
            math::Vec3 position{0.0f, 0.0f, 0.0f};
            math::Quat rotation{};
            math::Vec3 extentScale{1.0f, 1.0f, 1.0f};
            math::Vec3 axis{0.0f, 1.0f, 0.0f};
            math::Vec3 velocity{0.0f, 0.0f, 0.0f};
            math::Vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
            float sizeScale{1.0f};
        };

        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_minAxisLength{1e-6f};

        float elapsedMilliseconds(std::chrono::steady_clock::time_point start)
        {
            return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
        }

        template<typename T>
        T evaluate(const ParticleCurve<T>& curve, float t)
        {
            const float middle = std::clamp(curve.middleTime, 1e-4f, 1.0f - 1e-4f);

            if (t < middle)
            {
                return curve.start + (curve.middle - curve.start) * (t / middle);
            }

            return curve.middle + (curve.end - curve.middle) * ((t - middle) / (1.0f - middle));
        }

        Status validate(const ParticleEffectDescription& description)
        {
            if (description.atlasColumns == 0 || description.atlasRows == 0 || description.frameCount == 0
                || description.firstFrame + description.frameCount > description.atlasColumns * description.atlasRows)
            {
                return Error{ErrorCode::InvalidArgument, "Particle effect: atlas frame range outside the atlas grid"};
            }

            if (!(description.lifetime.x > 0.0f) || description.lifetime.y < description.lifetime.x)
            {
                return Error{ErrorCode::InvalidArgument, "Particle effect: lifetime must be positive and ordered"};
            }

            if (description.size.start < 0.0f || description.size.middle < 0.0f || description.size.end < 0.0f
                || description.sizeScale.x < 0.0f || description.sizeScale.y < description.sizeScale.x)
            {
                return Error{ErrorCode::InvalidArgument, "Particle effect: sizes must not be negative"};
            }

            return {};
        }

        math::Vec3 safeNormalize(const math::Vec3& vector, const math::Vec3& fallback)
        {
            const float length = math::length(vector);

            return length > k_minAxisLength ? vector * (1.0f / length) : fallback;
        }
    } //namespace

    struct ParticleSystemState
    {
        std::vector<ParticleEffectDescription> effects;
        std::vector<Particle> particles;
        std::vector<Emitter> emitters;
        std::vector<uint32_t> effectCounts;
        std::vector<math::Aabb> exclusionBoxes;
        uint32_t maxParticles{0};
        uint32_t random{0x9E3779B9u};
        ParticleStats stats{};

        uint32_t nextRandom()
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            return random;
        }

        float unit()
        {
            return static_cast<float>(nextRandom() >> 8) * (1.0f / 16777216.0f);
        }

        float range(const math::Vec2& bounds)
        {
            return bounds.x + (bounds.y - bounds.x) * unit();
        }

        float signedUnit()
        {
            return unit() * 2.0f - 1.0f;
        }

        math::Vec3 coneDirection(const math::Vec3& axis, float halfAngle)
        {
            const float cosine = 1.0f - unit() * (1.0f - std::cos(std::clamp(halfAngle, 0.0f, math::k_pi)));
            const float sine = std::sqrt(std::max(1.0f - cosine * cosine, 0.0f));
            const float angle = unit() * 2.0f * math::k_pi;
            const math::Vec3 helper = std::abs(axis.y) < 0.99f ? k_up : math::Vec3{1.0f, 0.0f, 0.0f};
            const math::Vec3 tangent = math::normalize(math::cross(helper, axis));
            const math::Vec3 bitangent = math::cross(axis, tangent);

            return axis * cosine + (tangent * std::cos(angle) + bitangent * std::sin(angle)) * sine;
        }

        uint32_t spawn(uint32_t effectIndex, const SpawnSource& source, uint32_t count)
        {
            const ParticleEffectDescription& effect = effects[effectIndex];
            const auto room = maxParticles - std::min(maxParticles, static_cast<uint32_t>(particles.size()));
            const uint32_t spawned = std::min(count, room);
            stats.dropped += count - spawned;

            for (uint32_t index = 0; index < spawned; ++index)
            {
                const math::Vec3 local{effect.spawnExtent.x * source.extentScale.x * signedUnit(),
                    effect.spawnExtent.y * source.extentScale.y * signedUnit(),
                    effect.spawnExtent.z * source.extentScale.z * signedUnit()};
                const float spinSign = (nextRandom() & 1u) != 0 ? 1.0f : -1.0f;

                particles.push_back(Particle{
                    .position = source.position + source.rotation * local,
                    .age = 0.0f,
                    .velocity = coneDirection(source.axis, effect.coneAngle) * range(effect.speed) + source.velocity,
                    .lifetime = range(effect.lifetime),
                    .tint = source.tint,
                    .sizeScale = range(effect.sizeScale) * source.sizeScale,
                    .rotation = effect.shape == ParticleShape::Billboard ? unit() * 2.0f * math::k_pi : 0.0f,
                    .spin = range(effect.spin) * spinSign,
                    .frame = effect.isFlipbook ? 0u : nextRandom() % effect.frameCount,
                    .effect = effectIndex});
            }

            return spawned;
        }

        uint32_t roundRandomly(float count)
        {
            const float whole = std::floor(std::max(count, 0.0f));

            return static_cast<uint32_t>(whole) + (unit() < count - whole ? 1u : 0u);
        }

        Emitter* findEmitter(ParticleEmitterId id)
        {
            if (id.index >= emitters.size() || !emitters[id.index].isAlive
                || emitters[id.index].generation != id.generation)
            {
                return nullptr;
            }

            return &emitters[id.index];
        }

        void spawnFromEmitters(float seconds)
        {
            for (Emitter& emitter : emitters)
            {
                if (!emitter.isAlive)
                {
                    continue;
                }

                const ParticleEffectDescription& effect = effects[emitter.effect];
                emitter.carry += effect.spawnRate * emitter.rate * seconds;
                const auto count = static_cast<uint32_t>(std::floor(emitter.carry));
                emitter.carry -= static_cast<float>(count);

                if (count == 0)
                {
                    continue;
                }

                spawn(emitter.effect, SpawnSource{.position = emitter.transform.position,
                    .rotation = emitter.transform.rotation,
                    .extentScale = emitter.transform.scale,
                    .axis = safeNormalize(emitter.transform.rotation * k_up, k_up)}, count);
            }
        }

        bool isExcluded(const math::Vec3& point) const
        {
            return std::any_of(exclusionBoxes.begin(), exclusionBoxes.end(), [&point](const math::Aabb& box)
            {
                return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y && point.y <= box.max.y
                    && point.z >= box.min.z && point.z <= box.max.z;
            });
        }

        void advance(float seconds)
        {
            std::size_t index = 0;

            while (index < particles.size())
            {
                Particle& particle = particles[index];
                particle.age += seconds;

                if (particle.age >= particle.lifetime)
                {
                    particle = particles.back();
                    particles.pop_back();
                    continue;
                }

                const ParticleEffectDescription& effect = effects[particle.effect];
                math::Vec3 acceleration = effect.acceleration;

                if (effect.jitter > 0.0f)
                {
                    acceleration += math::Vec3{signedUnit(), signedUnit(), signedUnit()} * effect.jitter;
                }

                particle.velocity += acceleration * seconds;

                if (effect.drag > 0.0f)
                {
                    particle.velocity *= std::exp(-effect.drag * seconds);
                }

                particle.position += particle.velocity * seconds;
                particle.rotation += particle.spin * seconds;

                if (effect.diesInExclusionBoxes && isExcluded(particle.position))
                {
                    particle = particles.back();
                    particles.pop_back();
                    continue;
                }

                ++index;
            }
        }

        GpuParticle gpuParticle(const Particle& particle) const
        {
            const ParticleEffectDescription& effect = effects[particle.effect];
            const float t = std::clamp(particle.age / particle.lifetime, 0.0f, 1.0f);
            const float size = evaluate(effect.size, t) * particle.sizeScale;
            const math::Vec4 color = evaluate(effect.color, t) * particle.tint;
            const float frame = effect.isFlipbook
                ? static_cast<float>(effect.firstFrame) + t * static_cast<float>(effect.frameCount - 1)
                : static_cast<float>(effect.firstFrame + particle.frame);
            const uint32_t flags = (effect.lighting == ParticleLighting::Lit ? k_particleLit : 0u)
                | (effect.blend == ParticleBlend::Additive ? k_particleAdditive : 0u);
            const float stretch = effect.shape == ParticleShape::Stretched ? effect.stretch : 0.0f;

            return GpuParticle{
                .positionSize = {particle.position.x, particle.position.y, particle.position.z, size},
                .velocityStretch = {particle.velocity.x, particle.velocity.y, particle.velocity.z, stretch},
                .color = {color.x, color.y, color.z, std::clamp(color.w, 0.0f, 1.0f)},
                .params = {particle.rotation, frame, effect.softness, static_cast<float>(flags)},
                .atlas = {static_cast<float>(effect.atlasColumns), static_cast<float>(effect.atlasRows),
                    std::clamp(effect.scattering, 0.0f, 0.95f), 0.0f}};
        }
    };

    Result<ParticleSystem> ParticleSystem::create(const ParticleSystemDescription& description)
    {
        if (description.maxParticles == 0 || description.maxParticles > k_maxParticleBudget)
        {
            return Error{ErrorCode::InvalidArgument, "Particle budget must be 1 .. k_maxParticleBudget"};
        }

        auto state = std::make_unique<ParticleSystemState>();
        state->maxParticles = description.maxParticles;
        state->particles.reserve(description.maxParticles);

        return ParticleSystem(std::move(state));
    }

    ParticleSystem::ParticleSystem(std::unique_ptr<ParticleSystemState> state)
        : m_state(std::move(state))
    {
    }

    ParticleSystem::~ParticleSystem() = default;

    ParticleSystem::ParticleSystem(ParticleSystem&& other) noexcept = default;

    ParticleSystem& ParticleSystem::operator=(ParticleSystem&& other) noexcept = default;

    Result<ParticleEffectId> ParticleSystem::createEffect(const ParticleEffectDescription& description)
    {
        if (auto valid = validate(description); !valid)
        {
            return valid.error();
        }

        m_state->effects.push_back(description);

        return ParticleEffectId{.index = static_cast<uint32_t>(m_state->effects.size() - 1)};
    }

    Status ParticleSystem::setEffect(ParticleEffectId effect, const ParticleEffectDescription& description)
    {
        if (effect.index >= m_state->effects.size())
        {
            return Error{ErrorCode::InvalidHandle, "Unknown particle effect"};
        }

        if (auto valid = validate(description); !valid)
        {
            return valid;
        }

        ParticleEffectDescription& stored = m_state->effects[effect.index];
        const uint32_t oldFrameCount = stored.frameCount;
        stored = description;

        if (description.frameCount < oldFrameCount)
        {
            for (Particle& particle : m_state->particles)
            {
                if (particle.effect == effect.index)
                {
                    particle.frame %= description.frameCount;
                }
            }
        }

        return {};
    }

    const ParticleEffectDescription* ParticleSystem::findEffect(ParticleEffectId effect) const
    {
        return effect.index < m_state->effects.size() ? &m_state->effects[effect.index] : nullptr;
    }

    Result<ParticleEmitterId> ParticleSystem::createEmitter(ParticleEffectId effect, const math::Transform& transform)
    {
        if (effect.index >= m_state->effects.size())
        {
            return Error{ErrorCode::InvalidHandle, "Unknown particle effect"};
        }

        auto free = std::find_if(m_state->emitters.begin(), m_state->emitters.end(),
            [](const Emitter& emitter) { return !emitter.isAlive; });

        if (free == m_state->emitters.end())
        {
            m_state->emitters.push_back(Emitter{});
            free = m_state->emitters.end() - 1;
        }

        free->effect = effect.index;
        free->transform = transform;
        free->rate = 1.0f;
        free->carry = 0.0f;
        free->isAlive = true;
        ++free->generation;
        ++m_state->stats.emitters;

        return ParticleEmitterId{.index = static_cast<uint32_t>(free - m_state->emitters.begin()),
            .generation = free->generation};
    }

    void ParticleSystem::setEmitterTransform(ParticleEmitterId emitter, const math::Transform& transform)
    {
        if (Emitter* found = m_state->findEmitter(emitter))
        {
            found->transform = transform;
        }
    }

    void ParticleSystem::setEmitterRate(ParticleEmitterId emitter, float scale)
    {
        if (Emitter* found = m_state->findEmitter(emitter))
        {
            found->rate = std::max(scale, 0.0f);
        }
    }

    bool ParticleSystem::destroyEmitter(ParticleEmitterId emitter)
    {
        Emitter* found = m_state->findEmitter(emitter);

        if (found == nullptr)
        {
            return false;
        }

        found->isAlive = false;
        --m_state->stats.emitters;

        return true;
    }

    uint32_t ParticleSystem::burst(ParticleEffectId effect, const ParticleBurst& burst)
    {
        if (effect.index >= m_state->effects.size())
        {
            return 0;
        }

        const ParticleEffectDescription& description = m_state->effects[effect.index];
        const uint32_t count = m_state->roundRandomly(static_cast<float>(description.burstCount) * burst.countScale);
        const math::Vec3 axis = safeNormalize(burst.direction, k_up);

        return m_state->spawn(effect.index, SpawnSource{.position = burst.position,
            .axis = axis,
            .velocity = burst.velocity,
            .tint = burst.tint,
            .sizeScale = burst.sizeScale}, count);
    }

    void ParticleSystem::update(float seconds)
    {
        const auto start = std::chrono::steady_clock::now();
        const float step = std::max(seconds, 0.0f);

        m_state->spawnFromEmitters(step);
        m_state->advance(step);
        m_state->stats.alive = static_cast<uint32_t>(m_state->particles.size());
        m_state->stats.updateMilliseconds = elapsedMilliseconds(start);
    }

    void ParticleSystem::setExclusionBoxes(std::span<const math::Aabb> boxes)
    {
        m_state->exclusionBoxes.assign(boxes.begin(), boxes.end());
    }

    void ParticleSystem::clear()
    {
        m_state->particles.clear();
        m_state->emitters.clear();
        m_state->stats.alive = 0;
        m_state->stats.emitters = 0;
    }

    void ParticleSystem::submit(Renderer& renderer)
    {
        const auto start = std::chrono::steady_clock::now();
        RendererState& rendererData = rendererState(renderer);
        FrameParticles& frame = rendererData.particles;
        ParticleSystemState& state = *m_state;

        frame.particles.resize(state.particles.size());
        frame.batches.clear();
        frame.frameIndex = rendererData.frameIndex;
        state.effectCounts.assign(state.effects.size(), 0);

        for (const Particle& particle : state.particles)
        {
            ++state.effectCounts[particle.effect];
        }

        uint32_t first = 0;

        for (uint32_t effectIndex = 0; effectIndex < state.effects.size(); ++effectIndex)
        {
            const uint32_t count = state.effectCounts[effectIndex];
            const ParticleEffectDescription& effect = state.effects[effectIndex];

            if (count > 0)
            {
                frame.batches.push_back(ParticleBatch{.texture = effect.texture,
                    .isAdditive = effect.blend == ParticleBlend::Additive,
                    .first = first,
                    .count = count});
            }

            state.effectCounts[effectIndex] = first;
            first += count;
        }

        for (const Particle& particle : state.particles)
        {
            frame.particles[state.effectCounts[particle.effect]++] = state.gpuParticle(particle);
        }

        state.stats.submitMilliseconds = elapsedMilliseconds(start);
    }

    ParticleStats ParticleSystem::stats() const
    {
        return m_state->stats;
    }
} //namespace lockstep::render
