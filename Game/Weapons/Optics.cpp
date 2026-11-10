#include "Weapons/Optics.hpp"

#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Render/Renderer.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace game
{
    namespace
    {
        namespace assets = lockstep::assets;
        namespace render = lockstep::render;

        struct OpticSource
        {
            OpticKind kind;
            std::string_view scene;
            std::string_view lens;
            std::string_view eyepiece;
            std::string_view objective;
        };

        constexpr OpticSource k_scopeSource{.kind = OpticKind::Scope4x, .scene = "characters/scope4x/scene",
            .lens = "Scope_LensRear", .eyepiece = "Scope_Eyepiece", .objective = "Scope_Objective"};
        constexpr OpticSource k_holographicSource{.kind = OpticKind::Holographic, .scene = "characters/eotech/scene",
            .lens = "Eotech_WindowRear", .eyepiece = "Optic_Eyepiece", .objective = "Optic_Objective"};

        std::optional<OpticModel> loadOpticModel(render::Renderer& renderer, const assets::FileSystem& files,
            const OpticSource& source)
        {
            const std::string_view path = source.scene;

            if (!files.exists(path))
            {
                std::clog << "Game: optic '" << path << "' not mounted (add --content Content/cooked/characters.pak)\n";
                return std::nullopt;
            }

            auto loaded = assets::loadCookedScene(renderer, files, path);

            if (!loaded)
            {
                std::clog << "Game: optic '" << path << "' not loaded: " << loaded.error().message << '\n';
                return std::nullopt;
            }

            OpticModel model{};
            model.kind = source.kind;
            model.scene = std::move(loaded).value();
            const std::optional<math::Transform> eyepiece = assets::findNode(model.scene, source.eyepiece);
            const std::optional<math::Transform> objective = assets::findNode(model.scene, source.objective);

            if (!eyepiece || !objective)
            {
                std::clog << "Game: optic '" << path << "' has no " << source.eyepiece << " or " << source.objective << " node\n";
                return std::nullopt;
            }

            model.eyepiece = *eyepiece;
            model.objective = *objective;

            for (const assets::SceneInstance& instance : model.scene.instances)
            {
                const assets::SceneMesh& mesh = model.scene.meshes[instance.mesh];
                const bool isLens = instance.node < model.scene.nodes.size()
                    && model.scene.nodes[instance.node].name == source.lens;
                model.parts.push_back(OpticPart{.mesh = mesh.mesh, .material = mesh.material,
                    .local = instance.transform, .isLens = isLens});
            }

            return model;
        }

#if LOCKSTEP_DEV_TOOLS
        const char* opticName(OpticKind kind)
        {
            switch (kind)
            {
                case OpticKind::None:
                {
                    return "Iron sights";
                }
                case OpticKind::Holographic:
                {
                    return "Holographic sight";
                }
                case OpticKind::Scope4x:
                {
                    return "4x scope";
                }
            }

            return "?";
        }
#endif
    } //namespace

    Optics loadOptics(render::Renderer& renderer, const assets::FileSystem& files)
    {
        return Optics{.holographic = loadOpticModel(renderer, files, k_holographicSource),
            .scope = loadOpticModel(renderer, files, k_scopeSource)};
    }

    const OpticModel* opticModel(const Optics& optics, OpticKind kind)
    {
        const std::optional<OpticModel>& model = kind == OpticKind::Holographic ? optics.holographic : optics.scope;

        return kind != OpticKind::None && model ? &*model : nullptr;
    }

    const OpticModel* weaponOptic(const Optics& optics, WeaponKind weapon)
    {
        return weapon == WeaponKind::Glock ? nullptr : opticModel(optics, optics.loadout.byWeapon[weaponIndex(weapon)]);
    }

    math::Transform placeOnMount(const math::Transform& mount, const math::Transform& local)
    {
        return math::Transform{.position = mount.position + mount.rotation * (local.position * mount.scale),
            .rotation = mount.rotation * local.rotation,
            .scale = mount.scale * local.scale};
    }

    void drawOptic(render::Renderer& renderer, const OpticModel& model, const math::Transform& mount,
        render::RenderLayers layers)
    {
        for (const OpticPart& part : model.parts)
        {
            renderer.submit(part.mesh, part.material, placeOnMount(mount, part.local), layers);
        }
    }

#if LOCKSTEP_DEV_TOOLS
    void opticsDevSection(Optics& optics)
    {
        if (!ImGui::TreeNode("Optics"))
        {
            return;
        }

        for (const WeaponKind weapon : {WeaponKind::M4, WeaponKind::Kriss})
        {
            OpticKind& choice = optics.loadout.byWeapon[weaponIndex(weapon)];
            const std::string label = std::string(weaponName(weapon)) + " optic";

            if (ImGui::BeginCombo(label.c_str(), opticName(choice)))
            {
                for (const OpticKind kind : {OpticKind::None, OpticKind::Holographic, OpticKind::Scope4x})
                {
                    if (ImGui::Selectable(opticName(kind), choice == kind))
                    {
                        choice = kind;
                    }
                }

                ImGui::EndCombo();
            }
        }

        if (!optics.holographic || !optics.scope)
        {
            ImGui::TextUnformatted("An optic is not loaded (characters/eotech or characters/scope4x missing)");
        }

        HolographicTuning& holographic = optics.holographicTuning;
        ImGui::SeparatorText("Holographic sight");
        ImGui::SliderFloat("Brightness", &holographic.brightness, 0.0f, 3.0f);
        ImGui::ColorEdit3("Reticle radiance", &holographic.radiance.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("Eye distance (m)", &holographic.eyeDistance, 0.04f, 0.25f);
        ImGui::SliderFloat("Ring radius (MOA)", &holographic.ringRadius, 5.0f, 60.0f);
        ImGui::SliderFloat("Ring width (MOA)", &holographic.ringWidth, 0.5f, 6.0f);
        ImGui::SliderFloat("Dot radius (MOA)", &holographic.dotRadius, 0.25f, 3.0f);
        ImGui::SeparatorText("4x scope");
        OpticTuning& tuning = optics.scopeTuning;
        float fieldDegrees = math::degrees(tuning.fieldOfView);
        ImGui::SliderFloat("Magnification", &tuning.magnification, 1.0f, 12.0f);

        if (ImGui::SliderFloat("True field of view (deg)", &fieldDegrees, 1.0f, 20.0f))
        {
            tuning.fieldOfView = math::radians(fieldDegrees);
        }

        ImGui::SliderFloat("Eye relief (m)", &tuning.eyeRelief, 0.03f, 0.15f);
        ImGui::SliderFloat("Eye box radius (m)", &tuning.exitPupil, 0.001f, 0.03f);
        ImGui::SliderFloat("Sway x while scoped", &tuning.swayScale, 0.0f, 4.0f);
        ImGui::SliderFloat("Sensitivity power", &tuning.sensitivityPower, 0.0f, 1.5f);
        ImGui::SliderFloat("Outer blur while scoped", &tuning.outerBlur, 0.0f, 1.0f);
        ImGui::Checkbox("Illuminated dot", &tuning.illuminated);
        ImGui::ColorEdit3("Dot radiance", &tuning.illumination.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
        render::ScopeReticle& reticle = tuning.reticle;
        ImGui::SliderFloat("Line width", &reticle.lineWidth, 0.0f, 0.03f);
        ImGui::SliderFloat("Post width", &reticle.postWidth, 0.0f, 0.1f);
        ImGui::SliderFloat("Post gap", &reticle.postGap, 0.05f, 1.0f);
        ImGui::SliderFloat("Stadia spacing", &reticle.stadiaSpacing, 0.0f, 0.2f);
        ImGui::SliderFloat("Stadia length", &reticle.stadiaLength, 0.0f, 0.06f);
        ImGui::SliderFloat("Dot radius", &reticle.dotRadius, 0.0f, 0.05f);
        ImGui::TreePop();
    }
#endif
} //namespace game
