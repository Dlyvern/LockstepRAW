#include "Characters/ViewModelSights.hpp"

#include <Lockstep/Render/Camera.hpp>

#include <cmath>

namespace game
{
    namespace
    {
        constexpr math::Vec3 k_rearSight{-0.0833f, 1.3428f, 0.2903f};
        constexpr math::Vec3 k_frontSight{-0.2536f, 1.4507f, 0.5030f};
        constexpr math::Vec3 k_muzzle{-0.3023f, 1.4032f, 0.5760f};
        constexpr math::Vec3 k_modelUp{0.0f, 1.0f, 0.0f};
        constexpr float k_railBack{0.07f};
        constexpr float k_railDrop{0.045f};
        constexpr math::Vec3 k_viewForward{0.0f, 0.0f, -1.0f};
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};
        constexpr math::Vec3 k_right{1.0f, 0.0f, 0.0f};

        struct Projection
        {
            const ViewModelState& state;
            float stretch{1.0f};

            math::Vec3 toWorld(const math::Vec3& model) const
            {
                const math::Vec3 view = state.view.position + state.view.rotation * model;
                const math::Vec3 apparent{view.x * stretch, view.y * stretch, view.z};

                return state.eye + state.camera * apparent;
            }
        };

        Projection projectionOf(const ViewModelState& state, float worldFov)
        {
            return Projection{.state = state,
                .stretch = std::tan(0.5f * worldFov) / std::tan(0.5f * viewModelFov())};
        }

        math::Quat facing(const math::Vec3& position, const math::Vec3& forward, const math::Vec3& up)
        {
            return math::fromRotationMatrix(math::inverse(math::lookAt(position, position + forward, up)));
        }
    } //namespace

    float viewModelFov()
    {
        return lockstep::render::Camera{}.viewModelFov;
    }

    std::optional<ViewModelSockets> viewModelSockets(const ViewModelState& state, float worldFov)
    {
        if (!state.isDrawn)
        {
            return std::nullopt;
        }

        const Projection projection = projectionOf(state, worldFov);
        const math::Vec3 rear = projection.toWorld(k_rearSight);
        const math::Vec3 front = projection.toWorld(k_frontSight);
        const math::Vec3 muzzle = projection.toWorld(k_muzzle);
        const math::Vec3 forward = math::normalize(front - rear);
        const math::Vec3 top = math::normalize(projection.toWorld(k_muzzle + k_modelUp * 0.1f) - muzzle);

        return ViewModelSockets{.muzzle = math::Transform{.position = muzzle, .rotation = facing(muzzle, forward, top)},
            .forward = forward,
            .rearSight = rear,
            .frontSight = front,
            .rail = muzzle - forward * k_railBack - top * k_railDrop};
    }

    std::optional<math::Vec3> viewModelShotDirection(const ViewModelState& state, float worldFov)
    {
        if (!state.isDrawn)
        {
            return std::nullopt;
        }

        const math::Quat look = math::angleAxis(state.lastYaw, k_up) * math::angleAxis(state.lastPitch, k_right);
        const math::Vec3 centre = look * k_viewForward;
        const math::Vec3 post = math::normalize(projectionOf(state, worldFov).toWorld(k_frontSight) - state.eye);

        return math::normalize(math::lerp(centre, post, viewModelZoom(state)));
    }
} //namespace game
