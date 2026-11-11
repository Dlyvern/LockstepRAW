#ifndef LOCKSTEP_GAME_CLOTH_CURTAINS_HPP
#define LOCKSTEP_GAME_CLOTH_CURTAINS_HPP

#include "Audio/CombatAudio.hpp"
#include "Cloth/ClothPanel.hpp"

#include <Lockstep/Core/Bounds.hpp>
#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Render/Resources.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace lockstep::app
{
    struct EngineContext;
    struct GameState;
} //namespace lockstep::app

namespace lockstep::assets
{
    struct Scene;
} //namespace lockstep::assets

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

    // How curtains move, editable from the dev panel.
    // enabled:     simulate them (off, or with GraphicsQuality::cloth off, they hang still).
    // simulateDistance: metres from the camera within which a panel may move; farther ones rest.
    // personRadius: radius of the capsule people push the cloth with.
    // shotKick:    velocity in m/s a bullet passing through gives the cloth where it passes.
    // spacing:     metres between simulated particles.
    // shakeDrill:  kick every nearby panel every few seconds (--dev-shake-cloth, for captures).
    struct CurtainTuning
    {
        bool enabled{true};
        float simulateDistance{14.0f};
        float personRadius{0.32f};
        float shotKick{3.5f};
        float spacing{0.22f};
        bool shakeDrill{false};
    };

    // One curtain panel of the level (a `Cloth_*` node), replaced by a simulated sheet.
    // instance:  the static scene instance it replaces (hidden while the sheet is drawn).
    // transform: panel space in the world: origin at the top edge's centre, x across, y up, z the
    //            panel's normal; unit scale.
    // cloth:     the simulation.
    // mesh:      the pleated skinned mesh drawn for it (one joint per particle).
    // bounds:    panel-space bounds of the mesh with room to swing, for culling.
    // joints, previousJoints: this and last frame's skinning matrices while awake.
    // isAwake:   simulating (else drawn at rest with the shared rest palette).
    // calmSeconds: how long it has barely moved.
    // isOutdoors: outside every interior box (feels the wind).
    struct CurtainPanel
    {
        uint32_t instance{0};
        lockstep::math::Transform transform{};
        ClothPanel cloth{};
        lockstep::render::MeshHandle mesh{};
        lockstep::math::Aabb bounds{};
        std::vector<lockstep::math::Mat4> joints{};
        std::vector<lockstep::math::Mat4> previousJoints{};
        bool isAwake{false};
        float calmSeconds{0.0f};
        bool isOutdoors{false};
    };

    // The level's curtains: presentation only, never replicated.
    // scene:       the level they were built for, and its instance count (a reloaded level may reuse
    //              the address).
    // material:    the shared skinned fabric material, once created.
    // panels:      the panels.
    // restPalette: identity matrices, enough for the largest panel.
    // stepCarry:   World time not yet simulated (fixed steps).
    // drillSeconds: World time since the shake drill last kicked.
    // tuning:      switches and sizes.
    struct Curtains
    {
        const lockstep::assets::Scene* scene{nullptr};
        std::size_t instanceCount{0};
        lockstep::render::MaterialHandle material{};
        std::vector<CurtainPanel> panels{};
        std::vector<lockstep::math::Mat4> restPalette{};
        float stepCarry{0.0f};
        float drillSeconds{0.0f};
        CurtainTuning tuning{};
    };

    // A bullet's path this frame, from where it was fired to where it stopped.
    struct BulletPath
    {
        lockstep::math::Vec3 from{0.0f, 0.0f, 0.0f};
        lockstep::math::Vec3 to{0.0f, 0.0f, 0.0f};
    };

    // What the curtains read each frame.
    // scene:     the loaded level, or null.
    // eye:       the camera position (only panels near it move).
    // feet:      foot positions of the people who can walk through curtains.
    // shots:     the local player's bullet paths this frame.
    // interiors: the level's interior boxes (no wind inside).
    // wind:      the wind velocity in m/s.
    // simulate:  cloth is allowed (GraphicsQuality::cloth).
    // worldSeconds: World time since the previous frame.
    struct CurtainFrame
    {
        const lockstep::assets::Scene* scene{nullptr};
        lockstep::math::Vec3 eye{0.0f, 0.0f, 0.0f};
        std::span<const lockstep::math::Vec3> feet{};
        std::span<const BulletPath> shots{};
        std::span<const InteriorBox> interiors{};
        lockstep::math::Vec3 wind{0.0f, 0.0f, 0.0f};
        bool simulate{true};
        float worldSeconds{0.0f};
    };

    // Removes the colliders of a level's cloth panels (instances under a node named "Cloth_*",
    // case-insensitive), so people walk through curtains. Part of loading a level on every machine,
    // whatever the graphics quality, so host and client collide alike.
    // level:   the loaded level.
    // physics: the physics world holding its colliders.
    // Returns: the number of colliders removed.
    uint32_t removeClothColliders(SceneLevel& level, lockstep::physics::PhysicsWorld& physics);

    // Builds the curtains when the level changed (hiding the static panels it replaces), steps the
    // ones near the camera (people push through them, the local player's bullets kick them, wind
    // moves the ones outdoors), lets settled ones rest and draws every panel.
    // curtains: the curtain state.
    // renderer: creates meshes and the material, and draws them.
    // scene:    the loaded level's scene (instances are hidden), or null.
    // frame:    this frame's people, shots and time.
    void presentCurtains(Curtains& curtains, lockstep::render::Renderer& renderer, lockstep::assets::Scene* scene,
        const CurtainFrame& frame);

    // presentCurtains() for the game: people are the drawn players and the armed NPCs, shots the local
    // player's bullet paths from the eye to each impact combat audio found this frame. Call after
    // the combat audio update.
    // state:  the game state (curtains, level, weather, combat audio).
    // engine: the engine (renderer, quality).
    // audio:  this frame's combat audio input (who is where).
    // worldSeconds: World time since the previous frame.
    void presentLevelCurtains(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const CombatAudioFrame& audio, float worldSeconds);

#if LOCKSTEP_DEV_TOOLS
    // Draws the curtain section of the Game dev panel.
    // curtains: the curtain state.
    void curtainDevSection(Curtains& curtains);
#endif
} //namespace game

#endif //LOCKSTEP_GAME_CLOTH_CURTAINS_HPP
