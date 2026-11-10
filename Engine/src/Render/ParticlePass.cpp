#include "ParticlePass.hpp"

#include "Lockstep/Core/Log.hpp"

#include "FrameAttachments.hpp"
#include "FramePasses.hpp"
#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <Vela/Graphics/VertexLayout.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_particleVertexShader{"particle.vert.spv"};
        constexpr std::string_view k_particleFragmentShader{"particle.frag.spv"};
        constexpr uint32_t k_lightingSlot{0};
        constexpr uint32_t k_depthSlot{0};
        constexpr uint32_t k_shadowAtlasSlot{1};
        constexpr uint32_t k_environmentSlot{2};
        constexpr uint32_t k_spriteSlot{3};
        constexpr uint32_t k_verticesPerParticle{6};
        constexpr uint32_t k_particleIndexBits{20};
        constexpr uint64_t k_particleIndexMask{(1u << k_particleIndexBits) - 1u};
        constexpr uint32_t k_maxSortedBatches{1u << (32 - k_particleIndexBits)};
        constexpr std::size_t k_initialInstanceBytes{sizeof(GpuParticle) * 1024};

        vela::graphics::VertexLayout particleLayout()
        {
            using vela::graphics::VertexAttributeFormat;

            return vela::graphics::VertexLayout{
                .stride = sizeof(GpuParticle),
                .attributes = {
                    {.location = 0, .format = VertexAttributeFormat::Float4, .offset = offsetof(GpuParticle, positionSize)},
                    {.location = 1, .format = VertexAttributeFormat::Float4, .offset = offsetof(GpuParticle, velocityStretch)},
                    {.location = 2, .format = VertexAttributeFormat::Float4, .offset = offsetof(GpuParticle, color)},
                    {.location = 3, .format = VertexAttributeFormat::Float4, .offset = offsetof(GpuParticle, params)},
                    {.location = 4, .format = VertexAttributeFormat::Float4, .offset = offsetof(GpuParticle, atlas)}},
                .inputRate = vela::graphics::VertexInputRate::Instance};
        }

        Result<vela::graphics::Material> createParticleMaterial(RendererState& state, vela::graphics::BlendMode blend)
        {
            vela::graphics::RenderState renderState{};
            renderState.depthTest = false;
            renderState.depthWrite = false;
            renderState.blend = blend;

            auto material = createBuiltinMaterial(state.context, k_particleVertexShader, k_particleFragmentShader,
                particleLayout(), renderState);

            if (!material)
            {
                return material.error();
            }

            if (auto bound = material.value().setStorageBuffer(k_lightingSlot, state.lightBuffer); !bound)
            {
                return toError(bound.error());
            }

            return std::move(material).value();
        }

        vela::graphics::AttachmentSlot loadedSlot(std::string_view name)
        {
            vela::graphics::AttachmentSlot slot{};
            slot.name = std::string(name);
            slot.format = k_sceneColorFormat;
            slot.load = vela::graphics::LoadOp::Load;
            slot.store = vela::graphics::StoreOp::Store;

            return slot;
        }

        uint32_t descendingDepthKey(float depth)
        {
            const auto bits = std::bit_cast<uint32_t>(depth);
            const uint32_t ascending = (bits & 0x80000000u) != 0 ? ~bits : bits | 0x80000000u;

            return ~ascending;
        }
    } //namespace

    Result<std::unique_ptr<ParticlePass>> ParticlePass::create(RendererState& state)
    {
        auto alphaMaterial = createParticleMaterial(state, vela::graphics::BlendMode::Alpha);

        if (!alphaMaterial)
        {
            return alphaMaterial.error();
        }

        auto additiveMaterial = createParticleMaterial(state, vela::graphics::BlendMode::Additive);

        if (!additiveMaterial)
        {
            return additiveMaterial.error();
        }

        auto instances = vela::graphics::DynamicBuffer::create(state.context, vela::graphics::DynamicBuffer::Usage::Vertex,
            k_initialInstanceBytes);

        if (!instances)
        {
            return toError(instances.error());
        }

        return std::unique_ptr<ParticlePass>(new ParticlePass(state, std::move(alphaMaterial).value(),
            std::move(additiveMaterial).value(), std::move(instances).value()));
    }

    ParticlePass::ParticlePass(const RendererState& state, vela::graphics::Material&& alphaMaterial,
        vela::graphics::Material&& additiveMaterial, vela::graphics::DynamicBuffer&& instances)
        : m_state(&state),
          m_alphaMaterial(std::move(alphaMaterial)),
          m_additiveMaterial(std::move(additiveMaterial)),
          m_instances(std::move(instances))
    {
    }

    vela::graphics::PassDescription ParticlePass::describe() const
    {
        vela::graphics::PassDescription description{};
        description.colorOutputs.push_back(loadedSlot(k_sceneColorAttachment));
        description.colorOutputs.push_back(loadedSlot(k_sceneAmbientAttachment));
        description.inputs.push_back({std::string(k_sceneDepthAttachment), vela::graphics::InputUsage::Sampled});
        description.inputs.push_back({std::string(k_shadowMapAttachment), vela::graphics::InputUsage::Sampled});
        description.inputs.push_back({std::string(k_environmentAttachment), vela::graphics::InputUsage::Sampled});

        return description;
    }

    void ParticlePass::buildDrawList()
    {
        const FrameParticles& frame = m_state->particles;
        const math::Vec3 eye = m_state->camera.position;
        const math::Vec3 forward = math::normalize(m_state->camera.target - m_state->camera.position);

        m_upload.clear();
        m_runs.clear();
        m_sortKeys.clear();

        for (uint32_t batchIndex = 0; batchIndex < frame.batches.size(); ++batchIndex)
        {
            const ParticleBatch& batch = frame.batches[batchIndex];

            if (batch.isAdditive || batchIndex >= k_maxSortedBatches || m_state->textures.find(batch.texture) == nullptr)
            {
                continue;
            }

            for (uint32_t index = batch.first; index < batch.first + batch.count; ++index)
            {
                const float* position = frame.particles[index].positionSize;
                const float depth = math::dot(math::Vec3{position[0], position[1], position[2]} - eye, forward);
                m_sortKeys.push_back((static_cast<uint64_t>(descendingDepthKey(depth)) << 32)
                    | (static_cast<uint64_t>(batchIndex) << k_particleIndexBits) | index);
            }
        }

        std::sort(m_sortKeys.begin(), m_sortKeys.end());

        auto appendRun = [this](TextureHandle texture, bool isAdditive)
        {
            const auto first = static_cast<uint32_t>(m_upload.size());

            if (!m_runs.empty() && m_runs.back().texture == texture && m_runs.back().isAdditive == isAdditive)
            {
                return;
            }

            m_runs.push_back(DrawRun{.texture = texture, .isAdditive = isAdditive, .first = first, .count = 0});
        };

        for (const uint64_t key : m_sortKeys)
        {
            const auto index = static_cast<uint32_t>(key & k_particleIndexMask);
            const auto batchIndex = static_cast<uint32_t>((key & 0xFFFFFFFFu) >> k_particleIndexBits);
            appendRun(frame.batches[batchIndex].texture, false);
            m_upload.push_back(frame.particles[index]);
            ++m_runs.back().count;
        }

        for (const ParticleBatch& batch : frame.batches)
        {
            if (!batch.isAdditive || batch.count == 0 || m_state->textures.find(batch.texture) == nullptr)
            {
                continue;
            }

            appendRun(batch.texture, true);
            m_upload.insert(m_upload.end(), frame.particles.begin() + batch.first,
                frame.particles.begin() + batch.first + batch.count);
            m_runs.back().count += batch.count;
        }
    }

    void ParticlePass::record(vela::graphics::PassRecorder& recorder)
    {
        if (m_state->particles.frameIndex != m_state->frameIndex || m_state->particles.particles.empty())
        {
            return;
        }

        buildDrawList();

        if (m_upload.empty())
        {
            return;
        }

        if (auto uploaded = m_instances.update(std::as_bytes(std::span{m_upload})); !uploaded)
        {
            log(LogLevel::Error, "Render", "Particle upload failed: " + toError(uploaded.error()).message);
            return;
        }

        const vela::graphics::Material* bound = nullptr;

        for (const DrawRun& run : m_runs)
        {
            const TextureRecord* texture = m_state->textures.find(run.texture);
            const vela::graphics::Material* material = run.isAdditive ? &m_additiveMaterial : &m_alphaMaterial;

            if (texture == nullptr || run.count == 0)
            {
                continue;
            }

            if (material != bound)
            {
                recorder.bind(*material);
                recorder.bindVertexBuffer(m_instances);
                recorder.bindAttachment(k_depthSlot, std::string(k_sceneDepthAttachment));
                recorder.bindAttachment(k_shadowAtlasSlot, std::string(k_shadowMapAttachment), shadowCompareSampler());
                recorder.bindAttachment(k_environmentSlot, std::string(k_environmentAttachment));
                bound = material;
            }

            recorder.bindTexture(k_spriteSlot, texture->texture);
            recorder.drawInstanced(k_verticesPerParticle, run.count, 0, run.first);
        }
    }
} //namespace lockstep::render
