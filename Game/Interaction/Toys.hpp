#ifndef LOCKSTEP_GAME_INTERACTION_TOYS_HPP
#define LOCKSTEP_GAME_INTERACTION_TOYS_HPP

#include "Level/SceneLevel.hpp"
#include "Player/Pawns.hpp"

#include <Lockstep/Core/Math.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Net/Replication.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>
#include <Lockstep/World/Entities.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
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
    // One drawn piece of a toy: a scene instance's mesh, placed relative to the toy's centre.
    // instance: index into Scene::instances whose mesh and material are drawn.
    // offset:   the piece's transform in the toy's frame.
    struct ToyPart
    {
        uint32_t instance{0};
        math::Transform offset{};
    };

    // A ball players kick, pick up and throw ("Toy_*" nodes with "ball" in the name). The host
    // simulates it as a dynamic sphere in the World time domain (bullet-time slows it) and
    // replicates it as an entity tagged Archetype::Toy + its index; while a player carries it, it
    // has no body and follows their hands. Both machines hide the level's instances of the toy
    // and draw it from its entity instead, interpolated.
    // name:         the node name, or "Toy_Basketball" for the ball made in code.
    // label:        what the prompt calls it ("ball").
    // parts:        what is drawn.
    // radius:       the sphere's radius, in metres.
    // rest:         where its centre starts (and returns to if it falls out of the level).
    // body:         host: its dynamic body; null while carried.
    // entity:       host: its entity; client: the mirror entity of it, null until it arrives.
    // kickCooldown: host: World seconds until the next kick can push it.
    struct Toy
    {
        std::string name;
        std::string label;
        std::vector<ToyPart> parts;
        float radius{0.1f};
        math::Vec3 rest{0.0f, 0.0f, 0.0f};
        lockstep::physics::BodyId body{};
        lockstep::world::EntityId entity{};
        float kickCooldown{0.0f};
    };

    // A player who may move a toy this tick (host).
    // foot:       their pawn's foot position.
    // velocity:   their pawn's velocity this tick, in m/s.
    // eye:        their eyes.
    // direction:  unit look direction.
    // controller: their pawn's controller.
    struct ToyPlayer
    {
        math::Vec3 foot{0.0f, 0.0f, 0.0f};
        math::Vec3 velocity{0.0f, 0.0f, 0.0f};
        math::Vec3 eye{0.0f, 0.0f, 0.0f};
        math::Vec3 direction{0.0f, 0.0f, -1.0f};
        lockstep::physics::ControllerId controller{};
    };

    // Finds the level's balls (at most k_maxToys). A level with a basketball court ("Court*"
    // nodes) but no "Toy_Basketball" node gets a basketball made in code, dropped onto the middle
    // of the court (the centre of the court pieces' bounds, just above the lowest piece's top),
    // drawn with the meshes of the first ball found scaled to basketball size.
    // scene:   the level's scene.
    // Returns: the toys, in node order (the made basketball last); the same on host and client.
    [[nodiscard]] std::vector<Toy> findToys(const lockstep::assets::Scene& scene);

    // Hides the level's instances of the toys and removes their static colliders.
    // toys:    the toys.
    // level:   the level.
    // physics: the physics world of the level's colliders.
    void hideToyInstances(std::span<const Toy> toys, SceneLevel& level, lockstep::physics::PhysicsWorld& physics);

    // Host: creates each toy's body and replicated entity at its rest position.
    // toys:        the toys; bodies and entities are set.
    // entities:    the entity store.
    // replication: tags the entities.
    // physics:     creates the bodies.
    // Returns:     success, or the first creation error.
    [[nodiscard]] lockstep::Status spawnToys(std::vector<Toy>& toys, lockstep::world::Entities& entities,
        lockstep::net::Replication& replication, lockstep::physics::PhysicsWorld& physics);

    // Client: points each toy at its mirror entity, by archetype.
    // toys:     the toys.
    // mirrored: the entities mirrored from the host this tick.
    void bindMirroredToys(std::vector<Toy>& toys, std::span<const lockstep::net::MirroredEntity> mirrored);

    // Which toy a player looks at within reach: a ray hitting the toy's body (host) or entering its
    // sphere before anything else in the way (client, which has no toy bodies).
    // toys:      the toys.
    // entities:  the entity store holding the toys' entities.
    // physics:   for the raycast.
    // heldToy:   per player, the toy they carry or k_nobody (carried toys are skipped).
    // eye:       ray start.
    // direction: unit look direction.
    // reach:     longest distance.
    // ignore:    the looking pawn's controller.
    // Returns:   the toy's index, or nothing.
    [[nodiscard]] std::optional<std::size_t> traceToy(std::span<const Toy> toys,
        const lockstep::world::Entities& entities, const lockstep::physics::PhysicsWorld& physics,
        const std::array<uint8_t, k_playerCount>& heldToy, const math::Vec3& eye, const math::Vec3& direction, float reach, lockstep::physics::ControllerId ignore);

    // Host: a player picks a toy up; its body goes away until it is released.
    // toy:     the toy.
    // physics: destroys the body.
    void pickUpToy(Toy& toy, lockstep::physics::PhysicsWorld& physics);

    // Host: a carried toy leaves the player's hands: a new body where it is, moving at `velocity`.
    // toy:      the toy.
    // entities: the toy's entity gives where it is.
    // physics:  creates the body.
    // velocity: its starting velocity, in m/s.
    // Returns:  success, or the body creation error (the toy then stays carried).
    [[nodiscard]] lockstep::Status releaseToy(Toy& toy, const lockstep::world::Entities& entities,
        lockstep::physics::PhysicsWorld& physics, const math::Vec3& velocity);

    // Where a player holds a toy: in front of the eyes, pulled in before walls.
    // toy:     the toy.
    // physics: for the wall check.
    // player:  the holder.
    // Returns: the toy's centre.
    [[nodiscard]] math::Vec3 holdPoint(const Toy& toy, const lockstep::physics::PhysicsWorld& physics,
        const ToyPlayer& player);

    // Host, once per tick after the pawns moved: carried toys follow their holder, free toys copy
    // their body into their entity, roll to a stop (stopped dead once slow on the ground, so they
    // don't creep), get kicked by players walking into them, and return to their rest position if
    // they fell out of the level.
    // toys:       the toys.
    // heldToy:    per player, the toy index they carry or k_nobody (RoomState::heldToy).
    // players:    per player, their pawn this tick, or nothing while absent.
    // entities:   the entity store.
    // physics:    the physics world.
    // worldDelta: World domain delta of this tick.
    void updateToysAsHost(std::vector<Toy>& toys, const std::array<uint8_t, k_playerCount>& heldToy,
        const std::array<std::optional<ToyPlayer>, k_playerCount>& players, lockstep::world::Entities& entities,
        lockstep::physics::PhysicsWorld& physics, float worldDelta);

    // Queues the toys for drawing at their interpolated entity transforms.
    // renderer:   the renderer.
    // scene:      the level's scene (meshes and materials of the parts).
    // toys:       the toys.
    // entities:   the entity store holding the toys' entities.
    // alpha:      FrameInfo::alpha.
    // localHeld:  the toy the local player carries and where to show it (in their hands, without
    //             the replication delay), or nothing.
    void drawToys(lockstep::render::Renderer& renderer, const lockstep::assets::Scene& scene, std::span<const Toy> toys,
        const lockstep::world::Entities& entities, float alpha,
        std::optional<std::pair<std::size_t, math::Vec3>> localHeld);
} //namespace game

#endif //LOCKSTEP_GAME_INTERACTION_TOYS_HPP
