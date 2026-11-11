#ifndef LOCKSTEP_GAME_EFFECTS_BULLET_HOLES_HPP
#define LOCKSTEP_GAME_EFFECTS_BULLET_HOLES_HPP

#include "Audio/CombatAudio.hpp"
#include "Effects/BulletHoleLooks.hpp"
#include "Gore/BloodDecals.hpp"

#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    struct SceneLevel;

    // How bullet holes look, editable from the dev panel.
    // enabled:     leave holes where shots hit level geometry.
    // size:        edge length of a rifle hole decal on concrete (crushed rim and scorch included),
    //              metres; other surfaces and calibres scale it.
    // maxHoles:    holes kept at most (GraphicsQuality::decalBudget caps it further); the oldest
    //              goes first.
    // reliefScale: factor on every surface's crater relief.
    // remote:      the partner's and NPCs' shots leave holes too.
    struct BulletHoleTuning
    {
        bool enabled{true};
        float size{0.07f};
        uint32_t maxHoles{512};
        float reliefScale{1.0f};
        bool remote{true};
    };

    // One hole on a surface.
    // decal:    its placement, size and material.
    // body:     the body it sits on.
    // bodyPose: where that body was when the hole was made (a moved body takes its holes along by
    //           dropping them).
    // surface:  what it was punched into.
    struct BulletHole
    {
        BloodDecal decal{};
        lockstep::physics::BodyId body{};
        math::Transform bodyPose{};
        HoleSurface surface{HoleSurface::Concrete};
    };

    // Bullet holes on walls, floors, props and glass (presentation only, from this frame's impacts:
    // the local player's shots and, with `remote`, the partner's and NPCs'). Each surface kind has
    // its own decal look (BulletHoleLooks.hpp) with relief: the crater darkens on the side away from
    // the light and catches highlights on the other. Holes go away with what they sit on: a broken
    // glass pane, a removed collider, or a prop or door that moved.
    // tuning:      the look.
    // holes:       the holes, oldest first.
    // materials:   per surface kind, its material, made on first use.
    // isTried:     making them was attempted.
    // appliedRelief: the relief scale the materials were last given.
    // counts:      holes made per surface kind since the level loaded (dev tools).
    struct BulletHoles
    {
        BulletHoleTuning tuning{};
        std::vector<BulletHole> holes{};
        std::array<std::optional<lockstep::render::MaterialHandle>, k_holeSurfaceCount> materials{};
        bool isTried{false};
        float appliedRelief{1.0f};
        std::array<uint32_t, k_holeSurfaceCount> counts{};
    };

    // What presentBulletHoles() reads each frame.
    // physics:     the physics world (fits holes onto small props; checks their bodies).
    // level:       the loaded level (surface kinds from node names), or null.
    // impacts:     this frame's impacts.
    // unpaintable: bodies nothing lands on (the level's invisible blockers).
    // quad:        a unit decal quad in the XZ plane facing +Y.
    // budget:      most holes kept (GraphicsQuality::decalBudget).
    struct BulletHoleFrame
    {
        const lockstep::physics::PhysicsWorld* physics{nullptr};
        const SceneLevel* level{nullptr};
        std::span<const CombatImpact> impacts{};
        std::span<const lockstep::physics::BodyId> unpaintable{};
        lockstep::render::MeshHandle quad{};
        uint32_t budget{160};
    };

    // level:   the loaded level, or null.
    // body:    a body a shot hit.
    // Returns: the surface kind the hole is punched into, from the shot material and the surface
    //          names of the level instance the body belongs to.
    [[nodiscard]] HoleSurface holeSurfaceOfBody(const SceneLevel* level, lockstep::physics::BodyId body);

    // Drops holes whose body is gone or moved, leaves a hole at each of this frame's impacts on
    // level geometry (not flesh or blockers) and queues every hole for drawing.
    // holes:    the bullet holes.
    // renderer: the renderer (creates the materials on first use).
    // frame:    what this frame brings.
    void presentBulletHoles(BulletHoles& holes, lockstep::render::Renderer& renderer, const BulletHoleFrame& frame);

    // Removes every hole (level switch).
    // holes: the bullet holes.
    void clearBulletHoles(BulletHoles& holes);

#if LOCKSTEP_DEV_TOOLS
    // Draws the bullet hole section of the gore dev panel.
    // holes: the bullet holes.
    void bulletHolesDevSection(BulletHoles& holes);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_EFFECTS_BULLET_HOLES_HPP
