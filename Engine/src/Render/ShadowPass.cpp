#include "ShadowPass.hpp"

#include "Lockstep/Core/Log.hpp"

#include "FrameAttachments.hpp"
#include "RendererState.hpp"

#include <cstddef>
#include <span>
#include <string>

namespace lockstep::render
{
    namespace
    {
        struct MaskedShadowConstants
        {
            math::Mat4 lightModel;
            float mask[4];
        };

        static_assert(sizeof(MaskedShadowConstants) == 80);
    } //namespace

    Result<std::unique_ptr<ShadowPass>> ShadowPass::create(RendererState& state)
    {
        return std::unique_ptr<ShadowPass>(new ShadowPass(state));
    }

    ShadowPass::ShadowPass(RendererState& state)
        : m_state(&state)
    {
    }

    vela::graphics::PassDescription ShadowPass::describe() const
    {
        vela::graphics::AttachmentSlot depth{};
        depth.name = std::string(k_shadowMapAttachment);
        depth.format = k_depthFormat;
        depth.load = vela::graphics::LoadOp::Load;
        depth.store = vela::graphics::StoreOp::Store;
        depth.clear.depth = 1.0f;
        depth.size = {m_state->shadowAtlasWidth, m_state->shadowAtlasWidth / 2};
        depth.persistent = true;

        vela::graphics::PassDescription description{};
        description.depthOutput = depth;

        return description;
    }

    void ShadowPass::record(vela::graphics::PassRecorder& recorder)
    {
        gatherTiles(recorder);

        if (m_hasInstances)
        {
            if (auto uploaded = m_state->shadowInstanceBuffer.update(std::as_bytes(std::span(m_instances))); !uploaded)
            {
                log(LogLevel::Error, "Render", "Shadow instances upload failed: " + uploaded.error().message);
                m_state->shadowCache.invalidate();
                return;
            }
        }

        for (uint32_t tile = 0; tile < k_maxShadowTiles; ++tile)
        {
            if (m_isTileRendered[tile])
            {
                drawTile(recorder, tile);
            }
        }
    }

    void ShadowPass::gatherTiles(const vela::graphics::PassRecorder& recorder)
    {
        ShadowCache& cache = m_state->shadowCache;
        const FrameDrawLists& lists = m_state->drawLists;
        const PreparedLighting& prepared = m_state->prepared;

        if (cache.isInvalidated || cache.depthBias != m_state->shadowMaterialBias
            || recorder.wasAttachmentReset(std::string(k_shadowMapAttachment)))
        {
            cache.tileValid.fill(false);
            cache.depthBias = m_state->shadowMaterialBias;
            cache.isInvalidated = false;
        }

        m_instances.clear();
        m_batches.clear();

        for (uint32_t tile = 0; tile < k_maxShadowTiles; ++tile)
        {
            m_tileBatches[tile] = static_cast<uint32_t>(m_batches.size());
            m_isTileRendered[tile] = false;

            if (!prepared.isTileActive(tile))
            {
                cache.tileValid[tile] = false;
                continue;
            }

            if (cache.tileValid[tile] && cache.tileKeys[tile] == lists.shadowKeys[tile] && !lists.shadowAnimated[tile])
            {
                continue;
            }

            cache.tileKeys[tile] = lists.shadowKeys[tile];
            cache.tileValid[tile] = true;
            m_isTileRendered[tile] = true;
            gatherCasters(tile);
        }

        m_tileBatches[k_maxShadowTiles] = static_cast<uint32_t>(m_batches.size());
        m_hasInstances = !m_instances.empty();
        cache.drawCalls = static_cast<uint32_t>(m_batches.size());
    }

    void ShadowPass::gatherCasters(uint32_t tile)
    {
        const math::Mat4& tileViewProjection = m_state->prepared.tiles[tile].atlasViewProjection;
        const uint32_t tileStart = m_tileBatches[tile];

        for (const uint32_t index : m_state->drawLists.shadowCasters[tile])
        {
            const DrawItem& item = m_state->draws[index];

            if (item.material->kind == MeshKind::Skinned)
            {
                m_batches.push_back(Batch{.mesh = item.mesh, .record = item.material, .draw = index});
                continue;
            }

            const vela::graphics::Material* material = item.material->maskedShadow
                ? &*item.material->maskedShadow : &*m_state->shadowMaterial;

            const bool extendsLast = m_batches.size() > tileStart && m_batches.back().count > 0
                && m_batches.back().mesh == item.mesh && m_batches.back().material == material;

            if (extendsLast)
            {
                ++m_batches.back().count;
            }
            else
            {
                m_batches.push_back(Batch{.mesh = item.mesh, .material = material, .record = item.material,
                    .firstInstance = static_cast<uint32_t>(m_instances.size()), .count = 1});
            }

            m_instances.push_back(tileViewProjection * item.model);
        }
    }

    void ShadowPass::drawTile(vela::graphics::PassRecorder& recorder, uint32_t tile)
    {
        const math::Vec4 rect = shadowTileRect(tile);
        const uint32_t atlasWidth = m_state->shadowAtlasWidth;
        const uint32_t texels = shadowTileTexels(tile, atlasWidth);
        const int32_t x = static_cast<int32_t>(rect.x * static_cast<float>(atlasWidth));
        const int32_t y = static_cast<int32_t>(rect.y * static_cast<float>(atlasWidth / 2));

        recorder.setScissor(x, y, texels, texels);
        recorder.clearDepth(x, y, texels, texels);

        const math::Mat4& tileViewProjection = m_state->prepared.tiles[tile].atlasViewProjection;

        for (uint32_t batchIndex = m_tileBatches[tile]; batchIndex < m_tileBatches[tile + 1]; ++batchIndex)
        {
            const Batch& batch = m_batches[batchIndex];
            const PbrFactors& factors = batch.record->factors;

            if (batch.count == 0)
            {
                const DrawItem& item = m_state->draws[batch.draw];
                const MaskedShadowConstants constants{.lightModel = tileViewProjection * item.model,
                    .mask = {factors.baseColorFactor[3], factors.surface[3], static_cast<float>(item.jointBase), 0.0f}};
                const vela::graphics::Material& material = item.material->maskedShadow
                    ? *item.material->maskedShadow : *m_state->skinnedShadowMaterial;
                recorder.draw(*item.mesh, material, constants);
                continue;
            }

            if (batch.record->maskedShadow)
            {
                const MaskedShadowConstants constants{.lightModel = math::Mat4{1.0f},
                    .mask = {factors.baseColorFactor[3], factors.surface[3], 0.0f, 0.0f}};
                recorder.bind(*batch.material);
                recorder.setConstants(constants);
            }

            recorder.drawInstanced(*batch.mesh, *batch.material, batch.count, batch.firstInstance);
        }
    }
} //namespace lockstep::render
