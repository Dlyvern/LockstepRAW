#include "Cloth/Curtains.hpp"

#include "Cloth/CurtainMesh.hpp"
#include "GameState.hpp"
#include "Level/SceneLevel.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Assets/Scene.hpp>
#include <Lockstep/Assets/SceneCollision.hpp>
#include <Lockstep/Render/Renderer.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <string>
#include <string_view>

namespace game
{
    namespace
    {
        namespace math = lockstep::math;
        namespace render = lockstep::render;
        namespace assets = lockstep::assets;

        constexpr std::string_view k_clothPrefix{"cloth_"};
        constexpr float k_stepSeconds{1.0f / 60.0f};
        constexpr uint32_t k_maxStepsPerFrame{3};
        constexpr float k_personHeight{1.8f};
        constexpr float k_personReach{0.6f};
        constexpr float k_calmMotion{0.0008f};
        constexpr float k_calmSecondsToRest{1.5f};
        constexpr float k_kickRadius{0.4f};
        constexpr float k_minWindToWake{1.5f};
        constexpr math::Vec3 k_fabricColor{0.42f, 0.55f, 0.52f};
        constexpr float k_fabricRoughness{0.92f};
        constexpr float k_playerChestHeight{1.5f};
        constexpr float k_drillSeconds{3.0f};
        constexpr float k_npcChestHeight{1.4f};

        bool isClothName(std::string_view name)
        {
            if (name.size() < k_clothPrefix.size())
            {
                return false;
            }

            for (std::size_t index = 0; index < k_clothPrefix.size(); ++index)
            {
                if (std::tolower(static_cast<unsigned char>(name[index])) != k_clothPrefix[index])
                {
                    return false;
                }
            }

            return true;
        }

        bool isClothInstance(const assets::Scene& scene, const assets::SceneInstance& instance)
        {
            for (uint32_t node = instance.node; node != assets::k_noNode && node < scene.nodes.size();
                node = scene.nodes[node].parent)
            {
                if (isClothName(scene.nodes[node].name))
                {
                    return true;
                }
            }

            return false;
        }

        bool isInside(std::span<const InteriorBox> boxes, const math::Vec3& point)
        {
            return std::any_of(boxes.begin(), boxes.end(), [&point](const InteriorBox& box)
            {
                return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y && point.y <= box.max.y
                    && point.z >= box.min.z && point.z <= box.max.z;
            });
        }

        math::Vec3 toPanel(const CurtainPanel& panel, const math::Vec3& world)
        {
            return math::conjugate(panel.transform.rotation) * (world - panel.transform.position);
        }

        math::Vec3 directionToPanel(const CurtainPanel& panel, const math::Vec3& world)
        {
            return math::conjugate(panel.transform.rotation) * world;
        }

        void release(Curtains& curtains, render::Renderer& renderer)
        {
            for (const CurtainPanel& panel : curtains.panels)
            {
                renderer.destroyMesh(panel.mesh);
            }

            curtains.panels.clear();
        }

        bool ensureMaterial(Curtains& curtains, render::Renderer& renderer)
        {
            if (curtains.material.isValid())
            {
                return true;
            }

            render::PbrMaterialDescription fabric{};
            fabric.baseColorFactor = math::Vec4{k_fabricColor.x, k_fabricColor.y, k_fabricColor.z, 1.0f};
            fabric.metallicFactor = 0.0f;
            fabric.roughnessFactor = k_fabricRoughness;
            fabric.doubleSided = true;
            fabric.detail = render::SurfaceDetail::Fibrous;
            fabric.detailScale = 2.0f;
            fabric.shading = render::MaterialShading{.model = render::ShadingModel::Fabric, .strength = 1.0f,
                .sheenRoughness = 0.7f};
            auto material = renderer.createMaterial(fabric, render::MeshKind::Skinned);

            if (!material)
            {
                std::clog << "Game: curtain material not created: " << material.error().message << '\n';
                return false;
            }

            curtains.material = material.value();
            return true;
        }

