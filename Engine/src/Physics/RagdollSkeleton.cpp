#include "Lockstep/Physics/RagdollSkeleton.hpp"

#include <algorithm>
#include <string_view>

namespace lockstep::physics
{
    namespace
    {
        constexpr float k_minScale{0.25f};
        constexpr float k_maxScale{4.0f};
        constexpr float k_fallbackBoneLength{0.1f};
        constexpr float k_minLength{1.0e-5f};
        constexpr math::Vec3 k_forward{0.0f, 0.0f, 1.0f};
        constexpr math::Vec3 k_back{0.0f, 0.0f, -1.0f};
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};

        bool matchesJoint(std::string_view name, std::string_view bone)
        {
            if (name == bone)
            {
                return true;
            }

            if (name.size() <= bone.size() || name.substr(name.size() - bone.size()) != bone)
            {
                return false;
            }

            const char separator = name[name.size() - bone.size() - 1];
            return separator == ':' || separator == '_';
        }

        uint32_t findOneJoint(const anim::Skeleton& skeleton, std::string_view bone)
        {
            for (uint32_t index = 0; index < skeleton.joints.size(); ++index)
            {
                if (matchesJoint(skeleton.joints[index].name, bone))
                {
                    return index;
                }
            }

            return anim::k_noJoint;
        }

        uint32_t findProfileJoint(const anim::Skeleton& skeleton, std::string_view names)
        {
            while (!names.empty())
            {
                const std::size_t bar = names.find('|');
                const uint32_t joint = findOneJoint(skeleton, names.substr(0, bar));

                if (joint != anim::k_noJoint || bar == std::string_view::npos)
                {
                    return joint;
                }

                names.remove_prefix(bar + 1);
            }

            return anim::k_noJoint;
        }

        math::Vec3 positionOf(const math::Mat4& matrix)
        {
            return matrix.getColumn(3).xyz();
        }

        math::Transform frameOf(const math::Mat4& matrix)
        {
            math::Transform frame{};
            math::Vec3 scale{1.0f, 1.0f, 1.0f};

            if (!math::decompose(matrix, frame.position, frame.rotation, scale))
            {
                frame.position = positionOf(matrix);
            }

            frame.scale = math::Vec3{1.0f, 1.0f, 1.0f};
            return frame;
        }

        math::Vec3 directionOr(const math::Vec3& vector, const math::Vec3& fallback)
        {
            const float length = math::length(vector);
            return length > k_minLength ? vector / length : fallback;
        }

        RagdollJointLimits limitsInDegrees(float twist, float bendMin, float bendMax, float side)
        {
            return RagdollJointLimits{.twist = math::radians(twist),
                .bendMin = math::radians(bendMin),
                .bendMax = math::radians(bendMax),
                .side = math::radians(side)};
        }

        Error invalid(std::string message)
        {
            return Error{ErrorCode::InvalidArgument, "describeSkeletonRagdoll: " + std::move(message)};
        }

        struct CharacterAxes
        {
            math::Vec3 left{1.0f, 0.0f, 0.0f};
            math::Vec3 up{0.0f, 1.0f, 0.0f};
            math::Vec3 forward{0.0f, 0.0f, 1.0f};
            float scale{1.0f};

            math::Vec3 toRest(const math::Vec3& character) const
            {
                return left * character.x + up * character.y + forward * character.z;
            }
        };

        Result<CharacterAxes> characterAxes(const anim::Skeleton& skeleton, const RagdollProfile& profile,
            std::span<const math::Mat4> bind)
        {
            const uint32_t left = findProfileJoint(skeleton, profile.leftMarker);
            const uint32_t right = findProfileJoint(skeleton, profile.rightMarker);
            const uint32_t top = findProfileJoint(skeleton, profile.topMarker);
            const uint32_t root = findProfileJoint(skeleton, profile.bones.front().bone);

            if (left == anim::k_noJoint || right == anim::k_noJoint || top == anim::k_noJoint || root == anim::k_noJoint)
            {
                return invalid("the skeleton lacks a marker joint or the root bone");
            }

            CharacterAxes axes{};
            axes.left = directionOr(positionOf(bind[left]) - positionOf(bind[right]), axes.left);
            const math::Vec3 torso = positionOf(bind[top]) - positionOf(bind[root]);
            axes.scale = std::clamp(math::length(torso) / profile.referenceTorso, k_minScale, k_maxScale);
            axes.up = directionOr(torso - axes.left * math::dot(torso, axes.left), axes.up);
            axes.forward = math::cross(axes.left, axes.up);
            return axes;
        }

