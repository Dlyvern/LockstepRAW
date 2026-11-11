#ifndef LOCKSTEP_ANIM_SKINNED_MODEL_HPP
#define LOCKSTEP_ANIM_SKINNED_MODEL_HPP

#include "Lockstep/Anim/AnimationClip.hpp"
#include "Lockstep/Anim/Animator.hpp"
#include "Lockstep/Anim/DrivenPose.hpp"
#include "Lockstep/Anim/Limbs.hpp"
#include "Lockstep/Anim/Skeleton.hpp"
#include "Lockstep/Core/Bounds.hpp"
#include "Lockstep/Core/Transform.hpp"
#include "Lockstep/Render/CharacterSurface.hpp"
#include "Lockstep/Render/Resources.hpp"
#include "Lockstep/Render/Views.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace lockstep::anim
{
    // One drawable part of a skinned model: a skinned GPU mesh and its skinned material.
    // mesh:         the GPU mesh.
    // material:     the GPU material.
    // materialName: the source material's name (e.g. "HeadGear_Helmet"), so Game can pick parts
    //               to hide; empty when the source had none (or a model cooked before names
    //               were kept).
    // limb:         the limb whose region or cut it belongs to (see Limbs.hpp); None for Whole.
    // role:         what it is in the limb split; Whole for a model cooked without limbs.
    struct SkinnedModelPart
    {
        render::MeshHandle mesh{};
        render::MaterialHandle material{};
        std::string materialName;
        Limb limb{Limb::None};
        LimbPartRole role{LimbPartRole::Whole};
    };

    // A set of a model's parts for SkinnedInstance::submit(): bit i stands for parts[i]. Parts
    // past the 64th are always drawn.
    using PartMask = uint64_t;

    // Every part of a model.
    inline constexpr PartMask k_allParts{~PartMask{0}};

    // A loaded skinned model (a glTF skin with the meshes it deforms and the clips that animate
    // it). GPU resources are owned by the Renderer; this holds handles. Shared by every instance.
    // name:        the skin's name, or its mesh node's.
    // skeleton:    the joints.
    // parts:       skinned meshes drawn with the skeleton's palette.
    // clips:       animations of the skeleton.
    // jointRadii:  per joint, the reach of the vertices it moves (see poseBounds()).
    // bindBounds:  model-space bounds in bind pose.
    // limbs:       the limbs of a model cooked with --limbs (every limb but the torso), else
    //              empty (see Limbs.hpp).
    struct SkinnedModel
    {
        std::string name;
        Skeleton skeleton;
        std::vector<SkinnedModelPart> parts;
        std::vector<AnimationClip> clips;
        std::vector<float> jointRadii;
        math::Aabb bindBounds{};
        std::vector<LimbInfo> limbs;
    };

    // One animated copy of a SkinnedModel: its Animator, current pose, and the skinning matrices
    // of this and the previous update (the previous ones give skinned draws motion vectors for
    // TAA). Presentation only: Game derives what it plays from replicated state; nothing here is
    // replicated. Holds no pointer to the model, so it can live in Game state.
    class SkinnedInstance
    {
    public:
        // Returns: the animator, to play clips and set the blend parameter.
        [[nodiscard]] Animator& animator() noexcept;

        // Returns: the animator, read-only.
        [[nodiscard]] const Animator& animator() const noexcept;

        // Advances the animator and computes the pose, joint matrices, skinning matrices and
        // bounds. Call once per presentation frame before submit().
        // model:        the model this instance animates (always the same one).
        // deltaSeconds: elapsed presentation time.
        void update(const SkinnedModel& model, float deltaSeconds);

        // Like update(), with rotations added on top of the animated pose (see
        // computeOffsetJointMatrices()), e.g. procedural hit reactions. pose() stays the animated
        // pose; the joint and skinning matrices include the offsets.
        // model:        the model this instance animates (always the same one).
        // deltaSeconds: elapsed presentation time.
        // offsets:      the added rotations; empty for none.
        void update(const SkinnedModel& model, float deltaSeconds, std::span<const JointRotationOffset> offsets);

        // Starts driving the pose from outside (e.g. a ragdoll): later updateDriven() calls fade
        // from the current joint matrices into the driven pose. update() may be called again
        // afterwards to go back to the animator.
        // blendSeconds: length of the fade; 0 switches at once.
        void beginDriven(float blendSeconds = 0.1f);

        // Computes the pose from world-placed joints instead of the animator, fading from the
        // pose at beginDriven() while the blend lasts. Joints that are not driven keep the local
        // transforms of the last animated pose. Call once per presentation frame before submit().
        // model:        the model this instance animates.
        // placement:    the world transform later passed to submit().
        // driven:       the world-placed joints (e.g. ragdoll bones).
        // deltaSeconds: elapsed presentation time (advances the fade).
        void updateDriven(const SkinnedModel& model, const math::Transform& placement,
            std::span<const DrivenJoint> driven, float deltaSeconds);

        // Queues the model's parts for the next render() with this instance's pose.
        // Does nothing before the first update().
        // renderer:  the renderer the model was loaded with.
        // model:     the model passed to update().
        // transform: world placement of the model.
        // layers:    render layers the draws are on.
        // parts:     which parts to draw (e.g. without a helmet); all by default.
        // surface:   the character surface (blood, dirt, rain) drawn over every part; see
        //            Renderer::submitCharacterSurface().
        void submit(render::Renderer& renderer, const SkinnedModel& model, const math::Transform& transform,
            render::RenderLayers layers = render::k_defaultRenderLayer, PartMask parts = k_allParts,
            render::CharacterSurfaceId surface = render::k_noCharacterSurface) const;

        // Maps a point on or near the posed body back into the bind pose, through the joint
        // nearest to it, e.g. to place a render::SurfaceStain where a bullet hit.
        // modelPoint: the point in model space (world point times the inverse of the placement).
        // Returns:    the point in the bind pose, or `modelPoint` before the first update().
        [[nodiscard]] math::Vec3 bindPosition(const math::Vec3& modelPoint) const;

        // Returns: the current pose (local joint transforms).
        [[nodiscard]] const Pose& pose() const noexcept;

        // Returns: model-space joint matrices of the current pose, e.g. to attach a weapon to a
        //          hand (multiply by the instance's world matrix for world space).
        [[nodiscard]] std::span<const math::Mat4> jointMatrices() const noexcept;

        // Returns: skinning matrices of the current pose (model-space joint matrix times inverse
        //          bind), e.g. to freeze a severed limb in this pose.
        [[nodiscard]] std::span<const math::Mat4> skinMatrices() const noexcept;

        // Returns: model-space bounds of the current pose.
        [[nodiscard]] const math::Aabb& bounds() const noexcept;

    private:
        void beginUpdate(const SkinnedModel& model);
        void finishUpdate(const SkinnedModel& model);

        Animator m_animator{};
        Pose m_pose{};
        std::vector<math::Mat4> m_joints;
        std::vector<math::Mat4> m_skin;
        std::vector<math::Mat4> m_previousSkin;
        math::Aabb m_bounds{};
        bool m_hasPrevious{false};
        std::vector<math::Mat4> m_blendFrom;
        float m_blendSeconds{0.0f};
        float m_drivenSeconds{0.0f};
    };
} //namespace lockstep::anim

#endif //LOCKSTEP_ANIM_SKINNED_MODEL_HPP