        std::optional<CurtainPanel> makePanel(const Curtains& curtains, render::Renderer& renderer,
            const assets::Scene& scene, uint32_t instanceIndex)
        {
            const assets::SceneInstance& instance = scene.instances[instanceIndex];
            const math::Aabb& local = scene.meshes[instance.mesh].localBounds;
            const math::Vec3& scale = instance.transform.scale;
            const math::Vec3 size{(local.max.x - local.min.x) * std::abs(scale.x),
                (local.max.y - local.min.y) * std::abs(scale.y), (local.max.z - local.min.z) * std::abs(scale.z)};
            const bool acrossZ = size.z > size.x;
            const math::Vec3 topCentre{(local.min.x + local.max.x) * 0.5f, local.max.y, (local.min.z + local.max.z) * 0.5f};
            const math::Vec3 top = instance.transform.position
                + instance.transform.rotation * math::Vec3{topCentre.x * scale.x, topCentre.y * scale.y, topCentre.z * scale.z};
            const math::Quat turn = acrossZ ? math::angleAxis(-math::k_pi * 0.5f, math::Vec3{0.0f, 1.0f, 0.0f}) : math::Quat{};

            CurtainPanel panel{};
            panel.instance = instanceIndex;
            panel.transform = math::Transform{.position = top, .rotation = instance.transform.rotation * turn};
            panel.cloth = createClothPanel(acrossZ ? size.z : size.x, size.y, curtains.tuning.spacing);

            const CurtainMeshData mesh = buildCurtainMesh(panel.cloth);
            auto created = renderer.createMesh(std::span<const render::SkinnedPbrVertex>(mesh.vertices), mesh.indices);

            if (!created)
            {
                std::clog << "Game: curtain mesh not created: " << created.error().message << '\n';
                return std::nullopt;
            }

            panel.mesh = created.value();
            panel.bounds = mesh.bounds;
            return panel;
        }

        void build(Curtains& curtains, render::Renderer& renderer, assets::Scene* scene,
            std::span<const InteriorBox> interiors)
        {
            release(curtains, renderer);
            curtains.scene = scene;
            curtains.stepCarry = 0.0f;

            if (scene == nullptr || !ensureMaterial(curtains, renderer))
            {
                return;
            }

            uint32_t largest = 0;

            for (uint32_t index = 0; index < scene->instances.size(); ++index)
            {
                if (!isClothInstance(*scene, scene->instances[index]))
                {
                    continue;
                }

                std::optional<CurtainPanel> panel = makePanel(curtains, renderer, *scene, index);

                if (!panel)
                {
                    continue;
                }

                panel->isOutdoors = !isInside(interiors, panel->transform.position - math::Vec3{0.0f, 0.5f, 0.0f});
                largest = std::max(largest, static_cast<uint32_t>(panel->cloth.positions.size()));
                scene->instances[index].isVisible = false;
                curtains.panels.push_back(std::move(*panel));
            }

            curtains.restPalette.assign(largest, math::Mat4{1.0f});

            if (!curtains.panels.empty())
            {
                std::clog << "Game: " << curtains.panels.size() << " cloth panels\n";
            }
        }

        bool isPersonNear(const CurtainPanel& panel, const math::Vec3& foot)
        {
            const math::Vec3 local = toPanel(panel, foot);
            const float halfWidth = panel.cloth.width * 0.5f + k_personReach;

            return std::abs(local.x) < halfWidth && std::abs(local.z) < k_personReach + 0.3f
                && local.y > -panel.cloth.height - k_personHeight && local.y < 0.5f;
        }

        void kickByShots(CurtainPanel& panel, const CurtainFrame& frame, float kick)
        {
            for (const BulletPath& shot : frame.shots)
            {
                const math::Vec3 from = toPanel(panel, shot.from);
                const math::Vec3 to = toPanel(panel, shot.to);

                if ((from.z > 0.0f) == (to.z > 0.0f))
                {
                    continue;
                }

                const float t = from.z / (from.z - to.z);
                const math::Vec3 hit = from + (to - from) * t;

                if (std::abs(hit.x) > panel.cloth.width * 0.5f || hit.y > 0.0f || hit.y < -panel.cloth.height)
                {
                    continue;
                }

                panel.isAwake = true;
                panel.calmSeconds = 0.0f;
                kickCloth(panel.cloth, hit, math::normalize(to - from) * kick, k_kickRadius, k_stepSeconds);
            }
        }

