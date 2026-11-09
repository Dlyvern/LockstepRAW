#ifndef LOCKSTEP_RENDER_RESOURCE_POOL_HPP
#define LOCKSTEP_RENDER_RESOURCE_POOL_HPP

#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace lockstep::render
{
    // Stores GPU resources of one kind behind generation-checked handles. Backed by a deque so a
    // resource never moves once added: the render scene keeps raw pointers to submitted meshes
    // and materials until the frame is recorded.
    //
    // Destruction has two steps. retire() makes the handle stale at once (find() fails, the slot's
    // generation moves on) but keeps the resource, because draws already submitted this frame may
    // still point at it. releaseRetired() destroys retired resources at the end of render(); the
    // Vela objects then hand their GPU memory to Vela's deletion queue, which frees it once every
    // frame in flight that could use it has finished. Only then is the slot reused.
    template<typename HandleT, typename ResourceT>
    class ResourcePool
    {
    public:
        // Takes ownership of a resource.
        // resource: the resource to store.
        // Returns:  the handle that now refers to it.
        [[nodiscard]] HandleT add(ResourceT&& resource)
        {
            uint32_t index = 0;

            if (!m_freeSlots.empty())
            {
                index = m_freeSlots.back();
                m_freeSlots.pop_back();
            }
            else
            {
                index = static_cast<uint32_t>(m_slots.size());
                m_slots.emplace_back();
            }

            Slot& slot = m_slots[index];
            slot.resource.emplace(std::move(resource));
            slot.isAlive = true;
            ++m_aliveCount;

            return HandleT{index, slot.generation};
        }

        // Looks up a living resource.
        // handle:  the handle to resolve.
        // Returns: the resource, or nullptr if the handle is invalid, stale or out of range.
        [[nodiscard]] const ResourceT* find(HandleT handle) const
        {
            const Slot* slot = aliveSlot(handle);

            return slot != nullptr ? &*slot->resource : nullptr;
        }

        // handle:  a handle.
        // Returns: true if the handle looks valid but no longer resolves: its resource was
        //          destroyed (or it comes from another Renderer).
        [[nodiscard]] bool isStale(HandleT handle) const
        {
            return handle.isValid() && aliveSlot(handle) == nullptr;
        }

        // Makes a handle stale and queues its resource for releaseRetired().
        // handle:  the resource to destroy.
        // Returns: true if it was alive; false for an invalid, stale or foreign handle.
        bool retire(HandleT handle)
        {
            Slot* slot = const_cast<Slot*>(aliveSlot(handle));

            if (slot == nullptr)
            {
                return false;
            }

            slot->isAlive = false;
            slot->generation = slot->generation == std::numeric_limits<uint32_t>::max() ? 1 : slot->generation + 1;
            m_retired.push_back(handle.index);
            --m_aliveCount;

            return true;
        }

        // Destroys retired resources and frees their slots. A resource `canRelease` refuses stays
        // retired and is offered again on the next call.
        // canRelease: callable taking ResourceT&, returning true if it may be destroyed now. It may
        //             also do the work that must precede the destruction.
        template<typename Function>
        void releaseRetired(Function&& canRelease)
        {
            std::erase_if(m_retired, [this, &canRelease](uint32_t index)
            {
                Slot& slot = m_slots[index];

                if (!canRelease(*slot.resource))
                {
                    return false;
                }

                slot.resource.reset();
                m_freeSlots.push_back(index);

                return true;
            });
        }

        // index:   a slot index (a handle's index), alive or retired.
        // Returns: the resource stored there, or nullptr if the slot is free.
        [[nodiscard]] ResourceT* atIndex(uint32_t index)
        {
            return index < m_slots.size() && m_slots[index].resource ? &*m_slots[index].resource : nullptr;
        }

        // Calls `function` with every stored resource, living or retired but not yet released.
        // function: callable taking ResourceT&.
        template<typename Function>
        void forEach(Function&& function)
        {
            for (Slot& slot : m_slots)
            {
                if (slot.resource)
                {
                    function(*slot.resource);
                }
            }
        }

        // Returns: the number of living resources.
        [[nodiscard]] uint32_t aliveCount() const
        {
            return m_aliveCount;
        }

        // Returns: the number of retired resources not yet released.
        [[nodiscard]] uint32_t retiredCount() const
        {
            return static_cast<uint32_t>(m_retired.size());
        }

    private:
        struct Slot
        {
            std::optional<ResourceT> resource;
            uint32_t generation{1};
            bool isAlive{false};
        };

        [[nodiscard]] const Slot* aliveSlot(HandleT handle) const
        {
            if (!handle.isValid() || handle.index >= m_slots.size())
            {
                return nullptr;
            }

            const Slot& slot = m_slots[handle.index];

            return slot.isAlive && slot.generation == handle.generation ? &slot : nullptr;
        }

        std::deque<Slot> m_slots;
        std::vector<uint32_t> m_freeSlots;
        std::vector<uint32_t> m_retired;
        uint32_t m_aliveCount{0};
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RESOURCE_POOL_HPP
