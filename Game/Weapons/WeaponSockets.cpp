#include "Weapons/WeaponSockets.hpp"

#include "Characters/PawnCharacters.hpp"
#include "Characters/ViewModel.hpp"
#include "Characters/ViewModelSights.hpp"
#include "Npcs/NpcPresentation.hpp"

#include <cmath>
#include <string_view>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;

        constexpr std::string_view k_handJoint{"RightHand"};
        constexpr std::string_view k_weaponJoint{"W_Main"};

        constexpr WeaponPoints k_krissPoints{.joint = k_handJoint,
            .magazineJoint = "KRISS_Mag",
            .muzzle = {0.0838f, 0.4671f, 0.0207f},
            .forward = {0.0991f, 0.995f, -0.0132f},
            .up = {0.9929f, -0.098f, 0.0672f},
            .port = {0.0610f, 0.2366f, 0.0046f},
            .magazine = {-0.0956f, 0.2426f, 0.0122f},
            .magazineAxis = {0.9368f, 0.3436f, 0.0666f},
            .optic = {0.1013f, 0.1941f, 0.0273f},
            .opticForward = {0.0437f, 0.9989f, -0.0169f},
            .opticUp = {0.9969f, -0.0425f, 0.0664f}};

        constexpr WeaponPoints k_m4Points{.joint = k_weaponJoint,
            .magazineJoint = "W_Mag",
            .muzzle = {0.0034f, 0.0450f, 0.4326f},
            .forward = {0.0022f, 0.0165f, 0.9999f},
            .up = {0.0f, 0.9999f, -0.0165f},
            .port = {-0.0243f, 0.0497f, -0.0164f},
            .magazine = {0.0032f, -0.0968f, -0.0184f},
            .magazineAxis = {-0.1103f, 0.8405f, -0.5305f},
            .optic = {0.0028f, 0.0781f, -0.0622f},
            .opticForward = {0.0022f, 0.0165f, 0.9999f},
            .opticUp = {0.0f, 0.9999f, -0.0165f}};

        constexpr WeaponPoints k_glockPoints{.joint = k_weaponJoint,
            .magazineJoint = "W_Mag",
            .muzzle = {-0.0001f, 0.0465f, 0.1026f},
            .forward = {0.0053f, -0.0202f, 0.9998f},
            .up = {0.0001f, 0.9998f, 0.0202f},
            .port = {-0.0122f, 0.0705f, 0.0116f},
            .magazine = {-0.0008f, 0.0320f, -0.0465f},
            .magazineAxis = {0.0098f, 0.8641f, 0.5033f},
            .optic = {-0.0010f, 0.0726f, -0.0730f},
            .opticForward = {0.0053f, -0.0202f, 0.9998f},
            .opticUp = {0.0001f, 0.9998f, 0.0202f}};

        struct Placement
        {
            math::Mat4 hand{1.0f};
            math::Mat4 magazine{1.0f};
        };

        bool isJointNamed(std::string_view name, std::string_view wanted)
        {
            if (!name.ends_with(wanted))
            {
                return false;
            }

            const std::size_t before = name.size() - wanted.size();
            return before == 0 || name[before - 1] == ':' || name[before - 1] == '_';
        }

        uint32_t findSocketJoint(const anim::Skeleton& skeleton, std::string_view name)
        {
            for (uint32_t index = 0; index < skeleton.joints.size(); ++index)
            {
                if (isJointNamed(skeleton.joints[index].name, name))
                {
                    return index;
                }
            }

            return anim::k_noJoint;
        }

        math::Quat basisRotation(const math::Vec3& xAxis, const math::Vec3& yAxis)
        {
            const math::Vec3 y = math::normalize(yAxis);
            const math::Vec3 x = math::normalize(xAxis - y * math::dot(xAxis, y));
            const math::Vec3 z = math::cross(x, y);
            math::Mat4 basis{1.0f};
            basis.setColumn(0, math::Vec4{x.x, x.y, x.z, 0.0f});
            basis.setColumn(1, math::Vec4{y.x, y.y, y.z, 0.0f});
            basis.setColumn(2, math::Vec4{z.x, z.y, z.z, 0.0f});

            return math::fromRotationMatrix(basis);
        }

        template<typename ToWorld>
        WeaponSocket socketOf(const Placement& placement, const WeaponPoints& points, const ToWorld& toWorld)
        {
            const auto point = [&](const math::Mat4& joint, const math::Vec3& local)
            {
                return toWorld(math::transformPoint(joint, local));
            };

            const math::Vec3 muzzle = point(placement.hand, points.muzzle);
            const math::Vec3 forward = math::normalize(point(placement.hand, points.muzzle + points.forward * 0.1f) - muzzle);
            const math::Vec3 upPoint = point(placement.hand, points.muzzle + points.up * 0.1f);
            const math::Vec3 up = math::normalize(upPoint - muzzle - forward * math::dot(upPoint - muzzle, forward));
            const math::Vec3 right = math::cross(forward, up);
            const math::Vec3 magazine = point(placement.magazine, points.magazine);
            const math::Vec3 magazineUp = point(placement.magazine, points.magazine + points.magazineAxis * 0.1f) - magazine;

            const math::Vec3 optic = point(placement.hand, points.optic);
            const math::Vec3 opticForward = math::normalize(point(placement.hand, points.optic + points.opticForward * 0.1f)
                - optic);
            const math::Vec3 opticUp = point(placement.hand, points.optic + points.opticUp * 0.1f) - optic;

            return WeaponSocket{.muzzle = muzzle,
                .forward = forward,
                .up = up,
                .right = right,
                .port = point(placement.hand, points.port),
                .magazine = math::Transform{.position = magazine, .rotation = basisRotation(right, magazineUp)},
                .optic = math::Transform{.position = optic,
                    .rotation = basisRotation(math::cross(opticForward, opticUp), opticUp)}};
        }

        std::optional<Placement> placementOf(const anim::SkinnedModel& model, const anim::SkinnedInstance& instance,
            const WeaponPoints& points)
        {
            const std::span<const math::Mat4> joints = instance.jointMatrices();
            const uint32_t carrier = findSocketJoint(model.skeleton, points.joint);

            if (carrier >= joints.size() || carrier >= model.skeleton.joints.size())
            {
                return std::nullopt;
            }

            Placement placement{.hand = joints[carrier], .magazine = joints[carrier]};
            const uint32_t magazine = anim::findJoint(model.skeleton, points.magazineJoint);

            if (magazine < joints.size())
            {
                placement.magazine = joints[magazine] * model.skeleton.joints[magazine].inverseBind
                    * math::inverse(model.skeleton.joints[carrier].inverseBind);
            }

            return placement;
        }

        std::optional<WeaponSocket> viewModelSocketStretched(const FirstPersonModel& model, const ViewModelState& state,
            float stretch)
        {
            if (!state.isDrawn)
            {
                return std::nullopt;
            }

            const anim::SkinnedModel& skinned = model.character.skinned();
            const WeaponPoints& points = firstPersonWeaponPoints(model.weapon);
            const std::optional<Placement> joints = placementOf(skinned, state.instance, points);

            if (!joints)
            {
                return std::nullopt;
            }

            return socketOf(*joints, points, [&state, stretch](const math::Vec3& point)
            {
                const math::Vec3 view = state.view.position + state.view.rotation * point;
                return state.eye + state.camera * math::Vec3{view.x * stretch, view.y * stretch, view.z};
            });
        }
    } //namespace

    const WeaponPoints& firstPersonWeaponPoints(WeaponKind weapon)
    {
        switch (weapon)
        {
            case WeaponKind::M4:
            {
                return k_m4Points;
            }
            case WeaponKind::Glock:
            {
                return k_glockPoints;
            }
            case WeaponKind::Kriss:
            case WeaponKind::Count:
            {
                break;
            }
        }

        return k_krissPoints;
    }

    WeaponPoints heldWeaponPoints(const WeaponPoints& points, const math::Transform& carrier)
    {
        const math::Mat4 matrix = math::toMatrix(carrier);
        const auto direction = [&matrix](const math::Vec3& vector)
        {
            return math::normalize(math::transformDirection(matrix, vector));
        };

        return WeaponPoints{.joint = k_handJoint,
            .magazineJoint = "",
            .muzzle = math::transformPoint(matrix, points.muzzle),
            .forward = direction(points.forward),
            .up = direction(points.up),
            .port = math::transformPoint(matrix, points.port),
            .magazine = math::transformPoint(matrix, points.magazine),
            .magazineAxis = direction(points.magazineAxis),
            .optic = math::transformPoint(matrix, points.optic),
            .opticForward = direction(points.opticForward),
            .opticUp = direction(points.opticUp)};
    }

    std::optional<WeaponSocket> heldWeaponSocket(const anim::SkinnedModel& model, const anim::SkinnedInstance& instance,
        const math::Transform& placement, const WeaponPoints& points)
    {
        const std::optional<Placement> joints = placementOf(model, instance, points);

        if (!joints)
        {
            return std::nullopt;
        }

        const math::Mat4 world = math::toMatrix(placement);

        return socketOf(*joints, points, [&world](const math::Vec3& point)
        {
            return math::transformPoint(world, point);
        });
    }

    std::optional<math::Transform> modelOpticSocket(const anim::SkinnedModel& model,
        const anim::SkinnedInstance& instance, const WeaponPoints& points)
    {
        const std::optional<Placement> joints = placementOf(model, instance, points);

        if (!joints)
        {
            return std::nullopt;
        }

        return socketOf(*joints, points, [](const math::Vec3& point)
        {
            return point;
        }).optic;
    }

    std::optional<WeaponSocket> pawnWeaponSocket(const PawnCharacters& characters, std::size_t index)
    {
        if (index >= characters.pawns.size() || !characters.armed)
        {
            return std::nullopt;
        }

        const PawnCharacter& pawn = characters.pawns[index];
        const anim::SkinnedModel& model = characters.armed->body.skinned();

        if (!pawn.isDrawn || !pawn.isArmedModel)
        {
            return std::nullopt;
        }

        const HeldGun* held = heldGun(characters, pawn.weapon);
        const WeaponPoints points = held != nullptr ? heldWeaponPoints(firstPersonWeaponPoints(pawn.weapon), held->main)
                                                    : k_krissPoints;

        return heldWeaponSocket(model, pawn.instance, pawn.placement, points);
    }

    std::optional<WeaponSocket> npcWeaponSocket(const NpcLooks& looks, lockstep::world::EntityId entity)
    {
        for (const NpcLook& look : looks.looks)
        {
            if (look.entity != entity)
            {
                continue;
            }

            if (!look.isDrawn || look.model == nullptr || look.isWeaponDropped || look.ragdoll)
            {
                return std::nullopt;
            }

            return heldWeaponSocket(*look.model, look.instance, look.modelTransform, k_krissPoints);
        }

        return std::nullopt;
    }

    std::optional<WeaponSocket> viewModelWeaponSocket(const FirstPersonModel& model, const ViewModelState& state,
        float worldFov)
    {
        return viewModelSocketStretched(model, state, std::tan(0.5f * worldFov) / std::tan(0.5f * viewModelFov()));
    }

    std::optional<WeaponSocket> viewModelLayerSocket(const FirstPersonModel& model, const ViewModelState& state)
    {
        return viewModelSocketStretched(model, state, 1.0f);
    }
} //namespace game
