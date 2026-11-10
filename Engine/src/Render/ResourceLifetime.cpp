#include "Lockstep/Render/Renderer.hpp"

#include "Lockstep/Core/Log.hpp"

#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <cassert>
#include <string>

namespace lockstep::render
{
    bool Renderer::destroyMesh(MeshHandle mesh)
    {
        if (!m_state->meshes.retire(mesh))
        {
            return false;
        }

        m_state->shadowCache.invalidate();

        return true;
    }

    bool Renderer::destroyMaterial(MaterialHandle material)
    {
        if (!m_state->materials.retire(material))
        {
            return false;
        }

        m_state->shadowCache.invalidate();

        return true;
    }

    bool Renderer::destroyTexture(TextureHandle texture)
    {
        return m_state->textures.retire(texture);
    }

    GpuMemoryStats Renderer::memoryStats() const
    {
        const RendererState& state = *m_state;
        const vela::core::MemoryStats memory = state.context.getMemoryStats();

        return GpuMemoryStats{
            .deviceBytesUsed = memory.deviceBytesUsed,
            .deviceBytesBudget = memory.deviceBytesBudget,
            .deviceBytesAllocated = memory.deviceBytesAllocated,
            .allocationCount = memory.allocationCount,
            .isBudgetFromDriver = memory.budgetFromDriver,
            .meshes = state.meshes.aliveCount(),
            .materials = state.materials.aliveCount(),
            .textures = state.textures.aliveCount(),
            .pendingReleases = state.meshes.retiredCount() + state.materials.retiredCount()
                + state.textures.retiredCount()};
    }

    Status Renderer::releaseDestroyed()
    {
        RendererState& state = *m_state;
        state.draws.clear();
        state.jointData.clear();
        releaseRetiredResources(state);

        if (auto released = state.context.releaseDestroyed(); !released)
        {
            return toError(released.error());
        }

        return {};
    }

    void reportStaleHandle(RendererState& state, const char* what)
    {
        assert(false && "stale render handle: the resource was destroyed");

        if (state.hasLoggedStaleHandle)
        {
            return;
        }

        state.hasLoggedStaleHandle = true;
        log(LogLevel::Error, "Render", std::string(what)
            + " handle is stale (its resource was destroyed); the draw is ignored. Further stale handles are not logged.");
    }

    void releaseRetiredResources(RendererState& state)
    {
        state.materials.releaseRetired([&state](MaterialRecord& material)
        {
            for (const uint32_t slot : material.textureSlots)
            {
                if (TextureRecord* texture = state.textures.atIndex(slot); texture != nullptr && texture->materialUses > 0)
                {
                    --texture->materialUses;
                }
            }

            return true;
        });

        state.meshes.releaseRetired([](MeshRecord&)
        {
            return true;
        });

        state.textures.releaseRetired([](TextureRecord& texture)
        {
            return texture.materialUses == 0;
        });
    }
} //namespace lockstep::render
