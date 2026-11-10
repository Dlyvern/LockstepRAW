#ifndef LOCKSTEP_GAME_AUDIO_SURFACES_HPP
#define LOCKSTEP_GAME_AUDIO_SURFACES_HPP

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    namespace math = lockstep::math;

    struct SceneLevel;

    // What a pawn stands on, for footstep and landing sounds. Concrete is the fallback.
    enum class Surface : uint8_t
    {
        Concrete = 0,
        Tile,
        Wood,
        Metal,
        Grass,
        Asphalt,
        Count
    };

    // Number of Surface kinds.
    inline constexpr std::size_t k_surfaceCount{static_cast<std::size_t>(Surface::Count)};

    // surface: a surface kind.
    // Returns: its lower-case name, as used in sound file names ("tile").
    [[nodiscard]] const char* surfaceName(Surface surface);

    // Guesses a surface from an authored name (a node or material name such as "floor_tiles_06",
    // "wood_floor", "hangar_concrete_floor", "M_BrushedSteel", "Park_Grass", "BO_Asphalt").
    // Case-insensitive keyword match.
    // name:    the name.
    // Returns: the surface, or nothing when no keyword matches.
    [[nodiscard]] std::optional<Surface> surfaceFromName(std::string_view name);

    // Classifies the level instance a body belongs to by its node name and the names of its named
    // ancestors (nearest first).
    // level:   the loaded level, or null (Concrete).
    // body:    a body hit by a ray.
    // Returns: the surface, Concrete when nothing named is found.
    [[nodiscard]] Surface surfaceOfBody(const SceneLevel* level, lockstep::physics::BodyId body);

    // Finds the surface below a point: a short downward ray, mapped back to the level instance it
    // hit and classified by its node name and the names of its named ancestors (nearest first).
    // Material names would be better but are not in the loaded scene yet.
    // level:      the loaded level, or null (Concrete).
    // physics:    the physics world.
    // foot:       a pawn's foot position.
    // controller: the pawn's own controller, ignored by the ray (may be null).
    // Returns:    the surface, Concrete when nothing named is found.
    [[nodiscard]] Surface surfaceBelow(const SceneLevel* level, const lockstep::physics::PhysicsWorld& physics,
        const math::Vec3& foot, lockstep::physics::ControllerId controller);
} //namespace game

#endif //LOCKSTEP_GAME_AUDIO_SURFACES_HPP
