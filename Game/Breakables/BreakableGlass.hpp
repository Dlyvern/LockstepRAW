#ifndef LOCKSTEP_GAME_BREAKABLES_BREAKABLE_GLASS_HPP
#define LOCKSTEP_GAME_BREAKABLES_BREAKABLE_GLASS_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    namespace math = lockstep::math;

    struct SceneLevel;

    // Node name prefix of a breakable glass pane ("Breakable_Glass_Ward_3").
    inline constexpr std::string_view k_glassPanePrefix{"Breakable_Glass"};

    // Most panes a level may have (the replicated broken set is sized by it).
    inline constexpr std::size_t k_maxGlassPanes{4096};

    // Most panes one bullet breaks before it is spent.
    inline constexpr std::size_t k_maxPanesPerShot{4};

    // One breakable glass pane: a Breakable_Glass* node of the level (a thin box with a glass
    // material). Glass is alpha-blended, which normally gets no collider; panes get one so they
    // stop bullets and players until they break.
    // name:      the node's name.
    // instances: indices into Scene::instances of the pane's meshes.
    // centre:    world centre of the pane.
    // halfU, halfV: world half extents along the pane's two long sides.
    // normal:    unit world normal of the pane (its thin axis).
    struct GlassPane
    {
        std::string name;
        std::vector<uint32_t> instances;
        math::Vec3 centre{0.0f, 0.0f, 0.0f};
        math::Vec3 halfU{0.5f, 0.0f, 0.0f};
        math::Vec3 halfV{0.0f, 0.5f, 0.0f};
        math::Vec3 normal{0.0f, 0.0f, 1.0f};
    };

    // One pane breaking, for presentation (shards, debris, sound) and the client message.
    // pane:      index into BreakableGlass::panes.
    // point:     where the bullet struck it.
    // direction: unit direction the bullet travelled.
    struct GlassBreak
    {
        uint16_t pane{0};
        math::Vec3 point{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
    };

    // The level's breakable glass. The host decides what breaks and sends the broken set to the
    // client (GlassReplication.hpp); both apply it to their level the same way (hidden instances,
    // removed colliders). Reset when a level loads; broken panes stay broken over a checkpoint
    // restart, like the rest of the level state.
    // panes:    the panes, in scene node order (the same on host and client).
    // isBroken: per pane.
    // unsent:   host: breaks not sent to the client yet.
    // isSent:   host: the client has the current broken set (false forces a full resend, e.g. on
    //           a late join).
    // shown:    breaks this machine has not presented yet (consumed by presentGlassShatter()).
    struct BreakableGlass
    {
        std::vector<GlassPane> panes;
        std::vector<bool> isBroken;
        std::vector<GlassBreak> unsent;
        bool isSent{false};
        std::vector<GlassBreak> shown;
    };

    // Finds the level's Breakable_Glass* panes and gives each pane instance a collider, then logs
    // how many there are. Host and client.
    // level:   the loaded level (its scene and colliders).
    // physics: the physics world the colliders go into.
    // Returns: the panes, all intact.
    [[nodiscard]] BreakableGlass findGlassPanes(SceneLevel& level, lockstep::physics::PhysicsWorld& physics);

    // glass:   the level's glass.
    // level:   the loaded level.
    // body:    a body a ray hit.
    // Returns: the intact pane `body` belongs to, or nothing.
    [[nodiscard]] std::optional<std::size_t> findIntactPane(const BreakableGlass& glass, const SceneLevel& level,
        lockstep::physics::BodyId body);

    // Breaks a pane on this machine: its instances stop drawing and their colliders are removed;
    // the break is queued for presentation. Does nothing for a broken or unknown pane.
    // glass:   the level's glass.
    // level:   the loaded level.
    // physics: the physics world.
    // hit:     the pane, where and which way the bullet went.
    // Returns: true if the pane broke now.
    bool applyGlassBreak(BreakableGlass& glass, SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
        const GlassBreak& hit);

    // Host: a bullet (player or NPC; later explosions or melee) struck `body`. If it is an intact
    // pane, the pane breaks (applyGlassBreak()) and the break is queued for the client. The
    // collider is gone afterwards, so tracing the same ray again continues past the pane.
    // glass:     the level's glass.
    // level:     the loaded level.
    // physics:   the physics world.
    // body:      the body struck.
    // point:     where.
    // direction: unit direction of travel.
    // Returns:   true if a pane broke (the bullet flies on).
    bool shatterGlass(BreakableGlass& glass, SceneLevel& level, lockstep::physics::PhysicsWorld& physics,
        lockstep::physics::BodyId body, const math::Vec3& point, const math::Vec3& direction);

    // Returns: the number of broken panes.
    [[nodiscard]] std::size_t brokenPaneCount(const BreakableGlass& glass);
} //namespace game

#endif //LOCKSTEP_GAME_BREAKABLES_BREAKABLE_GLASS_HPP
