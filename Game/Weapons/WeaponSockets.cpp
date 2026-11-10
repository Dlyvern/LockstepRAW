#include "Weapons/WeaponSockets.hpp"

#include "Characters/PawnCharacters.hpp"
#include "Characters/ViewModel.hpp"
#include "Characters/ViewModelSights.hpp"
#include "Npcs/NpcPresentation.hpp"
#include "Weapons/DroppedWeapons.hpp"

#include <cmath>
#include <string_view>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;

        constexpr math::Vec3 k_handMuzzle{0.0838f, 0.4671f, 0.0207f};
        constexpr math::Vec3 k_handForward{0.0991f, 0.995f, -0.0132f};
        constexpr math::Vec3 k_handUp{0.9929f, -0.098f, 0.0672f};
        constexpr math::Vec3 k_handPort{0.0610f, 0.2366f, 0.0046f};
        constexpr math::Vec3 k_handMagazine{-0.0956f, 0.2426f, 0.0122f};
        constexpr math::Vec3 k_handMagazineAxis{0.9368f, 0.3436f, 0.0666f};
        constexpr std::string_view k_magazineJoint{"KRISS_Mag"};

        struct Placement
        {
            math::Mat4 hand{1.0f};
            math::Mat4 magazine{1.0f};
        };

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
        WeaponSocket socketOf(const Placement& placement, const ToWorld& toWorld)
        {
            const auto point = [&](const math::Mat4& joint, const math::Vec3& local)
            {
                return toWorld(math::transformPoint(joint, local));
            };

            const math::Vec3 muzzle = point(placement.hand, k_handMuzzle);
            const math::Vec3 forward = math::normalize(point(placement.hand, k_handMuzzle + k_handForward * 0.1f) - muzzle);
            const math::Vec3 upPoint = point(placement.hand, k_handMuzzle + k_handUp * 0.1f);
            const math::Vec3 up = math::normalize(upPoint - muzzle - forward * math::dot(upPoint - muzzle, forward));
            const math::Vec3 right = math::cross(forward, up);
            const math::Vec3 magazine = point(placement.magazine, k_handMagazine);
            const math::Vec3 magazineUp = point(placement.magazine, k_handMagazine + k_handMagazineAxis * 0.1f) - magazine;

            return WeaponSocket{.muzzle = muzzle,
                .forward = forward,
                .up = up,
                .right = right,
                .port = point(placement.hand, k_handPort),
                .magazine = math::Transform{.position = magazine, .rotation = basisRotation(right, magazineUp)}};
        }

        std::optional<Placement> placementOf(const anim::SkinnedModel& model, const anim::SkinnedInstance& instance,
            uint32_t hand)
        {
            const std::span<const math::Mat4> joints = instance.jointMatrices();

            if (hand >= joints.size() || hand >= model.skeleton.joints.size())
            {
                return std::nullopt;
            }

            Placement placement{.hand = joints[hand], .magazine = joints[hand]};
            const uint32_t magazine = anim::findJoint(model.skeleton, k_magazineJoint);

            if (magazine < joints.size())
            {
                placement.magazine = joints[magazine] * model.skeleton.joints[magazine].inverseBind
                    * math::inverse(model.skeleton.joints[hand].inverseBind);
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
            const std::optional<HeldWeapon> weapon = findHeldWeapon(skinned);
            const std::optional<Placement> joints = weapon ? placementOf(skinned, state.instance, weapon->hand)
                                                           : std::nullopt;

            if (!joints)
            {
                return std::nullopt;
            }

            return socketOf(*joints, [&state, stretch](const math::Vec3& point)
            {
                const math::Vec3 view = state.view.position + state.view.rotation * point;
                return state.eye + state.camera * math::Vec3{view.x * stretch, view.y * stretch, view.z};
            });
        }
    } //namespace

    std::optional<WeaponSocket> heldWeaponSocket(const anim::SkinnedModel& model, const anim::SkinnedInstance& instance,
        const math::Transform& placement, uint32_t hand)
    {
        const std::optional<Placement> joints = placementOf(model, instance, hand);

        if (!joints)
        {
            return std::nullopt;
        }

        const math::Mat4 world = math::toMatrix(placement);

        return socketOf(*joints, [&world](const math::Vec3& point)
        {
            return math::transformPoint(world, point);
        });
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

        const std::optional<HeldWeapon> weapon = findHeldWeapon(model);

        return weapon ? heldWeaponSocket(model, pawn.instance, pawn.placement, weapon->hand) : std::nullopt;
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

            const std::optional<HeldWeapon> weapon = findHeldWeapon(*look.model);

            return weapon ? heldWeaponSocket(*look.model, look.instance, look.modelTransform, weapon->hand)
                          : std::nullopt;
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