        Result<uint32_t> requiredJoint(const anim::Skeleton& skeleton, const std::string& bone)
        {
            uint32_t joint = findProfileJoint(skeleton, bone);

            if (joint == anim::k_noJoint)
            {
                return invalid("the skeleton has no joint '" + bone + "'");
            }

            return joint;
        }

        Result<uint32_t> parentBody(const RagdollProfile& profile, std::size_t body)
        {
            const std::string& parent = profile.bones[body].parent;

            if (body == 0)
            {
                if (!parent.empty())
                {
                    return invalid("the first profile bone must have no parent");
                }

                return uint32_t{k_noRagdollBody};
            }

            for (std::size_t earlier = 0; earlier < body; ++earlier)
            {
                if (profile.bones[earlier].bone == parent)
                {
                    return static_cast<uint32_t>(earlier);
                }
            }

            return invalid("profile bone '" + profile.bones[body].bone + "' has no earlier parent '" + parent + "'");
        }

        Result<RagdollBodyDescription> describeBody(const anim::Skeleton& skeleton, const RagdollBoneProfile& bone,
            uint32_t joint, std::span<const math::Mat4> bind, const CharacterAxes& axes)
        {
            RagdollBodyDescription body{.rest = frameOf(bind[joint]),
                .radius = bone.radius * axes.scale,
                .bendDirection = axes.toRest(bone.bendDirection),
                .limits = bone.limits};
            const math::Vec3 origin = body.rest.position;
            const uint32_t axisEnd = bone.axisEnd.empty() ? anim::k_noJoint : findProfileJoint(skeleton, bone.axisEnd);
            const math::Vec3 tip = axisEnd != anim::k_noJoint
                ? positionOf(bind[axisEnd])
                : origin + body.rest.rotation * (k_up * (k_fallbackBoneLength * axes.scale));
            body.twistAxis = directionOr(tip - origin, body.rest.rotation * k_up);

            math::Vec3 start = origin;
            math::Vec3 end = tip;

            if (!bone.capsuleFrom.empty())
            {
                auto from = requiredJoint(skeleton, bone.capsuleFrom);

                if (!from)
                {
                    return from.error();
                }

                start = positionOf(bind[from.value()]);
            }

            if (!bone.capsuleTo.empty())
            {
                auto to = requiredJoint(skeleton, bone.capsuleTo);

                if (!to)
                {
                    return to.error();
                }

                end = positionOf(bind[to.value()]);
            }

            const math::Vec3 direction = directionOr(end - start, body.twistAxis);
            body.capsuleStart = start + direction * (bone.startOffset * axes.scale);
            body.capsuleEnd = end + direction * (bone.endExtension * axes.scale);
            return body;
        }

        RagdollBoneProfile mixamoBone(std::string bone, std::string parent, std::string axisEnd, float radius,
            const math::Vec3& bendDirection, const RagdollJointLimits& limits)
        {
            return RagdollBoneProfile{.bone = std::move(bone),
                .parent = std::move(parent),
                .axisEnd = std::move(axisEnd),
                .capsuleFrom = {},
                .capsuleTo = {},
                .radius = radius,
                .bendDirection = bendDirection,
                .limits = limits};
        }

        const std::string k_mixamoChest{"Spine2|Spine1"};

