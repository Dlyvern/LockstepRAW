#include "Lockstep/Anim/SkinnedModel.hpp"

#include "Lockstep/Render/Renderer.hpp"

#include <span>

namespace lockstep::anim
{
    Animator& SkinnedInstance::animator() noexcept
    {
        return m_animator;
    }

    const Animator& SkinnedInstance::animator() const noexcept
    {
        return m_animator;
    }

    void SkinnedInstance::update(const SkinnedModel& model, float deltaSeconds)
    {
        const std::size_t jointCount = model.skeleton.joints.size();
        m_hasPrevious = m_skin.size() == jointCount;
        m_previousSkin.swap(m_skin);
        m_joints.resize(jointCount);
        m_skin.resize(jointCount);

        m_animator.advance(model.clips, deltaSeconds);
        m_animator.evaluate(model.skeleton, model.clips, m_pose);
        computeJointMatrices(model.skeleton, m_pose, m_joints);
        computeSkinMatrices(model.skeleton, m_joints, m_skin);
        m_bounds = poseBounds(m_joints, m_skin, model.jointRadii);

        if (math::isEmpty(m_bounds))
        {
            m_bounds = model.bindBounds;
        }
    }

    void SkinnedInstance::submit(render::Renderer& renderer, const SkinnedModel& model, const math::Transform& transform,
        render::RenderLayers layers) const
    {
        if (m_skin.empty() || m_skin.size() != model.skeleton.joints.size())
        {
            return;
        }

        const std::span<const math::Mat4> previous = m_hasPrevious ? std::span<const math::Mat4>(m_previousSkin)
            : std::span<const math::Mat4>(m_skin);
        const render::JointPalette palette = renderer.submitJointPalette(m_skin, previous);

        for (const SkinnedModelPart& part : model.parts)
        {
            renderer.submitSkinned(part.mesh, part.material, transform, palette, m_bounds, layers);
        }
    }

    const Pose& SkinnedInstance::pose() const noexcept
    {
        return m_pose;
    }

    std::span<const math::Mat4> SkinnedInstance::jointMatrices() const noexcept
    {
        return m_joints;
    }

    const math::Aabb& SkinnedInstance::bounds() const noexcept
    {
        return m_bounds;
    }
} //namespace lockstep::anim
