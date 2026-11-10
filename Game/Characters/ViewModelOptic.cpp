#include "Characters/ViewModelOptic.hpp"

#include "Characters/ViewModelSights.hpp"
#include "Weapons/WeaponSockets.hpp"

#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace render = lockstep::render;

        constexpr math::Vec3 k_forward{0.0f, 0.0f, -1.0f};
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr float k_minScopedZoom{0.05f};
        constexpr float k_upProbe{0.05f};
        constexpr float k_minutesPerDegree{60.0f};

        math::Vec3 apparent(const ViewModelState& state, const math::Vec3& world, float stretch)
        {
            const math::Vec3 view = math::conjugate(state.camera) * (world - state.eye);

            return state.eye + state.camera * math::Vec3{view.x * stretch, view.y * stretch, view.z};
        }

        math::Quat facing(const math::Vec3& forward, const math::Vec3& up)
        {
            const math::Vec3 z = math::normalize(forward) * -1.0f;
            const math::Vec3 x = math::normalize(math::cross(up, z));
            const math::Vec3 y = math::cross(z, x);
            math::Mat4 basis{1.0f};
            basis.setColumn(0, math::Vec4{x.x, x.y, x.z, 0.0f});
            basis.setColumn(1, math::Vec4{y.x, y.y, y.z, 0.0f});
            basis.setColumn(2, math::Vec4{z.x, z.y, z.z, 0.0f});

            return math::fromRotationMatrix(basis);
        }

        std::optional<math::Transform> aimPoseEyepiece(const FirstPersonModel& model, const OpticModel& optic)
        {
            const anim::SkinnedModel& skinned = model.character.skinned();
            anim::SkinnedInstance probe{};
            probe.animator().play(model.aimClip, 0.0f);
            probe.update(skinned, 0.0f);
            const std::optional<math::Transform> mount = modelOpticSocket(skinned, probe,
                firstPersonWeaponPoints(model.weapon));

            return mount ? std::optional<math::Transform>(placeOnMount(*mount, optic.eyepiece)) : std::nullopt;
        }

        const OpticPart* lensOf(const OpticModel& optic)
        {
            const auto lens = std::find_if(optic.parts.begin(), optic.parts.end(), [](const OpticPart& part)
            {
                return part.isLens;
            });

            return lens == optic.parts.end() ? nullptr : &*lens;
        }
    } //namespace

    void prepareViewModelOptic(ViewModelState& state, const FirstPersonModel& model, const OpticModel* optic,
        const Optics& optics)
    {
        const std::optional<math::Transform> eyepiece = optic ? aimPoseEyepiece(model, *optic) : std::nullopt;

        if (!eyepiece)
        {
            state.opticAim.reset();
            return;
        }

        const bool isScope = optic->kind == OpticKind::Scope4x;
        const float eyeDistance = isScope ? optics.scopeTuning.eyeRelief : optics.holographicTuning.eyeDistance;
        const math::Quat rotation = math::conjugate(eyepiece->rotation);
        const math::Vec3 origin = k_forward * eyeDistance - rotation * eyepiece->position;
        state.opticAim = ViewModelOpticAim{.sights = ViewModelPlacement{.rotation = rotation, .origin = origin},
            .swayScale = isScope ? optics.scopeTuning.swayScale : 1.0f,
            .isMagnified = isScope,
            .axis = std::nullopt};
    }

    void presentViewModelOptic(render::Renderer& renderer, const FirstPersonModel& model, ViewModelState& state,
        const OpticModel* optic, const Optics& optics, float worldFov, render::RenderLayers layers)
    {
        if (!optic || !state.opticAim)
        {
            return;
        }

        const std::optional<WeaponSocket> socket = viewModelLayerSocket(model, state);

        if (!socket)
        {
            return;
        }

        drawOptic(renderer, *optic, socket->optic, render::k_viewModelLayer);

        const float stretch = std::tan(0.5f * worldFov) / std::tan(0.5f * viewModelFov());
        const math::Transform eyepiece = placeOnMount(socket->optic, optic->eyepiece);
        const math::Transform objective = placeOnMount(socket->optic, optic->objective);
        const math::Vec3 front = apparent(state, objective.position, stretch);
        const math::Vec3 back = apparent(state, eyepiece.position, stretch);
        const math::Vec3 above = apparent(state, objective.position + objective.rotation * k_up * k_upProbe, stretch);
        const math::Vec3 axis = math::normalize(front - back);
        state.opticAim->axis = axis;

        const OpticPart* lens = lensOf(*optic);

        if (lens && optic->kind == OpticKind::Holographic)
        {
            const HolographicTuning& holographic = optics.holographicTuning;
            renderer.submitSight(render::HolographicSight{.window = lens->mesh,
                .windowTransform = placeOnMount(socket->optic, lens->local),
                .direction = eyepiece.rotation * k_forward,
                .radiance = holographic.radiance * std::max(holographic.brightness, 0.0f),
                .ringRadius = math::radians(holographic.ringRadius / k_minutesPerDegree),
                .ringWidth = math::radians(holographic.ringWidth / k_minutesPerDegree),
                .dotRadius = math::radians(holographic.dotRadius / k_minutesPerDegree)});
            return;
        }

        if (!lens || viewModelZoom(state) < k_minScopedZoom)
        {
            return;
        }

        const OpticTuning& tuning = optics.scopeTuning;

        render::ScopeReticle reticle = tuning.reticle;
        reticle.illumination = tuning.illuminated ? tuning.illumination : math::Vec3{0.0f, 0.0f, 0.0f};
        renderer.submitScope(render::ScopeView{.lens = lens->mesh,
            .lensTransform = placeOnMount(socket->optic, lens->local),
            .eyepiece = eyepiece,
            .objective = math::Transform{.position = front, .rotation = facing(axis, above - front)},
            .magnification = tuning.magnification,
            .fieldOfView = tuning.fieldOfView,
            .eyeRelief = tuning.eyeRelief,
            .exitPupil = tuning.exitPupil,
            .layers = layers,
            .reticle = reticle});
    }

    float opticSensitivityScale(const Optics& optics, const OpticModel* optic)
    {
        const OpticTuning& tuning = optics.scopeTuning;

        return optic && optic->kind == OpticKind::Scope4x
            ? 1.0f / std::pow(std::max(tuning.magnification, 1.0f), std::max(tuning.sensitivityPower, 0.0f))
            : 1.0f;
    }
} //namespace game