        void simulate(CurtainPanel& panel, const Curtains& curtains, const CurtainFrame& frame, uint32_t steps)
        {
            bool pushed = false;

            for (const math::Vec3& foot : frame.feet)
            {
                pushed = pushed || isPersonNear(panel, foot);
            }

            const math::Vec3 wind = panel.isOutdoors ? directionToPanel(panel, frame.wind) : math::Vec3{0.0f, 0.0f, 0.0f};

            if (pushed || math::length(wind) > k_minWindToWake)
            {
                panel.isAwake = true;
                panel.calmSeconds = 0.0f;
            }

            kickByShots(panel, frame, curtains.tuning.shotKick);

            if (!panel.isAwake)
            {
                return;
            }

            const ClothForces forces{.gravity = directionToPanel(panel, math::Vec3{0.0f, -9.81f, 0.0f}), .wind = wind};

            for (uint32_t step = 0; step < steps; ++step)
            {
                stepCloth(panel.cloth, forces, k_stepSeconds);

                for (const math::Vec3& foot : frame.feet)
                {
                    static_cast<void>(pushOutOfCapsule(panel.cloth, toPanel(panel, foot), k_personHeight,
                        curtains.tuning.personRadius));
                }
            }

            const bool isCalm = steps > 0 && clothMotion(panel.cloth) < k_calmMotion && !pushed;
            panel.calmSeconds = isCalm ? panel.calmSeconds + frame.worldSeconds : 0.0f;

            if (panel.calmSeconds > k_calmSecondsToRest)
            {
                restCloth(panel.cloth);
                panel.isAwake = false;
            }
        }

        void shake(CurtainPanel& panel)
        {
            panel.isAwake = true;
            panel.calmSeconds = 0.0f;
            kickCloth(panel.cloth, math::Vec3{0.0f, -panel.cloth.height * 0.6f, 0.0f}, math::Vec3{0.0f, 0.0f, 3.0f},
                panel.cloth.width, k_stepSeconds);
        }

        void draw(CurtainPanel& panel, const Curtains& curtains, render::Renderer& renderer,
            render::JointPalette& restPalette)
        {
            if (!panel.isAwake)
            {
                panel.previousJoints.clear();

                if (restPalette.count == 0)
                {
                    restPalette = renderer.submitJointPalette(curtains.restPalette, curtains.restPalette);
                }

                renderer.submitSkinned(panel.mesh, curtains.material, panel.transform, restPalette, panel.bounds);
                return;
            }

            clothJointMatrices(panel.cloth, panel.joints);

            if (panel.previousJoints.size() != panel.joints.size())
            {
                panel.previousJoints.assign(panel.joints.size(), math::Mat4{1.0f});
            }

            const render::JointPalette palette = renderer.submitJointPalette(panel.joints, panel.previousJoints);
            renderer.submitSkinned(panel.mesh, curtains.material, panel.transform, palette, panel.bounds);
            panel.previousJoints = panel.joints;
        }
    } //namespace

    uint32_t removeClothColliders(SceneLevel& level, lockstep::physics::PhysicsWorld& physics)
    {
        uint32_t removed = 0;

        for (uint32_t index = 0; index < level.scene.instances.size(); ++index)
        {
            if (isClothInstance(level.scene, level.scene.instances[index])
                && assets::removeInstanceCollider(level.collision, physics, index))
            {
                ++removed;
            }
        }

        return removed;
    }

