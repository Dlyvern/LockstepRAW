#include "Characters/CharacterStains.hpp"

#include <Lockstep/Render/Renderer.hpp>

#if LOCKSTEP_DEV_TOOLS
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;

        constexpr float k_minSeconds{0.1f};
        constexpr float k_maxStainBlood{1.0f};

        math::Vec3 modelPoint(const math::Transform& placement, const math::Vec3& point)
        {
            const math::Vec3 local = math::conjugate(placement.rotation) * (point - placement.position);
            return math::Vec3{local.x / std::max(std::abs(placement.scale.x), 1e-4f),
                local.y / std::max(std::abs(placement.scale.y), 1e-4f),
                local.z / std::max(std::abs(placement.scale.z), 1e-4f)};
        }

        float approach(float value, float target, float step)
        {
            return value < target ? std::min(value + step, target) : std::max(value - step, target);
        }
    } //namespace

    void addBodyStain(BodyStains& body, const lockstep::anim::SkinnedInstance& instance,
        const math::Transform& placement, const math::Vec3& point, float radius, float dirt)
    {
        const math::Vec3 bindPoint = instance.bindPosition(modelPoint(placement, point));

        for (uint32_t index = 0; index < body.count; ++index)
        {
            render::SurfaceStain& stain = body.stains[index];

            if (math::length(stain.position - bindPoint) < stain.radius * 0.5f)
            {
                stain.radius = std::max(stain.radius, radius) * 1.15f;
                stain.blood = k_maxStainBlood;
                stain.freshness = 1.0f;
                stain.dirt = std::max(stain.dirt, dirt);
                return;
            }
        }

        uint32_t slot = body.count;

        if (body.count < body.stains.size())
        {
            ++body.count;
        }
        else
        {
            slot = body.next;
            body.next = (body.next + 1) % static_cast<uint32_t>(body.stains.size());
        }

        body.stains[slot] = render::SurfaceStain{.position = bindPoint, .radius = radius, .blood = k_maxStainBlood,
            .freshness = 1.0f, .dirt = dirt};
    }

    void prepareBodyStains(render::Renderer& renderer, BodyStains& body, const CharacterStainTuning& tuning,
        float rain, float seconds)
    {
        body.surface = render::k_noCharacterSurface;
        const float drying = seconds / std::max(tuning.drySeconds, k_minSeconds);

        for (uint32_t index = 0; index < body.count; ++index)
        {
            body.stains[index].freshness = std::max(body.stains[index].freshness - drying, 0.0f);
        }

        const float rate = rain > body.soak ? seconds / std::max(tuning.soakSeconds, k_minSeconds)
                                            : seconds / std::max(tuning.dryOffSeconds, k_minSeconds);
        body.soak = approach(body.soak, std::clamp(rain, 0.0f, 1.0f), rate);

        if (!tuning.enabled)
        {
            return;
        }

        render::CharacterSurface surface{.soak = body.soak, .grime = tuning.grime, .noiseSeed = body.seed,
            .stains = body.stains, .stainCount = body.count};
        body.surface = renderer.submitCharacterSurface(surface);
    }

    void clearBodyStains(BodyStains& body)
    {
        body.count = 0;
        body.next = 0;
        body.soak = 0.0f;
        body.surface = render::k_noCharacterSurface;
    }

#if LOCKSTEP_DEV_TOOLS
    void characterStainsDevSection(CharacterStainTuning& tuning)
    {
        if (!ImGui::TreeNode("Blood, dirt and rain on characters"))
        {
            return;
        }

        ImGui::Checkbox("Character stains", &tuning.enabled);
        ImGui::SliderFloat("Hit blood radius (m)", &tuning.hitRadius, 0.02f, 0.2f, "%.3f");
        ImGui::SliderFloat("Stump blood radius (m)", &tuning.severRadius, 0.05f, 0.3f, "%.3f");
        ImGui::SliderFloat("Dirt around hits", &tuning.hitDirt, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("General grime", &tuning.grime, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Blood drying (s)", &tuning.drySeconds, 5.0f, 600.0f, "%.0f");
        ImGui::SliderFloat("Rain soak (s)", &tuning.soakSeconds, 1.0f, 120.0f, "%.0f");
        ImGui::SliderFloat("Drying indoors (s)", &tuning.dryOffSeconds, 10.0f, 900.0f, "%.0f");
        ImGui::Checkbox("Stain every NPC (drill)", &tuning.drill);
        ImGui::TreePop();
    }
#endif
} //namespace game
