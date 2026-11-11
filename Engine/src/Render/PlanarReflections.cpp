#include "PlanarReflections.hpp"

#include "Lockstep/Core/Bounds.hpp"
#include "Lockstep/Core/Transform.hpp"

#include "FrameAttachments.hpp"
#include "FrameCulling.hpp"
#include "FramePasses.hpp"
#include "RendererState.hpp"
#include "ViewModel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>

namespace lockstep::render
{
    namespace
    {
        constexpr uint32_t k_shadowAtlasSlot{0};
        constexpr uint32_t k_environmentSlot{1};
        constexpr uint32_t k_planarReflectionSlot{3};
        constexpr float k_minCameraClearance{0.01f};

        struct ReflectorPlane
        {
            math::Vec3 normal;
            float offset;
        };

        struct Candidate
        {
            const PlanarReflector* reflector{nullptr};
            const MeshRecord* mesh{nullptr};
            math::Mat4 model{1.0f};
            math::Aabb worldBounds{};
            ReflectorPlane plane{};
            float distance{0.0f};
        };

        float component(const math::Vec3& vector, int axis)
        {
            return axis == 0 ? vector.x : (axis == 1 ? vector.y : vector.z);
        }

        math::Vec3 unitAxis(int axis, float sign)
        {
            return {axis == 0 ? sign : 0.0f, axis == 1 ? sign : 0.0f, axis == 2 ? sign : 0.0f};
        }

        math::Vec3 xyz(const math::Vec4& vector)
        {
            return {vector.x, vector.y, vector.z};
        }

        ReflectorPlane derivePlane(const math::Aabb& local, const math::Mat4& model, const math::Vec3& camera)
        {
            const math::Vec3 extent = local.max - local.min;
            int axis = 0;

            for (int candidate = 1; candidate < 3; ++candidate)
            {
                if (component(extent, candidate) < component(extent, axis))
                {
                    axis = candidate;
                }
            }

            const math::Vec3 centre = (local.min + local.max) * 0.5f;
            const math::Mat4 normalMatrix = math::transpose(math::inverse(model));
            math::Vec3 normal = math::normalize(xyz(normalMatrix * math::Vec4{unitAxis(axis, 1.0f).x,
                unitAxis(axis, 1.0f).y, unitAxis(axis, 1.0f).z, 0.0f}));
            const math::Vec3 worldCentre = math::transformPoint(model, centre);
            const float side = math::dot(normal, camera - worldCentre) >= 0.0f ? 1.0f : -1.0f;
            normal = normal * side;

            const math::Vec3 face = centre + unitAxis(axis, side) * (component(extent, axis) * 0.5f);

            return {normal, math::dot(normal, math::transformPoint(model, face))};
        }

        float distanceToBox(const math::Aabb& box, const math::Vec3& point)
        {
            const math::Vec3 closest = math::max(box.min, math::min(point, box.max));

            return math::length(point - closest);
        }

        math::Mat4 reflectionMatrix(const ReflectorPlane& plane)
        {
            const math::Vec3& n = plane.normal;
            math::Mat4 matrix{1.0f};
            matrix[0][0] = 1.0f - 2.0f * n.x * n.x;
            matrix[1][0] = -2.0f * n.x * n.y;
            matrix[2][0] = -2.0f * n.x * n.z;
            matrix[0][1] = -2.0f * n.y * n.x;
            matrix[1][1] = 1.0f - 2.0f * n.y * n.y;
            matrix[2][1] = -2.0f * n.y * n.z;
            matrix[0][2] = -2.0f * n.z * n.x;
            matrix[1][2] = -2.0f * n.z * n.y;
            matrix[2][2] = 1.0f - 2.0f * n.z * n.z;
            matrix[3][0] = 2.0f * plane.offset * n.x;
            matrix[3][1] = 2.0f * plane.offset * n.y;
            matrix[3][2] = 2.0f * plane.offset * n.z;

            return matrix;
        }

