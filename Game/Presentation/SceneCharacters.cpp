#include "SceneCharacters.hpp"

#include <Lockstep/Render/Renderer.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <span>
#include <string_view>

namespace game
{
    namespace
    {
        namespace anim = lockstep::anim;
        namespace math = lockstep::math;
        namespace render = lockstep::render;

        constexpr std::string_view k_viewModelTestFlag{"--viewmodel-test"};
        constexpr math::Vec3 k_viewModelEye{0.0f, 1.68f, 0.08f};
        constexpr math::Vec3 k_worldUp{0.0f, 1.0f, 0.0f};
        constexpr float k_minHorizontalLength{1e-4f};

        math::Transform viewModelPlacement(const math::Vec3& eye, const math::Vec3& direction,
            const CharacterControls& controls)
        {
            const math::Vec3 forward = math::normalize(direction);
            const math::Vec3 sideways = math::cross(forward, k_worldUp);
            const math::Vec3 right = math::length(sideways) > k_minHorizontalLength ? math::normalize(sideways)
                : math::Vec3{1.0f, 0.0f, 0.0f};
            const math::Vec3 up = math::cross(right, forward);

            math::Mat4 basis{1.0f};
            const math::Vec3 columns[3]{-right, up, forward};

            for (int column = 0; column < 3; ++column)
            {
                basis[column][0] = columns[column].x;
                basis[column][1] = columns[column].y;
                basis[column][2] = columns[column].z;
            }

            const math::Mat4 yaw = math::rotate(math::Mat4{1.0f}, math::radians(controls.viewModelYaw), k_worldUp);
            const math::Mat4 turned = basis * yaw;
            const math::Vec3& offset = controls.viewModelOffset;
            const math::Vec3 shift = right * offset.x + up * offset.y + forward * offset.z;

            return math::Transform{.position = eye + shift - math::transformDirection(turned, k_viewModelEye),
                .rotation = math::fromRotationMatrix(turned)};
        }

        void applyControls(const CharacterControls& controls, const anim::SkinnedModel& model, anim::Animator& animator)
        {
            if (model.clips.empty())
            {
                return;
            }

            const anim::PlaybackOptions options{.loop = true, .speed = controls.speed};

            if (!controls.isBlending)
            {
                const auto clip = static_cast<uint32_t>(std::clamp(controls.clip, 0, static_cast<int>(model.clips.size()) - 1));
                animator.play(clip, controls.fadeSeconds, options);
                return;
            }

            std::vector<anim::BlendSample> samples;

            for (std::size_t clip = 0; clip < model.clips.size(); ++clip)
            {
                samples.push_back(anim::BlendSample{.clip = static_cast<uint32_t>(clip),
                    .parameter = static_cast<float>(clip)});
            }

            animator.playBlend(samples, controls.fadeSeconds, options);
            animator.setParameter(controls.parameter);
        }
    } //namespace

    SceneCharacters createSceneCharacters(const lockstep::assets::Scene& scene, std::span<const char* const> arguments)
    {
        SceneCharacters characters{};
        characters.instances.resize(scene.skinnedInstances.size());
        characters.controls.isViewModel = std::any_of(arguments.begin(), arguments.end(), [](const char* argument)
        {
            return argument != nullptr && std::string_view(argument) == k_viewModelTestFlag;
        });

        return characters;
    }

    void presentSceneCharacters(lockstep::render::Renderer& renderer, const lockstep::assets::Scene& scene,
        SceneCharacters& characters, float frameSeconds, const math::Vec3& eye, const math::Vec3& direction)
    {
        const std::size_t count = std::min(characters.instances.size(), scene.skinnedInstances.size());

        for (std::size_t index = 0; index < count; ++index)
        {
            const lockstep::assets::SceneSkinnedInstance& placed = scene.skinnedInstances[index];
            const anim::SkinnedModel& model = scene.skinnedModels[placed.model];
            anim::SkinnedInstance& instance = characters.instances[index];

            applyControls(characters.controls, model, instance.animator());
            instance.update(model, frameSeconds);

            if (index == 0 && characters.controls.isViewModel)
            {
                instance.submit(renderer, model, viewModelPlacement(eye, direction, characters.controls), render::k_viewModelLayer);
                continue;
            }

            instance.submit(renderer, model, placed.transform);
        }
    }

#if LOCKSTEP_DEV_TOOLS
    void sceneCharactersDevSection(const lockstep::assets::Scene& scene, SceneCharacters& characters)
    {
        if (scene.skinnedInstances.empty() || !ImGui::CollapsingHeader("Characters"))
        {
            return;
        }

        CharacterControls& controls = characters.controls;
        const anim::SkinnedModel& model = scene.skinnedModels[scene.skinnedInstances.front().model];
        const int lastClip = static_cast<int>(model.clips.size()) - 1;
        ImGui::Text("%zu skinned, '%s': %zu joints, %zu clips", scene.skinnedInstances.size(), model.name.c_str(),
            model.skeleton.joints.size(), model.clips.size());

        if (lastClip < 0)
        {
            return;
        }

        ImGui::Checkbox("First character as viewmodel", &controls.isViewModel);

        if (controls.isViewModel)
        {
            ImGui::DragFloat3("Viewmodel offset (m)", &controls.viewModelOffset.x, 0.005f, -1.0f, 1.0f);
            ImGui::SliderFloat("Viewmodel yaw (deg)", &controls.viewModelYaw, -180.0f, 180.0f);
        }

        ImGui::Checkbox("Blend all clips by parameter", &controls.isBlending);

        if (controls.isBlending)
        {
            ImGui::SliderFloat("Parameter", &controls.parameter, 0.0f, static_cast<float>(lastClip));
        }
        else
        {
            controls.clip = std::clamp(controls.clip, 0, lastClip);
            ImGui::SliderInt("Clip", &controls.clip, 0, lastClip, model.clips[static_cast<std::size_t>(controls.clip)].name.c_str());
        }

        ImGui::SliderFloat("Crossfade (s)", &controls.fadeSeconds, 0.0f, 2.0f);
        ImGui::SliderFloat("Speed", &controls.speed, 0.0f, 3.0f);

        const anim::Animator& animator = characters.instances.front().animator();
        ImGui::Text("Phase %.2f%s", animator.phase(), animator.isFading() ? "  (crossfading)" : "");
    }
#endif
} //namespace game
