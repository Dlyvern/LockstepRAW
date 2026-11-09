#ifndef LOCKSTEP_GAME_ARCHETYPES_HPP
#define LOCKSTEP_GAME_ARCHETYPES_HPP

#include <cstdint>

namespace game
{
    // What a replicated entity is, sent as its replication archetype so the client knows how to
    // draw it. 0 is the Engine's "untagged". A toy is tagged Toy + its index in the level's toy
    // list (see Toys.hpp).
    enum class Archetype : uint16_t
    {
        None = 0,
        Pawn = 1,
        Enemy = 2,
        Toy = 16
    };
} //namespace game

#endif //LOCKSTEP_GAME_ARCHETYPES_HPP