        float signOf(float value)
        {
            return value > 0.0f ? 1.0f : (value < 0.0f ? -1.0f : 0.0f);
        }

        math::Mat4 obliqueProjection(math::Mat4 projection, const math::Vec4& viewPlane)
        {
            const math::Vec4 clipPlane = math::transpose(math::inverse(projection)) * viewPlane;
            const math::Vec4 corner = math::inverse(projection)
                * math::Vec4{signOf(clipPlane.x), signOf(clipPlane.y), 1.0f, 1.0f};
            const float scale = 1.0f / math::dot(viewPlane, corner);

            projection[0][2] = viewPlane.x * scale;
            projection[1][2] = viewPlane.y * scale;
            projection[2][2] = viewPlane.z * scale;
            projection[3][2] = viewPlane.w * scale;

            return projection;
        }

        math::Vec4 screenFootprint(const math::Aabb& box, const math::Mat4& viewProjection)
        {
            math::Vec4 rect{1.0f, 1.0f, -1.0f, -1.0f};

            for (uint32_t corner = 0; corner < 8; ++corner)
            {
                const math::Vec4 point{(corner & 1u) != 0 ? box.max.x : box.min.x,
                    (corner & 2u) != 0 ? box.max.y : box.min.y, (corner & 4u) != 0 ? box.max.z : box.min.z, 1.0f};
                const math::Vec4 clip = viewProjection * point;

                if (clip.w <= 1e-4f)
                {
                    return {-1.0f, -1.0f, 1.0f, 1.0f};
                }

                rect.x = std::min(rect.x, clip.x / clip.w);
                rect.y = std::min(rect.y, clip.y / clip.w);
                rect.z = std::max(rect.z, clip.x / clip.w);
                rect.w = std::max(rect.w, clip.y / clip.w);
            }

            return {std::clamp(rect.x, -1.0f, 1.0f), std::clamp(rect.y, -1.0f, 1.0f), std::clamp(rect.z, -1.0f, 1.0f),
                std::clamp(rect.w, -1.0f, 1.0f)};
        }

        math::Mat4 rectCullMatrix(const math::Vec4& rect)
        {
            const float width = std::max(rect.z - rect.x, 1e-4f);
            const float height = std::max(rect.w - rect.y, 1e-4f);
            math::Mat4 matrix{1.0f};
            matrix[0][0] = 2.0f / width;
            matrix[1][1] = 2.0f / height;
            matrix[3][0] = -(rect.z + rect.x) / width;
            matrix[3][1] = -(rect.w + rect.y) / height;

            return matrix;
        }

        std::optional<Candidate> nearestReflector(const RendererState& state, const Frustum& cameraFrustum)
        {
            std::optional<Candidate> best;
            const math::Vec3& camera = state.camera.position;

            for (const PlanarReflector& reflector : state.reflectors)
            {
                const MeshRecord* mesh = state.meshes.find(reflector.mesh);

                if (mesh == nullptr)
                {
                    continue;
                }

                const math::Aabb worldBounds = math::transformBounds(mesh->bounds, reflector.transform);
                const float distance = distanceToBox(worldBounds, camera);

                if (distance > state.settings.reflections.maxDistance || !intersects(cameraFrustum, worldBounds)
                    || (best && best->distance <= distance))
                {
                    continue;
                }

                const math::Mat4 model = math::toMatrix(reflector.transform);
                const ReflectorPlane plane = derivePlane(mesh->bounds, model, camera);

                if (math::dot(plane.normal, camera) - plane.offset < k_minCameraClearance)
                {
                    continue;
                }

                best = Candidate{.reflector = &reflector, .mesh = mesh, .model = model, .worldBounds = worldBounds,
                    .plane = plane, .distance = distance};
            }

            return best;
        }

