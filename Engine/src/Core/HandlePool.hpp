#ifndef LOCKSTEP_CORE_HANDLE_POOL_HPP
#define LOCKSTEP_CORE_HANDLE_POOL_HPP

#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace lockstep
{
    // Slots with generation counters behind generation-checked handles (physics BodyId and
    // ControllerId, audio SoundHandle and VoiceHandle). A removed slot gets a new
    // generation, so handles to what it held before are detected as stale.
    template<typename T>
    class HandlePool
    {
    public:
        // Index and generation of one slot; converted to the public handle type by the caller.
        struct Key
        {
            uint32_t index{0};
            uint32_t generation{0};
        };

        // Stores a value in a free slot.
        // value:   the value.
        // Returns: its key, or nothing if every index is in use.
        [[nodiscard]] std::optional<Key> insert(T value)
        {
            uint32_t index = 0;

            if (!m_freeSlots.empty())
            {
                index = m_freeSlots.back();
                m_freeSlots.pop_back();
            }
            else
            {
                if (m_slots.size() >= std::numeric_limits<uint32_t>::max())
                {
                    return std::nullopt;
                }

                index = static_cast<uint32_t>(m_slots.size());
                m_slots.emplace_back();
            }

            Slot& slot = m_slots[index];
            slot.value = std::move(value);
            slot.isAlive = true;
            ++m_count;

            return Key{index, slot.generation};
        }

        // Frees a slot and makes its key stale.
        // key:     the slot.
        // Returns: the value it held, or nothing for a stale key.
        std::optional<T> remove(Key key)
        {
            T* value = find(key);

            if (value == nullptr)
            {
                return std::nullopt;
            }

            Slot& slot = m_slots[key.index];
            T removed = std::move(slot.value);
            slot.value = T{};
            slot.isAlive = false;
            slot.generation = slot.generation == std::numeric_limits<uint32_t>::max() ? 1 : slot.generation + 1;
            m_freeSlots.push_back(key.index);
            --m_count;

            return removed;
        }

        // key:     the slot.
        // Returns: its value, or nullptr for a stale or null key.
        [[nodiscard]] T* find(Key key) noexcept
        {
            return const_cast<T*>(static_cast<const HandlePool&>(*this).find(key));
        }

        // key:     the slot.
        // Returns: its value, or nullptr for a stale or null key.
        [[nodiscard]] const T* find(Key key) const noexcept
        {
            if (key.generation == 0 || key.index >= m_slots.size())
            {
                return nullptr;
            }

            const Slot& slot = m_slots[key.index];

            return slot.isAlive && slot.generation == key.generation ? &slot.value : nullptr;
        }

        // index:   a slot index.
        // Returns: the key of the living value at `index`, or nothing if the slot is free.
        [[nodiscard]] std::optional<Key> keyAt(uint32_t index) const noexcept
        {
            if (index >= m_slots.size() || !m_slots[index].isAlive)
            {
                return std::nullopt;
            }

            return Key{index, m_slots[index].generation};
        }

        // Returns: the number of slots, living or free; valid indices for keyAt() are below it.
        [[nodiscard]] uint32_t capacity() const noexcept
        {
            return static_cast<uint32_t>(m_slots.size());
        }

        // Returns: the number of living values.
        [[nodiscard]] uint32_t count() const noexcept
        {
            return m_count;
        }

    private:
        struct Slot
        {
            T value{};
            uint32_t generation{1};
            bool isAlive{false};
        };

        std::vector<Slot> m_slots;
        std::vector<uint32_t> m_freeSlots;
        uint32_t m_count{0};
    };
} //namespace lockstep

#endif //LOCKSTEP_CORE_HANDLE_POOL_HPP
