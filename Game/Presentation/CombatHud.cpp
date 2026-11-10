#include "Presentation/CombatHud.hpp"

#include "Presentation/Visuals.hpp"

#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/Render/Text.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace game
{
    namespace
    {
        namespace render = lockstep::render;

        constexpr math::Vec3 k_worldUp{0.0f, 1.0f, 0.0f};
        constexpr float k_markDistance{0.5f};
        constexpr float k_markRadius{0.045f};
        constexpr float k_markSize{0.0035f};
        constexpr float k_markSpread{0.12f};
        constexpr float k_markSeconds{1.6f};
        constexpr float k_hurtFlashPerHit{0.6f};
        constexpr float k_hurtFlashDecay{2.5f};
        constexpr float k_hurtFlashAmount{0.35f};
        constexpr float k_downedFade{0.3f};
        constexpr math::Vec3 k_hurtColour{0.5f, 0.0f, 0.0f};
        constexpr float k_hitSeconds{0.15f};
        constexpr float k_killSeconds{0.35f};
        constexpr float k_hitOffset{0.008f};
        constexpr float k_hitSize{0.0022f};
        constexpr float k_restartFadeRate{1.2f};
        constexpr std::size_t k_maxMarks{6};
        constexpr math::Vec2 k_ammoPosition{0.96f, 0.9f};
        constexpr math::Vec2 k_statePosition{0.5f, 0.66f};
        constexpr math::Vec2 k_detailPosition{0.5f, 0.70f};
        constexpr math::Vec4 k_ammoColour{0.9f, 0.92f, 0.95f, 0.9f};
        constexpr math::Vec4 k_emptyColour{1.0f, 0.4f, 0.3f, 0.95f};
        constexpr math::Vec4 k_downedColour{1.0f, 0.35f, 0.3f, 1.0f};
        constexpr math::Vec4 k_promptColour{1.0f, 0.86f, 0.55f, 1.0f};
        constexpr float k_ammoSize{30.0f};
        constexpr float k_stateSize{28.0f};
        constexpr float k_detailSize{22.0f};

        void takeCounters(CombatHud& hud, const PlayerStatus& own)
        {
            if (!hud.hasCounters)
            {
                hud.damageCount = own.damageCount;
                hud.hitCount = own.hitCount;
                hud.killCount = own.killCount;
                hud.hasCounters = true;
                return;
            }

            if (own.damageCount != hud.damageCount)
            {
                hud.damageCount = own.damageCount;
                hud.hurtFlash = std::min(hud.hurtFlash + k_hurtFlashPerHit, 1.0f);

                if (hud.marks.size() >= k_maxMarks)
                {
                    hud.marks.erase(hud.marks.begin());
                }

                hud.marks.push_back(DamageMark{.yaw = dequantizeYaw(own.damageYaw), .seconds = k_markSeconds});
            }

            if (own.killCount != hud.killCount)
            {
                hud.killCount = own.killCount;
                hud.hitCount = own.hitCount;
                hud.hitSeconds = k_killSeconds;
                hud.isKill = true;
            }
            else if (own.hitCount != hud.hitCount)
            {
                hud.hitCount = own.hitCount;
                hud.hitSeconds = std::max(hud.hitSeconds, k_hitSeconds);
            }
        }

        void age(CombatHud& hud, const CombatHudFrame& frame)
        {
            hud.hurtFlash = std::max(hud.hurtFlash - k_hurtFlashDecay * frame.seconds, 0.0f);
            hud.hitSeconds = std::max(hud.hitSeconds - frame.seconds, 0.0f);
            hud.isKill = hud.isKill && hud.hitSeconds > 0.0f;
            const float target = frame.isRestarting ? 1.0f : 0.0f;
            const float step = k_restartFadeRate * frame.seconds;
            hud.restartFade += std::clamp(target - hud.restartFade, -step, step);

            for (DamageMark& mark : hud.marks)
            {
                mark.seconds -= frame.seconds;
            }

            std::erase_if(hud.marks, [](const DamageMark& mark) { return mark.seconds <= 0.0f; });
        }

        void submitMark(render::Renderer& renderer, render::MaterialHandle material, const Visuals& visuals,
            const math::Vec3& position, float size)
        {
            renderer.submit(visuals.cube, material, math::Transform{.position = position,
                .scale = math::Vec3{size, size, size}});
        }

        void drawMarks(render::Renderer& renderer, const Visuals& visuals, const CombatHud& hud,
            const CombatHudFrame& frame)
        {
            const math::Vec3 right = math::normalize(math::cross(frame.direction, k_worldUp));
            const math::Vec3 up = math::cross(right, frame.direction);
            const math::Vec3 centre = frame.eye + frame.direction * k_markDistance;

            for (const DamageMark& mark : hud.marks)
            {
                const float relative = std::remainder(mark.yaw - frame.lookYaw, 2.0f * math::k_pi);

                for (const float offset : {-k_markSpread, 0.0f, k_markSpread})
                {
                    const float angle = relative + offset;
                    submitMark(renderer, visuals.healthFull, visuals,
                        centre + (up * std::cos(angle) - right * std::sin(angle)) * k_markRadius, k_markSize);
                }
            }

            if (hud.hitSeconds <= 0.0f)
            {
                return;
            }

            for (const float x : {-1.0f, 1.0f})
            {
                for (const float y : {-1.0f, 1.0f})
                {
                    submitMark(renderer, hud.isKill ? visuals.healthFull : visuals.crosshair, visuals,
                        centre + (right * x + up * y) * k_hitOffset, k_hitSize);
                }
            }
        }

        void drawTexts(render::Renderer& renderer, const CombatHudFrame& frame)
        {
            const PlayerStatus& own = frame.own;

            if (frame.isArmed && !own.isDowned)
            {
                std::string ammo = std::to_string(own.magazine) + " / " + std::to_string(own.reserve);

                if (own.isReloading)
                {
                    ammo = "RELOADING  " + ammo;
                }

                renderer.submitText(ammo, k_ammoPosition, render::TextStyle{.size = k_ammoSize,
                    .color = own.magazine > 0 || own.isReloading ? k_ammoColour : k_emptyColour,
                    .align = render::TextAlign::Right});
            }

            if (frame.isRestarting)
            {
                return;
            }

            if (own.isDowned)
            {
                renderer.submitText("DOWN  " + std::to_string(own.bleedSeconds) + " s", k_statePosition,
                    render::TextStyle{.size = k_stateSize, .color = k_downedColour, .align = render::TextAlign::Center});
                const std::string detail = own.reviveProgress > 0.0f
                    ? "Being revived  " + std::to_string(static_cast<int>(own.reviveProgress * 100.0f)) + "%"
                    : std::string("Crawl to cover. Your partner can revive you.");
                renderer.submitText(detail, k_detailPosition,
                    render::TextStyle{.size = k_detailSize, .color = k_ammoColour, .align = render::TextAlign::Center});
                return;
            }

            if (frame.partner.isJoined && frame.partner.isDowned)
            {
                const std::string text = frame.canRevive
                    ? (frame.partner.reviveProgress > 0.0f
                        ? "Reviving  " + std::to_string(static_cast<int>(frame.partner.reviveProgress * 100.0f)) + "%"
                        : std::string("Hold F to revive your partner"))
                    : "Partner down  " + std::to_string(frame.partner.bleedSeconds) + " s";
                renderer.submitText(text, k_statePosition, render::TextStyle{.size = k_stateSize,
                    .color = frame.canRevive ? k_promptColour : k_downedColour,
                    .align = render::TextAlign::Center});
            }
        }

        void applyFade(render::Renderer& renderer, const CombatHud& hud, const CombatHudFrame& frame)
        {
            const float black = std::max(frame.hudFade, hud.restartFade);

            if (black > 0.01f)
            {
                renderer.setScreenFade(black);
                return;
            }

            const float red = std::max(hud.hurtFlash * k_hurtFlashAmount, frame.own.isDowned ? k_downedFade : 0.0f);
            renderer.setScreenFade(red, k_hurtColour);
        }
    } //namespace

    void presentCombatHud(render::Renderer& renderer, const Visuals& visuals, CombatHud& hud,
        const CombatHudFrame& frame)
    {
        takeCounters(hud, frame.own);
        age(hud, frame);
        drawMarks(renderer, visuals, hud, frame);
        drawTexts(renderer, frame);
        applyFade(renderer, hud, frame);
    }
} //namespace game