        void writeLightingBlock(RendererState& state)
        {
            GpuFrameLighting& lighting = state.prepared.lighting;
            const PreparedReflection& reflection = state.reflection;

            lighting.reflectionViewProjection = reflection.viewProjection;
            lighting.reflectionCamera[0] = reflection.cameraPosition.x;
            lighting.reflectionCamera[1] = reflection.cameraPosition.y;
            lighting.reflectionCamera[2] = reflection.cameraPosition.z;
            lighting.reflectionCamera[3] = reflection.active ? 1.0f : 0.0f;
            lighting.reflectionPlane[0] = reflection.plane.x;
            lighting.reflectionPlane[1] = reflection.plane.y;
            lighting.reflectionPlane[2] = reflection.plane.z;
            lighting.reflectionPlane[3] = reflection.plane.w;
            lighting.reflectionTint[0] = reflection.tint.x;
            lighting.reflectionTint[1] = reflection.tint.y;
            lighting.reflectionTint[2] = reflection.tint.z;
            lighting.reflectionTint[3] = 1.0f;
        }

        void cullReflectionDraws(RendererState& state, const Candidate& chosen, const math::Mat4& cullMatrix)
        {
            PreparedReflection& reflection = state.reflection;
            const Frustum frustum = frustumFromMatrix(cullMatrix);
            const math::Vec3 eye = reflection.cameraPosition;
            std::vector<std::pair<float, uint32_t>> opaque;
            std::vector<std::pair<float, uint32_t>> blended;

            for (uint32_t index = 0; index < state.draws.size(); ++index)
            {
                DrawItem& item = state.draws[index];

                if (isViewModelDraw(item.layers))
                {
                    continue;
                }

                if (item.mesh == &chosen.mesh->mesh && item.model == chosen.model)
                {
                    item.isReflector = true;
                    continue;
                }

                if ((item.layers & reflection.layers) == 0 || item.material->shading != Shading::Lit
                    || !intersects(frustum, item.worldBounds))
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
                reflection.opaque.push_back(index);
            }

            for (const auto& [distance, index] : blended)
            {
                reflection.blended.push_back(index);
            }
        }
    } //namespace

    void prepareReflection(RendererState& state)
    {
        PreparedReflection& reflection = state.reflection;
        reflection.active = false;
        reflection.opaque.clear();
        reflection.blended.clear();

        const math::Mat4 cameraViewProjection = state.unjitteredProjection * state.view;
        const std::optional<Candidate> chosen = state.settings.reflections.enabled && !state.reflectors.empty()
            ? nearestReflector(state, frustumFromMatrix(cameraViewProjection)) : std::nullopt;

        if (!chosen)
        {
            writeLightingBlock(state);
            return;
        }

        const ReflectorPlane& plane = chosen->plane;
        const math::Vec4 worldPlane{plane.normal.x, plane.normal.y, plane.normal.z, -plane.offset};
        const math::Mat4 mirroredView = state.view * reflectionMatrix(plane);
        const math::Vec4 viewPlane = math::transpose(math::inverse(mirroredView)) * worldPlane;

        math::Mat4 flip{1.0f};
        flip[0][0] = -1.0f;

        const math::Vec3& camera = state.camera.position;
        const float height = math::dot(plane.normal, camera) - plane.offset;
        const math::Vec4 footprint = screenFootprint(chosen->worldBounds, cameraViewProjection);

        reflection.active = true;
        reflection.viewProjection = flip * obliqueProjection(state.unjitteredProjection, viewPlane) * mirroredView;
        reflection.cameraPosition = camera - plane.normal * (2.0f * height);
        reflection.plane = worldPlane;
        reflection.tint = chosen->reflector->tint;
        reflection.layers = chosen->reflector->layers;
        reflection.clipRect = {-footprint.z, footprint.y, -footprint.x, footprint.w};
        reflection.distance = chosen->distance;

        cullReflectionDraws(state, *chosen, rectCullMatrix(reflection.clipRect) * reflection.viewProjection);
        writeLightingBlock(state);
    }

