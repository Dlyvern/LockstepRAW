#include "ScopeView.hpp"

#include "Lockstep/Core/Transform.hpp"

#include "FrameAttachments.hpp"
#include "FrameCulling.hpp"
#include "FramePasses.hpp"
#include "RendererState.hpp"
#include "VelaConversions.hpp"
#include "ViewModel.hpp"
#include "VolumetricFog.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr uint32_t k_shadowAtlasSlot{0};
        constexpr uint32_t k_environmentSlot{1};
        constexpr uint32_t k_planarReflectionSlot{3};
        constexpr uint32_t k_minScopeResolution{256};
        constexpr uint32_t k_minScopeTexels{64};
        constexpr uint32_t k_maxScopeResolution{1024};
        constexpr float k_scopeNearPlane{0.05f};
        constexpr float k_minFieldOfView{math::radians(0.5f)};
        constexpr float k_maxFieldOfView{math::radians(40.0f)};
        constexpr float k_minMagnification{1.0f};
        constexpr float k_maxMagnification{40.0f};
        constexpr math::Vec3 k_forward{0.0f, 0.0f, -1.0f};
        constexpr math::Vec3 k_up{0.0f, 1.0f, 0.0f};

        void setVector(float (&target)[4], const math::Vec3& xyz, float w)
        {
            target[0] = xyz.x;
            target[1] = xyz.y;
            target[2] = xyz.z;
            target[3] = w;
        }

        bool markLens(RendererState& state, const ScopeView& scope)
        {
            const MeshRecord* mesh = state.meshes.find(scope.lens);

            if (mesh == nullptr)
            {
                return false;
            }

            const math::Mat4 model = math::toMatrix(scope.lensTransform);
            bool found = false;

            for (DrawItem& item : state.draws)
            {
                if (item.mesh == &mesh->mesh && item.model == model && item.material->shading == Shading::Lit)
                {
                    item.isScopeLens = true;
                    found = true;
                }
            }

            return found;
        }

        void cullScopeDraws(RendererState& state, const ScopeView& scope, const math::Vec3& eye)
        {
            PreparedScope& prepared = state.scope;
            const Frustum frustum = frustumFromMatrix(prepared.viewProjection);
            std::vector<std::pair<float, uint32_t>> opaque;
            std::vector<std::pair<float, uint32_t>> blended;

            for (uint32_t index = 0; index < state.draws.size(); ++index)
            {
                const DrawItem& item = state.draws[index];

                if (isViewModelDraw(item.layers) || (item.layers & scope.layers) == 0
                    || item.material->shading != Shading::Lit || !intersects(frustum, item.worldBounds))
                {
                    continue;
                }

                const float distance = math::length((item.worldBounds.min + item.worldBounds.max) * 0.5f - eye);
                (item.material->alphaMode == AlphaMode::Blend ? blended : opaque).emplace_back(distance, index);
            }

            std::sort(opaque.begin(), opaque.end());
            std::sort(blended.begin(), blended.end(), [](const auto& left, const auto& right)
            {
                return left.first > right.first;
            });

            for (const auto& [distance, index] : opaque)
            {
                prepared.opaque.push_back(index);
            }

            for (const auto& [distance, index] : blended)
            {
                prepared.blended.push_back(index);
            }
        }

        void writeFog(RendererState& state, const math::Vec3& eye)
        {
            GpuFrameLighting& lighting = state.prepared.lighting;
            const VolumetricFogSettings& fog = state.settings.volumetricFog;
            const float density = state.settings.scope.fog ? volumetricFogCameraDensity(state) : 0.0f;
            const float falloff = std::max(fog.heightFalloff, 0.0f);
            const float exponent = std::clamp(-falloff * (eye.y - fog.baseHeight), -30.0f, 30.0f);

            lighting.scopeFog[0] = density * std::exp(exponent);
            lighting.scopeFog[1] = falloff;
            lighting.scopeFog[2] = fog.baseHeight;
            lighting.scopeFog[3] = density > 0.0f ? std::max(fog.maxDistance, 0.0f) : 0.0f;
            setVector(lighting.scopeFogColor, fog.albedo * std::max(fog.ambientIntensity, 0.0f), 0.0f);
        }

        void writeLightingBlock(RendererState& state, const ScopeView& scope, float tanHalfFov,
            float magnification)
        {
            GpuFrameLighting& lighting = state.prepared.lighting;
            const ScopeReticle& reticle = scope.reticle;
            const math::Vec3 eye = scope.objective.position;
            const float pupil = state.settings.scope.shadow
                ? std::max(scope.exitPupil * std::max(state.settings.scope.eyeBoxScale, 0.0f), 1e-4f) : 0.0f;

            lighting.scopeViewProjection = state.scope.viewProjection;
            setVector(lighting.scopeCamera, eye, 1.0f);
            setVector(lighting.scopeCameraForward, scope.objective.rotation * k_forward, tanHalfFov);
            setVector(lighting.scopeCameraUp, scope.objective.rotation * k_up, magnification);
            setVector(lighting.scopeEyepiece, scope.eyepiece.position, std::max(scope.eyeRelief, 0.0f));
            setVector(lighting.scopeAxis, scope.eyepiece.rotation * k_forward, pupil);
            setVector(lighting.scopeAxisUp, scope.eyepiece.rotation * k_up,
                static_cast<float>(state.scope.texels) / static_cast<float>(state.scopeResolution));
            setVector(lighting.scopeReticle, reticle.color, std::max(reticle.lineWidth, 0.0f));
            lighting.scopeReticleShape[0] = std::max(reticle.postWidth, 0.0f);
            lighting.scopeReticleShape[1] = std::max(reticle.postGap, 0.0f);
            lighting.scopeReticleShape[2] = std::max(reticle.stadiaSpacing, 0.0f);
            lighting.scopeReticleShape[3] = std::max(reticle.stadiaLength, 0.0f);
            setVector(lighting.scopeIllumination, math::max(reticle.illumination, math::Vec3{0.0f, 0.0f, 0.0f}),
                std::max(reticle.dotRadius, 0.0f));
            writeFog(state, eye);
        }

        uint32_t viewTexels(const RendererState& state, float tanHalfFov, float magnification)
        {
            const float screenTan = std::tan(0.5f * std::clamp(state.camera.viewModelFov, k_minFieldOfView, math::radians(150.0f)));
            const float fieldPixels = tanHalfFov * magnification * static_cast<float>(state.frameHeight) / screenTan;
            const float wanted = std::ceil(fieldPixels * std::max(state.settings.scope.texelsPerPixel, 0.0f));

            return std::clamp(static_cast<uint32_t>(std::max(wanted, 0.0f)), k_minScopeTexels, state.scopeResolution);
        }

        math::Mat4 cornerMatrix(float share)
        {
            math::Mat4 matrix{1.0f};
            matrix[0][0] = share;
            matrix[1][1] = share;
            matrix[3][0] = share - 1.0f;
            matrix[3][1] = share - 1.0f;

            return matrix;
        }
    } //namespace

    void prepareScope(RendererState& state)
    {
        PreparedScope& prepared = state.scope;
        prepared.active = false;
        prepared.opaque.clear();
        prepared.blended.clear();
        state.prepared.lighting.scopeCamera[3] = 0.0f;

        if (!state.settings.scope.enabled || !state.scopeView || !markLens(state, *state.scopeView))
        {
            return;
        }

        const ScopeView& scope = *state.scopeView;
        const float fieldOfView = std::clamp(scope.fieldOfView, k_minFieldOfView, k_maxFieldOfView);
        const float magnification = std::clamp(scope.magnification, k_minMagnification, k_maxMagnification);
        Camera camera{};
        camera.position = scope.objective.position;
        camera.target = scope.objective.position + scope.objective.rotation * k_forward;
        camera.up = scope.objective.rotation * k_up;
        camera.verticalFov = fieldOfView;
        camera.nearPlane = k_scopeNearPlane;
        camera.farPlane = state.camera.farPlane;

        const math::Mat4 viewProjection = toProjectionMatrix(camera, 1.0f) * toViewMatrix(camera);
        const float tanHalfFov = std::tan(0.5f * fieldOfView);
        prepared.active = true;
        prepared.texels = viewTexels(state, tanHalfFov, magnification);
        prepared.viewProjection = viewProjection;
        cullScopeDraws(state, scope, camera.position);
        prepared.viewProjection = cornerMatrix(static_cast<float>(prepared.texels)
            / static_cast<float>(state.scopeResolution)) * viewProjection;
        writeLightingBlock(state, scope, tanHalfFov, magnification);
    }

    void prepareSight(RendererState& state)
    {
        GpuFrameLighting& lighting = state.prepared.lighting;
        lighting.sightDirection[3] = 0.0f;

        if (!state.settings.scope.holographic || !state.sight)
        {
            return;
        }

        const HolographicSight& sight = *state.sight;
        const MeshRecord* mesh = state.meshes.find(sight.window);

        if (mesh == nullptr || math::length(sight.direction) <= 0.0f)
        {
            return;
        }

        const math::Mat4 model = math::toMatrix(sight.windowTransform);
        bool found = false;

        for (DrawItem& item : state.draws)
        {
            if (item.mesh == &mesh->mesh && item.model == model && item.material->shading == Shading::Lit
                && item.material->alphaMode == AlphaMode::Blend)
            {
                item.isSightWindow = true;
                found = true;
            }
        }

        if (!found)
        {
            return;
        }

        const float brightness = std::max(state.settings.scope.reticleBrightness, 0.0f);
        setVector(lighting.sightDirection, math::normalize(sight.direction), 1.0f);
        setVector(lighting.sightReticle, math::max(sight.radiance, math::Vec3{0.0f, 0.0f, 0.0f}) * brightness,
            std::max(sight.ringRadius, 0.0f));
        lighting.sightShape[0] = std::max(sight.ringWidth, 0.0f) * 0.5f;
        lighting.sightShape[1] = std::max(sight.dotRadius, 0.0f);
    }

    Status updateScopeResolution(RendererState& state)
    {
        uint32_t& requested = state.settings.scope.resolution;
        requested = std::clamp(requested, k_minScopeResolution, k_maxScopeResolution);

        if (requested == state.scopeResolution)
        {
            return {};
        }

        const uint32_t previous = state.scopeResolution;
        state.scopeResolution = requested;

        if (auto redescribed = state.graph->redescribePass(std::string(k_scopePassName)); !redescribed)
        {
            state.scopeResolution = previous;
            requested = previous;
            return toError(redescribed.error());
        }

        return {};
    }

    Result<std::unique_ptr<ScopePass>> ScopePass::create(RendererState& state)
    {
        return std::unique_ptr<ScopePass>(new ScopePass(state));
    }

    ScopePass::ScopePass(const RendererState& state)
        : m_state(&state)
    {
    }

    vela::graphics::PassDescription ScopePass::describe() const
    {
        auto target = [this](std::string_view name, vela::graphics::TextureFormat format, bool keep)
        {
            vela::graphics::AttachmentSlot slot{};
            slot.name = std::string(name);
            slot.format = format;
            slot.load = vela::graphics::LoadOp::Clear;
            slot.store = keep ? vela::graphics::StoreOp::Store : vela::graphics::StoreOp::DontCare;
            slot.clear = {0.0f, 0.0f, 0.0f, 0.0f};
            slot.clear.depth = 1.0f;
            slot.size = {m_state->scopeResolution, m_state->scopeResolution};

            return slot;
        };

        vela::graphics::PassDescription description{};
        description.colorOutputs.push_back(target(k_scopeColorAttachment, k_sceneColorFormat, true));
        description.colorOutputs.push_back(target(k_scopeAmbientAttachment, k_sceneColorFormat, false));
        description.colorOutputs.push_back(target(k_scopeVelocityAttachment, k_sceneColorFormat, false));
        description.colorOutputs.push_back(target(k_scopeSurfaceNormalAttachment, k_sceneColorFormat, false));
        description.colorOutputs.push_back(target(k_scopeSurfaceSpecularAttachment, k_sceneColorFormat, false));
        description.depthOutput = target(k_scopeDepthAttachment, k_depthFormat, false);
        description.inputs.push_back({std::string(k_shadowMapAttachment), vela::graphics::InputUsage::Sampled});
        description.inputs.push_back({std::string(k_environmentAttachment), vela::graphics::InputUsage::Sampled});

        return description;
    }

    void ScopePass::record(vela::graphics::PassRecorder& recorder)
    {
        const PreparedScope& scope = m_state->scope;

        if (!scope.active)
        {
            return;
        }

        recorder.setScissor(0, 0, scope.texels, scope.texels);
        recorder.bindAttachment(k_shadowAtlasSlot, std::string(k_shadowMapAttachment), shadowCompareSampler());
        recorder.bindAttachment(k_environmentSlot, std::string(k_environmentAttachment));
        recorder.bindAttachment(k_shadowDepthSlot, std::string(k_shadowMapAttachment), shadowDepthSampler());
        recorder.bindTexture(k_planarReflectionSlot, m_state->environment.whiteLinear);

        for (const uint32_t index : scope.opaque)
        {
            recordSceneDraw(recorder, m_state->draws[index], PbrView::Scope);
        }

        for (const uint32_t index : scope.blended)
        {
            recordSceneDraw(recorder, m_state->draws[index], PbrView::Scope);
        }
    }
} //namespace lockstep::render
