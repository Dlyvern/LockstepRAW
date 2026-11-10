#ifndef LOCKSTEP_GAME_GORE_SEVERED_LIMBS_HPP
#define LOCKSTEP_GAME_GORE_SEVERED_LIMBS_HPP

#include <Lockstep/Anim/SkinnedModel.hpp>
#include <Lockstep/Core/Bounds.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Core/Transform.hpp>
#include <Lockstep/Physics/PhysicsTypes.hpp>

#include <cstdint>
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
    namespace math = lockstep::math;

    // A severed limb flying or lying around: the limb's parts of the character model frozen in
    // the pose it was cut in, carried by a dynamic convex debris body. Presentation only; each
    // machine simulates its own.
    // model:      the character model it was cut from (lives as long as the game).
    // skin:       the frozen skinning matrices.
    // parts:      the parts drawn (the limb's regions, its piece cap, stumps of limbs cut off it
    //             earlier).
    // body:       its debris body.
    // bodyStart:  the body's placement when it was cut.
    // modelStart: the model's placement when it was cut.
    // previous, current: the body's last two placements seen, for interpolation.
    // bounds:     model-space bounds of the frozen limb.
    // cut:        model-space point of the cut (the limb's root joint), where it bleeds.
    // seconds:    World seconds since it was cut.
    // lastSmear:  where it last left a smear while sliding.
    // drip:       World seconds until it drips again.
    struct SeveredPiece
    {
        const lockstep::anim::SkinnedModel* model{nullptr};
        std::vector<math::Mat4> skin;
        lockstep::anim::PartMask parts{0};
        lockstep::physics::BodyId body{};
        math::Transform bodyStart{};
        math::Transform modelStart{};
        math::Transform previous{};
        math::Transform current{};
        math::Aabb bounds{};
        math::Vec3 cut{0.0f, 0.0f, 0.0f};
        float seconds{0.0f};
        math::Vec3 lastSmear{0.0f, 0.0f, 0.0f};
        float drip{0.0f};
    };

    // The character a limb is cut from, as it was last drawn.
    // model:     its model (with limbs).
    // instance:  its pose.
    // transform: the model's world placement.
    // gearParts: the head gear parts it is drawn with (see gearMask()).
    // severed:   limbs cut off it before.
    struct SeverSource
    {
        const lockstep::anim::SkinnedModel& model;
        const lockstep::anim::SkinnedInstance& instance;
        const math::Transform& transform;
        lockstep::anim::PartMask gearParts{lockstep::anim::k_allParts};
        lockstep::anim::LimbMask severed{0};
    };

    // How the piece leaves the body.
    // linear:  starting velocity, m/s.
    // angular: starting spin, rad/s.
    struct SeverThrow
    {
        math::Vec3 linear{0.0f, 0.0f, 0.0f};
        math::Vec3 angular{0.0f, 0.0f, 0.0f};
    };

    // The parts to draw of a character missing some limbs: its gear parts without the severed
    // limbs' regions (and the limbs hanging from them) or any wound cap, plus the stump cap of
    // each outermost cut. A model without limbs gets `gearParts` back.
    // model:     the character model.
    // gearParts: the parts its head gear look draws (wound caps may be in it; they are removed).
    // severed:   limbs cut off.
    // Returns:   the parts to draw.
    [[nodiscard]] lockstep::anim::PartMask severedBodyParts(const lockstep::anim::SkinnedModel& model,
        lockstep::anim::PartMask gearParts, lockstep::anim::LimbMask severed);

    // source:  the character.
    // limb:    a limb.
    // Returns: true if the limb can be cut off: the model has limbs, the limb is not the torso
    //          and neither it nor a limb it hangs from is already gone.
    [[nodiscard]] bool canSever(const SeverSource& source, lockstep::anim::Limb limb);

    // Cuts a limb off a character: creates the piece's debris body (convex hull of the limb's
    // posed hull points, mass from its volume) and freezes its pose. The caller adds the limb to
    // the character's severed limbs.
    // physics: the physics world.
    // source:  the character (canSever() must be true).
    // limb:    the limb.
    // motion:  how it leaves.
    // Returns: the piece, or an error if the hull or the body could not be made.
    [[nodiscard]] lockstep::Result<SeveredPiece> severLimb(lockstep::physics::PhysicsWorld& physics,
        const SeverSource& source, lockstep::anim::Limb limb, const SeverThrow& motion);

    // Reads the piece's body placement after a physics step.
    // piece:   the piece.
    // physics: the physics world.
    void trackPiece(SeveredPiece& piece, const lockstep::physics::PhysicsWorld& physics);

    // piece:   a piece.
    // alpha:   0 for the previous body placement .. 1 for the current one.
    // Returns: the world placement of the piece's model space.
    [[nodiscard]] math::Transform pieceTransform(const SeveredPiece& piece, float alpha);

    // Queues a piece's parts with its frozen pose.
    // renderer: the renderer.
    // piece:    the piece.
    // alpha:    interpolation between the body's last two placements.
    void drawPiece(lockstep::render::Renderer& renderer, const SeveredPiece& piece, float alpha);

    // Destroys a piece's debris body.
    // physics: the physics world.
    // piece:   the piece.
    void releasePiece(lockstep::physics::PhysicsWorld& physics, SeveredPiece& piece);
} //namespace game

#endif //LOCKSTEP_GAME_GORE_SEVERED_LIMBS_HPP
