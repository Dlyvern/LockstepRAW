#ifndef LOCKSTEP_GAME_EFFECTS_GLASS_SHATTER_HPP
#define LOCKSTEP_GAME_EFFECTS_GLASS_SHATTER_HPP

#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Render/Particles.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lockstep::app
{
    struct EngineContext;
    struct GameState;
} //namespace lockstep::app

namespace game
{
    // Number of floor shard shapes.
    inline constexpr std::size_t k_glassShardShapes{3};

    // Most shards lying on the floor at once (the oldest are reused).
    inline constexpr std::size_t k_maxGlassDebris{320};

    // One shard lying on the floor.
    // transform: where it lies (flat, scaled to its size).
    // shape:     which shard mesh.
    struct GlassDebris
    {
        lockstep::math::Transform transform{};
        uint8_t shape{0};
    };

    // How breaking glass looks and sounds on this machine (presentation only, never replicated):
    // shards flying in the shot's direction and falling, glints, a puff of glass dust, shards left
    // on the floor below and the crash. Spawned from BreakableGlass::shown on both machines.
    // flying, falling: shard particles thrown along the shot and dropping off the frame.
    // glints:      tiny additive sparkles that catch the light.
    // dust:        a faint puff of powdered glass.
    // isTried:     the particle effects and meshes were attempted (not retried after a failure).
    // shardMeshes: flat floor shard shapes (unit size, facing +Y).
    // shardLook:   glossy, slightly see-through glass for floor shards.
    // hasDebrisAssets: the shard meshes and material exist.
    // debris:      shards on the floor, at most k_maxGlassDebris.
    // nextDebris:  the slot the next shard takes once `debris` is full.
    // random:      presentation-only random state.
    struct GlassShatter
    {
        lockstep::render::ParticleEffectId flying{};
        lockstep::render::ParticleEffectId falling{};
        lockstep::render::ParticleEffectId glints{};
        lockstep::render::ParticleEffectId dust{};
        bool isTried{false};
        std::array<lockstep::render::MeshHandle, k_glassShardShapes> shardMeshes{};
        lockstep::render::MaterialHandle shardLook{};
        bool hasDebrisAssets{false};
        std::vector<GlassDebris> debris{};
        std::size_t nextDebris{0};
        uint32_t random{0x2F6B1D93u};
    };

    // Shows the glass breaks this machine has not shown yet (state.glass.shown, cleared) and
    // draws the shards on the floor. Call from frame() before presentEffects() (which simulates
    // and submits the particles).
    // state:  the game state (glass, effects, combat sounds).
    // engine: the engine (renderer, physics for the floor below a pane, audio).
    void presentGlassShatter(lockstep::app::GameState& state, lockstep::app::EngineContext& engine);

    // Removes the floor shards, e.g. when a level loads.
    // shatter: the glass presentation state.
    void clearGlassDebris(GlassShatter& shatter);
} //namespace game

#endif //LOCKSTEP_GAME_EFFECTS_GLASS_SHATTER_HPP