        void addMixamoSide(RagdollProfile& profile, const std::string& side)
        {
            RagdollBoneProfile upperArm = mixamoBone(side + "Arm", k_mixamoChest, side + "ForeArm", 0.055f, k_forward,
                limitsInDegrees(70.0f, -60.0f, 110.0f, 120.0f));
            upperArm.startOffset = 0.03f;
            profile.bones.push_back(std::move(upperArm));

            RagdollBoneProfile foreArm = mixamoBone(side + "ForeArm", side + "Arm", side + "Hand", 0.045f, k_forward,
                limitsInDegrees(80.0f, -5.0f, 145.0f, 10.0f));
            foreArm.endExtension = 0.1f;
            profile.bones.push_back(std::move(foreArm));

            profile.bones.push_back(mixamoBone(side + "UpLeg", "Hips", side + "Leg", 0.08f, k_forward,
                limitsInDegrees(30.0f, -30.0f, 115.0f, 40.0f)));
            profile.bones.push_back(mixamoBone(side + "Leg", side + "UpLeg", side + "Foot", 0.06f, k_back,
                limitsInDegrees(10.0f, -3.0f, 145.0f, 5.0f)));

            RagdollBoneProfile foot = mixamoBone(side + "Foot", side + "Leg", side + "ToeBase", 0.045f, k_up,
                limitsInDegrees(15.0f, -35.0f, 35.0f, 15.0f));
            foot.capsuleTo = side + "Toe_End";
            foot.startOffset = -0.03f;
            profile.bones.push_back(std::move(foot));
        }
    } //namespace

    RagdollProfile mixamoRagdollProfile()
    {
        RagdollProfile profile{};

        RagdollBoneProfile pelvis = mixamoBone("Hips", "", "Spine", 0.1f, k_forward, {});
        pelvis.capsuleFrom = "LeftUpLeg";
        pelvis.capsuleTo = "RightUpLeg";
        profile.bones.push_back(std::move(pelvis));

        profile.bones.push_back(mixamoBone("Spine", "Hips", k_mixamoChest, 0.11f, k_forward,
            limitsInDegrees(15.0f, -20.0f, 35.0f, 20.0f)));

        RagdollBoneProfile chest = mixamoBone(k_mixamoChest, "Spine", "Neck", 0.13f, k_forward,
            limitsInDegrees(15.0f, -20.0f, 30.0f, 20.0f));
        profile.bones.push_back(std::move(chest));

        RagdollBoneProfile head = mixamoBone("Head", k_mixamoChest, "HeadTop_End", 0.1f, k_forward,
            limitsInDegrees(70.0f, -40.0f, 45.0f, 35.0f));
        head.startOffset = 0.05f;
        head.endExtension = -0.09f;
        profile.bones.push_back(std::move(head));

        addMixamoSide(profile, "Left");
        addMixamoSide(profile, "Right");
        return profile;
    }

    Result<SkeletonRagdoll> describeSkeletonRagdoll(const anim::Skeleton& skeleton, const RagdollProfile& profile,
        std::span<const math::Mat4> jointMatrices, const math::Transform& placement)
    {
        if (jointMatrices.size() != skeleton.joints.size())
        {
            return invalid("one joint matrix per skeleton joint is needed");
        }

        if (profile.bones.empty() || profile.bones.size() > k_maxRagdollBodies || !(profile.referenceTorso > 0.0f))
        {
            return invalid("the profile needs 1 to k_maxRagdollBodies bones and a positive referenceTorso");
        }

        std::vector<math::Mat4> bind;
        bind.reserve(skeleton.joints.size());

        for (const anim::Joint& joint : skeleton.joints)
        {
            bind.push_back(math::inverse(joint.inverseBind));
        }

        auto axes = characterAxes(skeleton, profile, bind);

        if (!axes)
        {
            return axes.error();
        }

        const math::Mat4 world = math::toMatrix(placement);
        SkeletonRagdoll ragdoll{};

        for (std::size_t index = 0; index < profile.bones.size(); ++index)
        {
            const RagdollBoneProfile& bone = profile.bones[index];
            auto joint = requiredJoint(skeleton, bone.bone);
            auto parent = parentBody(profile, index);

            if (!joint || !parent)
            {
                return !joint ? joint.error() : parent.error();
            }

            auto body = describeBody(skeleton, bone, joint.value(), bind, axes.value());

            if (!body)
            {
                return body.error();
            }

            body.value().parent = parent.value();
            body.value().pose = frameOf(world * jointMatrices[joint.value()]);
            body.value().density = profile.density;
            ragdoll.description.bodies.push_back(body.value());
            ragdoll.joints.push_back(joint.value());
        }

        return ragdoll;
    }
} //namespace lockstep::physics