    Result<std::unique_ptr<ReflectionPass>> ReflectionPass::create(RendererState& state)
    {
        return std::unique_ptr<ReflectionPass>(new ReflectionPass(state));
    }

    ReflectionPass::ReflectionPass(const RendererState& state)
        : m_state(&state)
    {
    }

    vela::graphics::PassDescription ReflectionPass::describe() const
    {
        auto target = [this](std::string_view name, vela::graphics::TextureFormat format, bool keep)
        {
            vela::graphics::AttachmentSlot slot{};
            slot.name = std::string(name);
            slot.format = format;
            slot.load = vela::graphics::LoadOp::Clear;
            slot.store = keep ? vela::graphics::StoreOp::Store : vela::graphics::StoreOp::DontCare;
            slot.clear = {0.0f, 0.0f, 0.0f, 1.0f};
            slot.scale = m_state->reflectionScale;

            return slot;
        };

        vela::graphics::PassDescription description{};
        description.colorOutputs.push_back(target(k_reflectionColorAttachment, k_sceneColorFormat, true));
        description.colorOutputs.push_back(target(k_reflectionAmbientAttachment, k_sceneColorFormat, false));
        description.colorOutputs.push_back(target(k_reflectionVelocityAttachment, k_sceneColorFormat, false));
        description.colorOutputs.push_back(target(k_reflectionSurfaceNormalAttachment, k_sceneColorFormat, false));
        description.colorOutputs.push_back(target(k_reflectionSurfaceSpecularAttachment, k_sceneColorFormat, false));
        description.depthOutput = target(k_reflectionDepthAttachment, k_depthFormat, false);
        description.inputs.push_back({std::string(k_shadowMapAttachment), vela::graphics::InputUsage::Sampled});
        description.inputs.push_back({std::string(k_environmentAttachment), vela::graphics::InputUsage::Sampled});

        return description;
    }

    void ReflectionPass::record(vela::graphics::PassRecorder& recorder)
    {
        const PreparedReflection& reflection = m_state->reflection;

        if (!reflection.active)
        {
            return;
        }

        const vela::graphics::Extent2D extent = recorder.extent();
        const auto toPixel = [](float ndc, uint32_t size)
        {
            return static_cast<int32_t>(std::clamp((ndc * 0.5f + 0.5f) * static_cast<float>(size), 0.0f,
                static_cast<float>(size)));
        };
        const int32_t x0 = toPixel(reflection.clipRect.x, extent.width);
        const int32_t y0 = toPixel(reflection.clipRect.y, extent.height);
        const int32_t x1 = std::min(toPixel(reflection.clipRect.z, extent.width) + 1, static_cast<int32_t>(extent.width));
        const int32_t y1 = std::min(toPixel(reflection.clipRect.w, extent.height) + 1, static_cast<int32_t>(extent.height));

        if (x1 <= x0 || y1 <= y0)
        {
            return;
        }

        recorder.setScissor(x0, y0, static_cast<uint32_t>(x1 - x0), static_cast<uint32_t>(y1 - y0));
        recorder.bindAttachment(k_shadowAtlasSlot, std::string(k_shadowMapAttachment), shadowCompareSampler());
        recorder.bindAttachment(k_environmentSlot, std::string(k_environmentAttachment));
        recorder.bindAttachment(k_shadowDepthSlot, std::string(k_shadowMapAttachment), shadowDepthSampler());
        recorder.bindTexture(k_planarReflectionSlot, m_state->environment.whiteLinear);

        for (const uint32_t index : reflection.opaque)
        {
            recordSceneDraw(recorder, m_state->draws[index], PbrView::Reflection);
        }

        for (const uint32_t index : reflection.blended)
        {
            recordSceneDraw(recorder, m_state->draws[index], PbrView::Reflection);
        }
    }
} //namespace lockstep::render