    void presentCurtains(Curtains& curtains, render::Renderer& renderer, assets::Scene* scene, const CurtainFrame& frame)
    {
        const std::size_t instanceCount = scene != nullptr ? scene->instances.size() : 0;

        if (scene != curtains.scene || instanceCount != curtains.instanceCount)
        {
            build(curtains, renderer, scene, frame.interiors);
            curtains.instanceCount = instanceCount;
        }

        if (curtains.panels.empty())
        {
            return;
        }

        curtains.stepCarry += std::max(frame.worldSeconds, 0.0f);
        const auto steps = std::min(static_cast<uint32_t>(curtains.stepCarry / k_stepSeconds), k_maxStepsPerFrame);
        curtains.stepCarry = std::min(curtains.stepCarry - static_cast<float>(steps) * k_stepSeconds, k_stepSeconds);
        const bool canMove = curtains.tuning.enabled && frame.simulate;
        render::JointPalette restPalette{};
        curtains.drillSeconds += frame.worldSeconds;
        const bool isDrillKick = curtains.tuning.shakeDrill && curtains.drillSeconds > k_drillSeconds;
        curtains.drillSeconds = isDrillKick ? 0.0f : curtains.drillSeconds;

        for (CurtainPanel& panel : curtains.panels)
        {
            const bool isNear = math::length(panel.transform.position - frame.eye) < curtains.tuning.simulateDistance;

            if (canMove && isNear && isDrillKick)
            {
                shake(panel);
            }

            if (canMove && isNear)
            {
                simulate(panel, curtains, frame, steps);
            }
            else if (panel.isAwake)
            {
                restCloth(panel.cloth);
                panel.isAwake = false;
            }

            scene->instances[panel.instance].isVisible = false;
            draw(panel, curtains, renderer, restPalette);
        }
    }

    void presentLevelCurtains(lockstep::app::GameState& state, lockstep::app::EngineContext& engine,
        const CombatAudioFrame& audio, float worldSeconds)
    {
        std::vector<math::Vec3> feet;

        for (const std::optional<math::Vec3>& chest : audio.positions)
        {
            if (chest)
            {
                feet.push_back(*chest - math::Vec3{0.0f, k_playerChestHeight, 0.0f});
            }
        }

        for (const NpcShotSource& npc : audio.npcs)
        {
            feet.push_back(npc.position - math::Vec3{0.0f, k_npcChestHeight, 0.0f});
        }

        std::vector<BulletPath> shots;

        for (const CombatImpact& impact : state.combatAudio.impacts)
        {
            if (!impact.isExit)
            {
                shots.push_back(BulletPath{.from = audio.eye, .to = impact.point});
            }
        }

        presentCurtains(state.curtains, engine.renderer, state.level ? &state.level->scene : nullptr, CurtainFrame{
            .scene = state.level ? &state.level->scene : nullptr,
            .eye = audio.listener,
            .feet = feet,
            .shots = shots,
            .interiors = state.combatAudio.interiors,
            .wind = windVelocity(state.weather.wind),
            .simulate = engine.renderer.graphicsQuality().cloth,
            .worldSeconds = worldSeconds});
    }

#if LOCKSTEP_DEV_TOOLS
    void curtainDevSection(Curtains& curtains)
    {
        if (!ImGui::TreeNode("Curtains"))
        {
            return;
        }

        CurtainTuning& tuning = curtains.tuning;
        ImGui::Checkbox("Simulate", &tuning.enabled);
        ImGui::SliderFloat("Simulate within (m)", &tuning.simulateDistance, 2.0f, 40.0f, "%.0f");
        ImGui::SliderFloat("Person radius (m)", &tuning.personRadius, 0.1f, 0.6f, "%.2f");
        ImGui::SliderFloat("Bullet kick (m/s)", &tuning.shotKick, 0.0f, 15.0f, "%.1f");
        const auto awake = std::count_if(curtains.panels.begin(), curtains.panels.end(),
            [](const CurtainPanel& panel) { return panel.isAwake; });
        ImGui::Text("%zu panels, %td moving", curtains.panels.size(), awake);

        if (ImGui::Button("Shake every panel"))
        {
            for (CurtainPanel& panel : curtains.panels)
            {
                shake(panel);
            }
        }

        ImGui::TreePop();
    }
#endif
} //namespace game
