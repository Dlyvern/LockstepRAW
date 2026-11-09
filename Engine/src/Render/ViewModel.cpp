#include "ViewModel.hpp"

#include "RendererState.hpp"
#include "VelaConversions.hpp"

#include <algorithm>

namespace lockstep::render
{
    namespace
    {
        constexpr float k_viewModelNearPlane{0.01f};
        constexpr float k_viewModelFarPlane{100.0f};
        constexpr float k_viewModelDepthScale{0.01f};
        constexpr float k_minViewModelFov{math::radians(10.0f)};
        constexpr float k_maxViewModelFov{math::radians(150.0f)};

        math::Mat4 squeezeDepth(math::Mat4 projection)
        {
            for (int column = 0; column < 4; ++column)
            {
                projection[column][2] *= k_viewModelDepthScale;
            }

            return projection;
        }

        math::Mat4 viewModelProjection(const Camera& camera, float aspectRatio)
        {
            Camera lens = camera;
            lens.verticalFov = std::clamp(camera.viewModelFov, k_minViewModelFov, k_maxViewModelFov);
            lens.nearPlane = k_viewModelNearPlane;
            lens.farPlane = k_viewModelFarPlane;

            return toProjectionMatrix(lens, aspectRatio);
        }
    } //namespace

    void prepareViewModel(RendererState& state)
    {
        const math::Mat4 projection = viewModelProjection(state.camera, state.aspectRatio);
        math::Mat4 jittered = projection;
        jittered[2][0] += state.projection[2][0] - state.unjitteredProjection[2][0];
        jittered[2][1] += state.projection[2][1] - state.unjitteredProjection[2][1];

        TemporalState& temporal = state.temporal;
        temporal.viewModelViewProjection = squeezeDepth(projection) * state.view;

        if (!temporal.hasPrevious)
        {
            temporal.previousViewModelViewProjection = temporal.viewModelViewProjection;
        }

        GpuFrameLighting& lighting = state.prepared.lighting;
        lighting.viewModelViewProjection = squeezeDepth(jittered) * state.view;
        lighting.viewModelMotionViewProjection = temporal.viewModelViewProjection;
        lighting.previousViewModelMotionViewProjection = temporal.previousViewModelViewProjection;
        lighting.sceneDepthParams[0] = state.unjitteredProjection[2][2];
        lighting.sceneDepthParams[1] = state.unjitteredProjection[3][2];
    }
} //namespace lockstep::render
