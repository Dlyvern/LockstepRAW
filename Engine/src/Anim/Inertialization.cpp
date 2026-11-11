#include "Lockstep/Anim/Inertialization.hpp"

#include "Lockstep/Core/Rotation.hpp"
#include "Lockstep/Core/Spring.hpp"

#include <algorithm>

namespace lockstep::anim
{
    namespace
    {
        constexpr float k_settledHalflives{8.0f};

        math::Vec3 rotationDelta(const math::Quat& from, const math::Quat& to)
        {
            return math::toScaledAngleAxis(math::normalize(to * math::inverse(from)));
        }
    } //namespace

    void PoseInertializer::begin(const Pose& target, const Pose& targetBefore, float stepSeconds, float halflife)
    {
        const std::size_t count = target.joints.size();

        if (halflife <= 0.0f || m_shown.joints.size() != count || m_shownPositionVelocities.size() != count)
        {
            m_isActive = false;
            return;
        }

        const bool hasVelocity = stepSeconds > 0.0f && targetBefore.joints.size() == count;
        const float inverseStep = hasVelocity ? 1.0f / stepSeconds : 0.0f;
        m_positionOffsets.resize(count);
        m_positionVelocities.resize(count);
        m_rotationOffsets.resize(count);
        m_rotationVelocities.resize(count);

        for (std::size_t index = 0; index < count; ++index)
        {
            const math::Transform& shown = m_shown.joints[index];
            const math::Transform& wanted = target.joints[index];
            const math::Vec3 targetVelocity = hasVelocity
                ? (wanted.position - targetBefore.joints[index].position) * inverseStep : math::Vec3{0.0f, 0.0f, 0.0f};
            const math::Vec3 targetSpin = hasVelocity
                ? rotationDelta(targetBefore.joints[index].rotation, wanted.rotation) * inverseStep
                : math::Vec3{0.0f, 0.0f, 0.0f};

            m_positionOffsets[index] = shown.position - wanted.position;
            m_positionVelocities[index] = m_shownPositionVelocities[index] - targetVelocity;
            m_rotationOffsets[index] = rotationDelta(wanted.rotation, shown.rotation);
            m_rotationVelocities[index] = m_shownRotationVelocities[index] - targetSpin;
        }

        m_halflife = halflife;
        m_activeSeconds = 0.0f;
        m_isActive = true;
        m_isFresh = true;
    }

    void PoseInertializer::apply(Pose& pose, float deltaSeconds)
    {
        const float delta = std::max(deltaSeconds, 0.0f);

        if (m_isActive && m_positionOffsets.size() == pose.joints.size())
        {
            if (!m_isFresh)
            {
                decay(delta);
            }

            for (std::size_t index = 0; index < pose.joints.size(); ++index)
            {
                math::Transform& joint = pose.joints[index];
                joint.position = joint.position + m_positionOffsets[index];
                joint.rotation = math::normalize(math::fromScaledAngleAxis(m_rotationOffsets[index]) * joint.rotation);
            }
        }
        else
        {
            m_isActive = false;
        }

        m_isFresh = false;
        remember(pose, delta);
    }

    void PoseInertializer::reset()
    {
        m_isActive = false;
        m_isFresh = false;
        m_shown.joints.clear();
        m_shownPositionVelocities.clear();
        m_shownRotationVelocities.clear();
    }

    bool PoseInertializer::isActive() const noexcept
    {
        return m_isActive;
    }

    void PoseInertializer::decay(float deltaSeconds)
    {
        for (std::size_t index = 0; index < m_positionOffsets.size(); ++index)
        {
            math::decaySpring(m_positionOffsets[index], m_positionVelocities[index], m_halflife, deltaSeconds);
            math::decaySpring(m_rotationOffsets[index], m_rotationVelocities[index], m_halflife, deltaSeconds);
        }

        m_activeSeconds += deltaSeconds;
        m_isActive = m_activeSeconds < m_halflife * k_settledHalflives;
    }

    void PoseInertializer::remember(const Pose& pose, float deltaSeconds)
    {
        const std::size_t count = pose.joints.size();

        if (m_shown.joints.size() != count || m_shownPositionVelocities.size() != count)
        {
            m_shown = pose;
            m_shownPositionVelocities.assign(count, math::Vec3{0.0f, 0.0f, 0.0f});
            m_shownRotationVelocities.assign(count, math::Vec3{0.0f, 0.0f, 0.0f});
            return;
        }

        if (deltaSeconds <= 0.0f)
        {
            return;
        }

        const float inverseStep = 1.0f / deltaSeconds;

        for (std::size_t index = 0; index < count; ++index)
        {
            const math::Transform& before = m_shown.joints[index];
            const math::Transform& now = pose.joints[index];
            m_shownPositionVelocities[index] = (now.position - before.position) * inverseStep;
            m_shownRotationVelocities[index] = rotationDelta(before.rotation, now.rotation) * inverseStep;
        }

        m_shown = pose;
    }
} //namespace lockstep::anim
