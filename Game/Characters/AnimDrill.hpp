#ifndef LOCKSTEP_GAME_CHARACTERS_ANIM_DRILL_HPP
#define LOCKSTEP_GAME_CHARACTERS_ANIM_DRILL_HPP

#include <Lockstep/Core/Math.hpp>

#include <optional>
#include <span>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    namespace math = lockstep::math;

    struct NpcLooks;
    struct PawnCharacters;
    struct Visuals;

    // Where the animation drill runs.
    // origin: the ground point it starts from (y: the ground height there; the ground is probed
    //         from 1 m above it, so keep it below any floor overhead).
    // yaw:    which way "forward" of the routine points (radians, 0 is -Z, positive left).
    // spacing: metres between the stand-ins.
    struct AnimDrillSetup
    {
        math::Vec3 origin{0.0f, 0.0f, 0.0f};
        float yaw{0.0f};
        float spacing{1.1f};
    };

    // Dev (`--dev-anim-drill x,y,z,yaw[,spacing]`): three client-only stand-ins side by side, a squad officer
    // (alert: aiming clips and turn clips) and a merc (relaxed rifle clips), drawn through the NPC
    // path, and Player2's character holding the Glock (pistol set; drawn through the pawn path, so
    // don't combine with a partner), walking a fixed routine from the origin so the locomotion
    // layers can be watched and captured: idle, walk forward 3 m, back, strafe right and left, a
    // diagonal, sharp and slow turns on the spot, a crouched walk; it repeats every
    // k_animDrillSeconds. They stand on the ground below them (stairs too). Presentation only.
    // setup:   from the command line, parsed on the first call.
    // seconds: time into the routine.
    // isParsed: the command line was read.
    struct AnimDrill
    {
        std::optional<AnimDrillSetup> setup{};
        float seconds{0.0f};
        bool isParsed{false};
    };

    // Length of the drill's routine, seconds.
    inline constexpr float k_animDrillSeconds{32.0f};

    // Advances the drill and draws its stand-ins (call between beginNpcLooks() and endNpcLooks()).
    // Does nothing without `--dev-anim-drill` or without characters.
    // drill:      the drill.
    // arguments:  the command line.
    // renderer:   the renderer.
    // physics:    the world the stand-ins stand in.
    // looks:      the NPC looks (the stand-ins have their own).
    // characters: the pawn characters (Player2's character is used), or null.
    // visuals:    the fallback boxes.
    // seconds:    World presentation seconds since the last frame.
    void presentAnimDrill(AnimDrill& drill, std::span<const char* const> arguments, lockstep::render::Renderer& renderer,
        lockstep::physics::PhysicsWorld& physics, NpcLooks& looks, PawnCharacters* characters,
        const Visuals& visuals, float seconds);
} //namespace game

#endif //LOCKSTEP_GAME_CHARACTERS_ANIM_DRILL_HPP
