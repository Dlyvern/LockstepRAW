#ifndef LOCKSTEP_GAME_WEAPONS_PENETRATION_HPP
#define LOCKSTEP_GAME_WEAPONS_PENETRATION_HPP

#include "Weapons/Hitscan.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    namespace math = lockstep::math;

    struct SceneLevel;

    // What a bullet goes through, guessed from the level node names (and their ancestors) of the
    // body it hits. Hard is the fallback for level geometry (outer walls, floors); Soft for props
    // that are not level instances (toys).
    enum class ShotMaterial : uint8_t
    {
        Glass = 0,
        Soft,
        Wood,
        Drywall,
        Metal,
        Hard,
        Count
    };

    // Number of ShotMaterial kinds.
    inline constexpr std::size_t k_shotMaterialCount{static_cast<std::size_t>(ShotMaterial::Count)};

    // How rounds go through thin things. Host and client use the same settings (the client only
    // for impact effects). Editable from the dev panel.
    // power:           what a round can go through, in units of `cost` times metres.
    // cost:            per material, power used per metre of thickness.
    // maxPenetrations: most surfaces one round goes through (glass does not count).
    // maxThickness:    thickest thing probed, metres; anything thicker stops the round.
    // damageKeep:      damage multiplier per penetration, on top of the power left.
    // rangeKeep:       fraction of the remaining range kept per penetration.
    struct PenetrationSettings
    {
        float power{1.0f};
        std::array<float, k_shotMaterialCount> cost{{2.0f, 3.0f, 7.0f, 5.5f, 40.0f, 60.0f}};
        uint32_t maxPenetrations{2};
        float maxThickness{0.45f};
        float damageKeep{0.85f};
        float rangeKeep{0.7f};
    };

    // One straight piece of a shot's path.
    // origin, end:  where it starts and stops.
    // damageScale:  damage a hit at its end does, relative to an unbroken round.
    struct ShotSegment
    {
        math::Vec3 origin{0.0f, 0.0f, 0.0f};
        math::Vec3 end{0.0f, 0.0f, 0.0f};
        float damageScale{1.0f};
    };

    // A surface a shot struck: its entry, or the exit face of something it went through.
    // point, normal: where and the surface normal (facing the side the round came from for an
    //               entry, the side it left to for an exit).
    // body:        the body (null for a character).
    // controller:  the character struck (null for a body).
    // material:    what it is made of.
    // isExit:      the exit face of a penetration.
    struct ShotImpact
    {
        math::Vec3 point{0.0f, 0.0f, 0.0f};
        math::Vec3 normal{0.0f, 1.0f, 0.0f};
        lockstep::physics::BodyId body{};
        lockstep::physics::ControllerId controller{};
        ShotMaterial material{ShotMaterial::Hard};
        bool isExit{false};
    };

    // Everything one shot did.
    // result:       what stopped it (or Miss), with the distance along its whole path.
    // segments:     its straight pieces, first to last; the last one ends at `result`.
    // impacts:      surfaces struck, in order.
    // penetrations: surfaces it went through.
    struct ShotPath
    {
        ShotResult result{};
        std::vector<ShotSegment> segments;
        std::vector<ShotImpact> impacts;
        uint32_t penetrations{0};
    };

    // Called for a level body a shot strikes, before penetration is tried: returns true when the
    // body is gone now (a glass pane shattered), so the round flies on unslowed.
    using ShotBreaker = std::function<bool(lockstep::physics::BodyId body, const math::Vec3& point,
        const math::Vec3& direction)>;

    // level:   the loaded level, or null.
    // body:    a body a ray hit.
    // Returns: what the body is made of (Hard for unnamed level geometry, Soft for non-level bodies).
    [[nodiscard]] ShotMaterial shotMaterialOfBody(const SceneLevel* level, lockstep::physics::BodyId body);

    // material: a material kind.
    // Returns:  its readable name.
    [[nodiscard]] const char* shotMaterialName(ShotMaterial material);

    // Traces a shot that may go through thin things: at each level surface struck, a ray traced
    // back from beyond it finds the far face; the thickness times the material's cost uses up the
    // round's power, and while power, range and the penetration count allow, the round goes on
    // from the far face with less damage. Characters always stop it.
    // physics:   the physics world.
    // level:     the loaded level (materials), or null.
    // origin:    the shooter's eyes.
    // direction: unit shot direction.
    // range:     farthest the round reaches.
    // shooter:   the shooter's controller, which the round passes through.
    // enemy:     the arena enemy's controller, or null.
    // settings:  penetration rules.
    // breaker:   host: shatters glass (may be empty).
    // Returns:   the path.
    [[nodiscard]] ShotPath traceShotPath(const lockstep::physics::PhysicsWorld& physics, const SceneLevel* level,
        const math::Vec3& origin, const math::Vec3& direction, float range, lockstep::physics::ControllerId shooter,
        lockstep::physics::ControllerId enemy, const PenetrationSettings& settings, const ShotBreaker& breaker = {});

#if LOCKSTEP_DEV_TOOLS
    // Draws the penetration controls of the Game dev panel.
    // settings: the rules, edited.
    void penetrationDevSection(PenetrationSettings& settings);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_WEAPONS_PENETRATION_HPP
