#ifndef LOCKSTEP_GAME_GORE_GORE_HPP
#define LOCKSTEP_GAME_GORE_GORE_HPP

#include "Gore/BloodAssets.hpp"
#include "Gore/BloodDecals.hpp"
#include "Gore/BloodSpray.hpp"
#include "Gore/CorpseHits.hpp"
#include "Gore/GoreEvents.hpp"
#include "Gore/GoreRandom.hpp"
#include "Gore/SeveredLimbs.hpp"
#include "Npcs/NpcPresentation.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/World/Entities.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace game
{
    // How blood and severed limbs look on this machine, editable from the dev panel.
    // maxPieces:      severed pieces kept (the oldest goes first).
    // pieceSeconds:   World seconds a piece lies around before it is removed.
    // pieceSpeed:     speed a severed piece flies off with, m/s.
    // maxDecals:      blood decals kept.
    // splatterSize:   size of a splatter behind a hit, metres.
    // splatterReach:  how far behind a hit blood reaches a wall or floor, metres.
    // poolSize:       full size of the pool under a body, metres.
    // poolSeconds:    World seconds a pool takes to spread.
    // maxDroplets:    flying droplets kept.
    // hitDroplets:    droplets a hit throws (a sever throws three times as many).
    // bleedSeconds:   World seconds a stump and a piece drip.
    // dripInterval:   World seconds between drips of one wound.
    struct GoreTuning
    {
        uint32_t maxPieces{24};
        float pieceSeconds{60.0f};
        float pieceSpeed{5.5f};
        uint32_t maxDecals{320};
        float splatterSize{1.0f};
        float splatterReach{3.0f};
        float poolSize{1.8f};
        float poolSeconds{9.0f};
        uint32_t maxDroplets{400};
        uint32_t hitDroplets{18};
        float bleedSeconds{12.0f};
        float dripInterval{0.09f};
    };

    // A stump that still bleeds.
    // entity:  the character's local entity.
    // limb:    the severed limb (its root joint is where the blood comes out).
    // seconds: World seconds since the cut.
    // drip:    World seconds until the next drip.
    struct BleedingStump
    {
        lockstep::world::EntityId entity{};
        lockstep::anim::Limb limb{lockstep::anim::Limb::None};
        float seconds{0.0f};
        float drip{0.0f};
    };

    // Blood and dismemberment as this machine shows them. Presentation only.
    // tuning:      the look.
    // pending:     events to show next frame (the host's from its rules, the client's received).
    // assets:      blood textures, materials and meshes, made on first use.
    // isAssetsTried: making them was attempted (a failure is logged once; gore is then skipped).
    // decals:      splatters, pools and drips.
    // spray:       flying droplets.
    // pieces:      severed limbs, oldest first.
    // stumps:      stumps still bleeding.
    // pooled:      local entities of bodies that already have a pool.
    // random:      for drips and smears (event looks use their own seed).
    // eye:         where the camera was last frame (the dev panel's "nearest NPC").
    // corpseShots: the local player's shots already traced against dead bodies.
    struct GoreState
    {
        GoreTuning tuning{};
        std::vector<GoreEvent> pending;
        std::optional<BloodAssets> assets;
        bool isAssetsTried{false};
        BloodDecals decals{};
        BloodSpray spray{};
        std::vector<SeveredPiece> pieces;
        std::vector<BleedingStump> stumps;
        std::vector<lockstep::world::EntityId> pooled;
        GoreRandom random{};
        math::Vec3 eye{0.0f, 0.0f, 0.0f};
        CorpseShots corpseShots{};
    };

    // Shows this frame's gore: applies pending events (bursts, splatters behind hits, severed
    // limbs on the NPC hit; a hit on a dead body also pushes that body part of this machine's
    // ragdoll unless this machine fired it; a severed arm drops the weapon it held), lays pools under fresh bodies, lets stumps and pieces drip, moves
    // droplets and pieces, and queues everything for drawing; a ragdoll of a body missing limbs
    // loses the bodies of those limbs. Call after the NPCs are drawn.
    // state:   the game state (gore, NPC looks).
    // engine:  Engine services.
    // alpha:   interpolation between the last two ticks.
    // seconds: World seconds since the last frame.
    // eye:     camera position.
    void presentGore(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, float alpha, float seconds,
        const math::Vec3& eye);

    // Removes every piece, decal and droplet (level switch, restart).
    // gore:    the gore state.
    // physics: destroys the pieces' bodies.
    void clearGore(GoreState& gore, lockstep::physics::PhysicsWorld& physics);

    // Client: takes a Game message if it is a gore message (queued for the next frame).
    // state:   the game state.
    // message: a received Game message.
    // Returns: true if it was a gore message.
    bool receiveGoreMessage(lockstep::app::GameState& state, std::span<const std::byte> message);
} //namespace game

#endif //LOCKSTEP_GAME_GORE_GORE_HPP
