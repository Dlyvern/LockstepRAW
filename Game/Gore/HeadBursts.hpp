#ifndef LOCKSTEP_GAME_GORE_HEAD_BURSTS_HPP
#define LOCKSTEP_GAME_GORE_HEAD_BURSTS_HPP

#include "Gore/GoreEvents.hpp"
#include "Gore/GoreRandom.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Audio/AudioTypes.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>
#include <Lockstep/Render/Particles.hpp>
#include <Lockstep/Render/Resources.hpp>
#include <Lockstep/World/Entities.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace lockstep::physics
{
    class PhysicsWorld;
} //namespace lockstep::physics

namespace game
{
    namespace math = lockstep::math;

    struct BloodAssets;
    struct NpcLook;

    // How a head burst looks on this machine (dev panel). Presentation only.
    // maxGibs:         gibs kept lying around (the oldest goes first).
    // gibSeconds:      World seconds a gib stays.
    // gibsPerBurst:    flesh chunks one burst throws (and half as many bone bits).
    // gibSpeed:        speed gibs fly off with, m/s.
    // gibSize:         size of a flesh chunk, metres (bone bits are smaller).
    // fountainSeconds: World seconds the neck spurts.
    // fountainPulse:   World seconds between spurts.
    // splatterRays:    rays that look for walls, floor and ceiling to splatter.
    // splatterReach:   how far the splatter reaches, metres.
    // splatterSize:    size of a splatter at the burst, metres (smaller further away).
    // push:            push on the chest of the ragdoll, kg m/s.
    // dropletScale:    factor on the droplets of an ordinary hit.
    // particleScale:   factor on the particle blood of an ordinary head hit.
    // volume:          the splatter sound.
    struct HeadBurstTuning
    {
        uint32_t maxGibs{48};
        float gibSeconds{45.0f};
        uint32_t gibsPerBurst{8};
        float gibSpeed{4.5f};
        float gibSize{0.045f};
        float fountainSeconds{1.1f};
        float fountainPulse{0.16f};
        uint32_t splatterRays{16};
        float splatterReach{4.5f};
        float splatterSize{1.5f};
        float push{240.0f};
        float dropletScale{5.0f};
        float particleScale{2.0f};
        float volume{1.0f};
    };

    // A small piece of a burst head on a debris body.
    // body:     its debris body.
    // isBone:   a bone-white bit, else a dark red flesh chunk.
    // size:     its scale, metres.
    // seconds:  World seconds since it flew off.
    // previous, current: its last two placements seen, for interpolation.
    struct Gib
    {
        lockstep::physics::BodyId body{};
        bool isBone{false};
        float size{0.04f};
        float seconds{0.0f};
        math::Transform previous{};
        math::Transform current{};
    };

    // A neck spurting after a head burst.
    // entity:  the body's local entity.
    // seconds: World seconds since the burst.
    // pulse:   World seconds until the next spurt.
    struct NeckFountain
    {
        lockstep::world::EntityId entity{};
        float seconds{0.0f};
        float pulse{0.0f};
    };

    // The meshes, materials, sounds and particle effects of head bursts, made on first use.
    // chunk:     an irregular unit lump (gibs are scaled copies).
    // flesh, bone: the gib materials.
    // sounds:    the wet splatter variations (sounds/impacts/headburst_<n>).
    // mist:      a lingering red mist cloud.
    // spurt:     the neck's pulsing stream.
    // chunkHull: the lump's points, for the gib bodies.
    struct HeadBurstAssets
    {
        lockstep::render::MeshHandle chunk{};
        lockstep::render::MaterialHandle flesh{};
        lockstep::render::MaterialHandle bone{};
        std::vector<lockstep::audio::SoundHandle> sounds;
        lockstep::render::ParticleEffectId mist{};
        lockstep::render::ParticleEffectId spurt{};
        std::vector<math::Vec3> chunkHull;
    };

    // Head bursts as this machine shows them.
    // tuning:      the look.
    // assets:      made on first use.
    // isAssetsTried: making them was attempted.
    // gibs:        gibs lying around, oldest first.
    // fountains:   necks still spurting.
    // random:      sound picks and gib variation.
    struct HeadBursts
    {
        HeadBurstTuning tuning{};
        std::optional<HeadBurstAssets> assets{};
        bool isAssetsTried{false};
        std::vector<Gib> gibs;
        std::vector<NeckFountain> fountains;
        GoreRandom random{};
    };

    // Shows a head burst on an NPC: hides its head and head gear for good (the neck's wound stump
    // shows), pushes its ragdoll hard, and throws a big blood burst (droplets, particles, a
    // lingering mist), gibs, a wide splatter on the surfaces around and a splatter sound; the
    // neck spurts for a moment. A head already gone gets the blood only.
    // state:  the game state (gore, NPC looks, effects).
    // engine: Engine services.
    // assets: the blood assets.
    // look:   the NPC's look.
    // event:  the HeadBurst event (point and direction of the shot).
    // random: the event's seeded random.
    void showHeadBurst(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, const BloodAssets& assets,
        NpcLook& look, const GoreEvent& event, GoreRandom& random);

    // Lets necks spurt, moves and expires gibs, and queues the gibs for drawing.
    // state:   the game state.
    // engine:  Engine services.
    // alpha:   interpolation between the last two ticks.
    // seconds: World seconds since the last frame.
    void presentHeadBursts(lockstep::app::GameState& state, lockstep::app::EngineContext& engine, float alpha,
        float seconds);

    // Removes every gib and fountain (level switch, restart).
    // bursts:  the head bursts.
    // physics: destroys the gibs' bodies.
    void clearHeadBursts(HeadBursts& bursts, lockstep::physics::PhysicsWorld& physics);

} //namespace game

#endif //LOCKSTEP_GAME_GORE_HEAD_BURSTS_HPP
