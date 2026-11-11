#include "Lockstep/Anim/SkinnedModel.hpp"

#include "Lockstep/Render/Renderer.hpp"

#include <algorithm>
#include <limits>
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
        beginUpdate(model);
        m_animator.advance(model.clips, deltaSeconds);
        m_animator.evaluate(model.skeleton, model.clips, m_pose);
        computeJointMatrices(model.skeleton, m_pose, m_joints);
        finishUpdate(model);
    }

    void SkinnedInstance::update(const SkinnedModel& model, float deltaSeconds,
        std::span<const JointRotationOffset> offsets)
    {
        beginUpdate(model);
        m_animator.advance(model.clips, deltaSeconds);
        m_animator.evaluate(model.skeleton, model.clips, m_pose);
        computeOffsetJointMatrices(model.skeleton, m_pose, offsets, m_joints);
        finishUpdate(model);
    }

    void SkinnedInstance::beginDriven(float blendSeconds)
    {
        m_blendFrom = m_joints;
        m_blendSeconds = std::max(blendSeconds, 0.0f);
        m_drivenSeconds = 0.0f;
    }

    void SkinnedInstance::updateDriven(const SkinnedModel& model, const math::Transform& placement,
        std::span<const DrivenJoint> driven, float deltaSeconds)
    {
        beginUpdate(model);

        if (m_pose.joints.size() != model.skeleton.joints.size())
        {
            resetToRest(model.skeleton, m_pose);
        }

        computeDrivenJointMatrices(model.skeleton, m_pose, placement, driven, m_joints);
        m_drivenSeconds += std::max(deltaSeconds, 0.0f);
        const bool isBlending = m_drivenSeconds < m_blendSeconds && m_blendFrom.size() == m_joints.size();

        if (isBlending)
        {
            const float weight = m_drivenSeconds / m_blendSeconds;

            for (std::size_t index = 0; index < m_joints.size(); ++index)
            {
                m_joints[index] = blendJointMatrix(m_blendFrom[index], m_joints[index], weight);
            }
        }

        finishUpdate(model);
    }

    void SkinnedInstance::beginUpdate(const SkinnedModel& model)
    {
        const std::size_t jointCount = model.skeleton.joints.size();
        m_hasPrevious = m_skin.size() == jointCount;
        m_previousSkin.swap(m_skin);
        m_joints.resize(jointCount);
        m_skin.resize(jointCount);
    }

    void SkinnedInstance::finishUpdate(const SkinnedModel& model)
    {
        computeSkinMatrices(model.skeleton, m_joints, m_skin);
        m_bounds = poseBounds(m_joints, m_skin, model.jointRadii);

        if (math::isEmpty(m_bounds))
        {
            m_bounds = model.bindBounds;
        }
    }

    void SkinnedInstance::submit(render::Renderer& renderer, const SkinnedModel& model, const math::Transform& transform,
        render::RenderLayers layers, PartMask parts, render::CharacterSurfaceId surface) const
    {
        if (m_skin.empty() || m_skin.size() != model.skeleton.joints.size())
        {
            return;
        }

        const std::span<const math::Mat4> previous = m_hasPrevious ? std::span<const math::Mat4>(m_previousSkin)
            : std::span<const math::Mat4>(m_skin);
        const render::JointPalette palette = renderer.submitJointPalette(m_skin, previous);

        for (std::size_t index = 0; index < model.parts.size(); ++index)
        {
            const bool isDrawn = index >= std::numeric_limits<PartMask>::digits || ((parts >> index) & 1u) != 0;

            if (isDrawn)
            {
                renderer.submitSkinned(model.parts[index].mesh, model.parts[index].material, transform, palette,
                    m_bounds, layers, surface);
            }
        }
    }

    math::Vec3 SkinnedInstance::bindPosition(const math::Vec3& modelPoint) const
    {
        if (m_joints.empty() || m_skin.size() != m_joints.size())
        {
            return modelPoint;
        }

        std::size_t nearest = 0;
        float nearestDistance = std::numeric_limits<float>::max();

        for (std::size_t joint = 0; joint < m_joints.size(); ++joint)
        {
            const math::Vec3 position{m_joints[joint][3][0], m_joints[joint][3][1], m_joints[joint][3][2]};
            const float distance = math::lengthSquared(position - modelPoint);

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearest = joint;
            }
        }

        return math::transformPoint(math::inverse(m_skin[nearest]), modelPoint);
    }

    const Pose& SkinnedInstance::pose() const noexcept
    {
        return m_pose;
    }

    std::span<const math::Mat4> SkinnedInstance::jointMatrices() const noexcept
    {
        return m_joints;
    }

    std::span<const math::Mat4> SkinnedInstance::skinMatrices() const noexcept
    {
        return m_skin;
    }

    const math::Aabb& SkinnedInstance::bounds() const noexcept
    {
        return m_bounds;
    }
} //namespace lockstep::anim
