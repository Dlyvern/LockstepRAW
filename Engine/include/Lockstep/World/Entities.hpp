#ifndef LOCKSTEP_WORLD_ENTITIES_HPP
#define LOCKSTEP_WORLD_ENTITIES_HPP

#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Core/Transform.hpp"
#include "Lockstep/World/TimeDomains.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::world
{
    // Stable handle to an entity. The generation tells a reused slot apart from the entity that
    // held it before, so an id kept after its entity was destroyed is detected as stale. A
    // default-constructed id (generation 0) never refers to an entity.
    struct EntityId
    {
        uint32_t index{0};
        uint32_t generation{0};

        // Returns: true for the null id. A non-null id may still be stale; ask Entities::isAlive().
        [[nodiscard]] constexpr bool isNull() const noexcept
        {
            return generation == 0;
        }

        friend constexpr bool operator==(EntityId, EntityId) noexcept = default;
    };

    // Who has authority over an entity once networking exists.
    // Host:    the host simulates it (world objects, NPCs).
    // Player1: the first player's machine controls it.
    // Player2: the second player's machine controls it.
    enum class EntityOwner : uint8_t
    {
        Host,
        Player1,
        Player2
    };

    // Settings for a new entity.
    // transform:  where it starts. Used as both the current and the previous tick's transform,
    //             so it does not interpolate in from the origin.
    // domain:     the time domain whose delta steps it.
    // owner:      who has authority over it.
    // clientOnly: true for local-only entities that are never replicated. Fixed for the
    //             entity's lifetime.
    struct EntityDescription
    {
        math::Transform transform{};
        TimeDomain domain{TimeDomain::World};
        EntityOwner owner{EntityOwner::Host};
        bool clientOnly{false};
    };

    // One entity's simulation state: its transform at the current and the previous tick plus the
    // properties it was created with. Simulation changes transform() during a tick; rendering
    // blends with interpolatedTransform().
    class Entity
    {
    public:
        // Returns: the transform at the current tick. Simulation writes this.
        [[nodiscard]] math::Transform& transform() noexcept
        {
            return m_current;
        }

        // Returns: the transform at the current tick.
        [[nodiscard]] const math::Transform& transform() const noexcept
        {
            return m_current;
        }

        // Returns: the transform at the previous tick.
        [[nodiscard]] const math::Transform& previousTransform() const noexcept
        {
            return m_previous;
        }

        // Blends the previous and the current tick's transform for rendering.
        // alpha:   FrameInfo::alpha, 0 gives the previous tick and 1 the current one.
        // Returns: the blended transform.
        [[nodiscard]] math::Transform interpolatedTransform(float alpha) const noexcept
        {
            return math::interpolate(m_previous, m_current, alpha);
        }

        // Moves the entity without interpolating: sets the current and the previous transform,
        // so the next frames show it at the new place right away. For spawns and corrections.
        // transform: the new transform.
        void teleport(const math::Transform& transform) noexcept
        {
            m_current = transform;
            m_previous = transform;
        }

        // Returns: the time domain whose delta steps this entity.
        [[nodiscard]] TimeDomain domain() const noexcept
        {
            return m_domain;
        }

        // Returns: who has authority over this entity.
        [[nodiscard]] EntityOwner owner() const noexcept
        {
            return m_owner;
        }

        // Returns: true if this entity exists only on this machine and is never replicated.
        [[nodiscard]] bool isClientOnly() const noexcept
        {
            return m_clientOnly;
        }

    private:
        friend class Entities;

        explicit Entity(const EntityDescription& description) noexcept
            : m_current(description.transform)
            , m_previous(description.transform)
            , m_domain(description.domain)
            , m_owner(description.owner)
            , m_clientOnly(description.clientOnly)
        {
        }

        math::Transform m_current;
        math::Transform m_previous;
        TimeDomain m_domain;
        EntityOwner m_owner;
        bool m_clientOnly;
    };

    // The Engine's entity store. Entities are kept densely, so iterating all() is a plain array
    // walk; ids() lists the id of each, in the same order. Creating or destroying entities
    // invalidates spans and Entity pointers taken before, and may reorder the entities.
    // The run loop calls beginTick() before every fixed tick; Game creates, destroys and steps
    // entities (each by its domain's delta).
    class Entities
    {
    public:
        // Creates an entity.
        // description: its starting transform, time domain, owner and client-only flag.
        // Returns:     the new entity's id, or OutOfMemory if no more ids are available.
        [[nodiscard]] Result<EntityId> create(const EntityDescription& description);

        // Destroys an entity. Its id, and every copy of it, becomes stale.
        // id:      the entity to destroy.
        // Returns: true if the entity was alive and is now destroyed, false for a stale or null id.
        bool destroy(EntityId id);

        // id:      the entity to check.
        // Returns: true if `id` refers to a living entity.
        [[nodiscard]] bool isAlive(EntityId id) const noexcept;

        // Looks up an entity.
        // id:      the entity.
        // Returns: the entity, or nullptr for a stale or null id.
        [[nodiscard]] Entity* find(EntityId id) noexcept;

        // Looks up an entity.
        // id:      the entity.
        // Returns: the entity, or nullptr for a stale or null id.
        [[nodiscard]] const Entity* find(EntityId id) const noexcept;

        // Returns: all living entities, in the same order as ids().
        [[nodiscard]] std::span<Entity> all() noexcept;

        // Returns: all living entities, in the same order as ids().
        [[nodiscard]] std::span<const Entity> all() const noexcept;

        // Returns: the ids of all living entities, in the same order as all().
        [[nodiscard]] std::span<const EntityId> ids() const noexcept;

        // Returns: the number of living entities.
        [[nodiscard]] std::size_t count() const noexcept;

        // Starts a new tick: every entity's current transform becomes its previous one. Called by
        // the run loop before each fixed tick; Game does not call it.
        void beginTick() noexcept;

    private:
        struct Slot
        {
            uint32_t generation{1};
            uint32_t denseIndex{0};
            bool isAlive{false};
        };

        [[nodiscard]] const Slot* aliveSlot(EntityId id) const noexcept;

        std::vector<Slot> m_slots;
        std::vector<uint32_t> m_freeSlots;
        std::vector<Entity> m_entities;
        std::vector<EntityId> m_ids;
    };
} //namespace lockstep::world

#endif //LOCKSTEP_WORLD_ENTITIES_HPP
