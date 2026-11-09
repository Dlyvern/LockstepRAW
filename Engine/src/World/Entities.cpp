#include "Lockstep/World/Entities.hpp"

#include <limits>

namespace lockstep::world
{
    Result<EntityId> Entities::create(const EntityDescription& description)
    {
        uint32_t slotIndex = 0;

        if (!m_freeSlots.empty())
        {
            slotIndex = m_freeSlots.back();
            m_freeSlots.pop_back();
        }
        else
        {
            if (m_slots.size() >= std::numeric_limits<uint32_t>::max())
            {
                return Error{ErrorCode::OutOfMemory, "Entities::create: no entity ids left"};
            }

            slotIndex = static_cast<uint32_t>(m_slots.size());
            m_slots.push_back(Slot{});
        }

        Slot& slot = m_slots[slotIndex];
        slot.denseIndex = static_cast<uint32_t>(m_entities.size());
        slot.isAlive = true;

        const EntityId id{.index = slotIndex, .generation = slot.generation};
        m_entities.push_back(Entity(description));
        m_ids.push_back(id);

        return EntityId{id};
    }

    bool Entities::destroy(EntityId id)
    {
        if (aliveSlot(id) == nullptr)
        {
            return false;
        }

        Slot& slot = m_slots[id.index];
        const uint32_t removedIndex = slot.denseIndex;
        const uint32_t lastIndex = static_cast<uint32_t>(m_entities.size() - 1);

        if (removedIndex != lastIndex)
        {
            m_entities[removedIndex] = m_entities[lastIndex];
            m_ids[removedIndex] = m_ids[lastIndex];
            m_slots[m_ids[removedIndex].index].denseIndex = removedIndex;
        }

        m_entities.pop_back();
        m_ids.pop_back();

        slot.isAlive = false;
        ++slot.generation;

        const bool isGenerationExhausted = slot.generation == 0;

        if (!isGenerationExhausted)
        {
            m_freeSlots.push_back(id.index);
        }

        return true;
    }

    bool Entities::isAlive(EntityId id) const noexcept
    {
        return aliveSlot(id) != nullptr;
    }

    Entity* Entities::find(EntityId id) noexcept
    {
        const Slot* slot = aliveSlot(id);
        return slot != nullptr ? &m_entities[slot->denseIndex] : nullptr;
    }

    const Entity* Entities::find(EntityId id) const noexcept
    {
        const Slot* slot = aliveSlot(id);
        return slot != nullptr ? &m_entities[slot->denseIndex] : nullptr;
    }

    std::span<Entity> Entities::all() noexcept
    {
        return m_entities;
    }

    std::span<const Entity> Entities::all() const noexcept
    {
        return m_entities;
    }

    std::span<const EntityId> Entities::ids() const noexcept
    {
        return m_ids;
    }

    std::size_t Entities::count() const noexcept
    {
        return m_entities.size();
    }

    void Entities::beginTick() noexcept
    {
        for (Entity& entity : m_entities)
        {
            entity.m_previous = entity.m_current;
        }
    }

    const Entities::Slot* Entities::aliveSlot(EntityId id) const noexcept
    {
        if (id.isNull() || id.index >= m_slots.size())
        {
            return nullptr;
        }

        const Slot& slot = m_slots[id.index];
        return slot.isAlive && slot.generation == id.generation ? &slot : nullptr;
    }
} //namespace lockstep::world
