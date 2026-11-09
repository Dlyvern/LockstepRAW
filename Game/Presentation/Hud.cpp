#include "Presentation/Hud.hpp"

#include "Level/RoomState.hpp"

#include <Lockstep/Render/Renderer.hpp>
#include <Lockstep/Render/Text.hpp>

#include <algorithm>
#include <iostream>

namespace game
{
    namespace
    {
        namespace math = lockstep::math;
        namespace render = lockstep::render;

        constexpr std::size_t k_maxLines{4};
        constexpr float k_lineSeconds{5.0f};
        constexpr float k_lineFadeSeconds{0.6f};
        constexpr float k_lineSpacing{0.04f};
        constexpr math::Vec2 k_promptPosition{0.5f, 0.56f};
        constexpr math::Vec2 k_objectivePosition{0.03f, 0.04f};
        constexpr math::Vec2 k_linesPosition{0.5f, 0.64f};
        constexpr math::Vec2 k_captionPosition{0.5f, 0.47f};
        constexpr math::Vec4 k_promptColor{1.0f, 0.86f, 0.55f, 1.0f};
        constexpr math::Vec4 k_objectiveColor{0.85f, 0.88f, 0.9f, 0.9f};
        constexpr math::Vec4 k_lineColor{1.0f, 1.0f, 1.0f, 1.0f};
        constexpr float k_promptSize{28.0f};
        constexpr float k_objectiveSize{24.0f};
        constexpr float k_lineSize{26.0f};
        constexpr float k_captionSize{40.0f};
        constexpr float k_captionFadeStart{0.5f};
    } //namespace

    void setHudPrompt(Hud& hud, std::string_view prompt)
    {
        if (prompt == hud.prompt)
        {
            return;
        }

        hud.prompt = prompt;

        if (!prompt.empty())
        {
            std::clog << "Game: [" << playerName(hud.localPlayer) << "] prompt: " << prompt << '\n';
        }
    }

    void setHudObjective(Hud& hud, std::string_view objective)
    {
        if (objective == hud.objective)
        {
            return;
        }

        hud.objective = objective;
        std::clog << "Game: [" << playerName(hud.localPlayer) << "] objective: "
            << (objective.empty() ? "-" : objective) << '\n';
    }

    void addHudLine(Hud& hud, std::string_view text)
    {
        const auto shown = std::find_if(hud.lines.begin(), hud.lines.end(),
            [text](const HudLine& line) { return line.text == text; });

        if (shown != hud.lines.end())
        {
            shown->seconds = k_lineSeconds;
            return;
        }

        if (hud.lines.size() >= k_maxLines)
        {
            hud.lines.erase(hud.lines.begin());
        }

        hud.lines.push_back(HudLine{.text = std::string(text), .seconds = k_lineSeconds});
        std::clog << "Game: [" << playerName(hud.localPlayer) << "] " << text << '\n';
    }

    void stepHud(Hud& hud, float playerDelta)
    {
        for (HudLine& line : hud.lines)
        {
            line.seconds -= playerDelta;
        }

        std::erase_if(hud.lines, [](const HudLine& line) { return line.seconds <= 0.0f; });
    }

    void drawHud(render::Renderer& renderer, const Hud& hud)
    {
        renderer.setScreenFade(hud.fade);

        if (!hud.objective.empty())
        {
            renderer.submitText(hud.objective, k_objectivePosition,
                render::TextStyle{.size = k_objectiveSize, .color = k_objectiveColor});
        }

        if (!hud.prompt.empty())
        {
            renderer.submitText(hud.prompt, k_promptPosition,
                render::TextStyle{.size = k_promptSize, .color = k_promptColor, .align = render::TextAlign::Center});
        }

        for (std::size_t index = 0; index < hud.lines.size(); ++index)
        {
            const HudLine& line = hud.lines[index];
            math::Vec4 color = k_lineColor;
            color.w = std::clamp(line.seconds / k_lineFadeSeconds, 0.0f, 1.0f);
            const math::Vec2 position{k_linesPosition.x, k_linesPosition.y + k_lineSpacing * static_cast<float>(index)};
            renderer.submitText(line.text, position,
                render::TextStyle{.size = k_lineSize, .color = color, .align = render::TextAlign::Center});
        }

        if (!hud.caption.empty() && hud.fade > k_captionFadeStart)
        {
            math::Vec4 color = k_lineColor;
            color.w = (hud.fade - k_captionFadeStart) / (1.0f - k_captionFadeStart);
            renderer.submitText(hud.caption, k_captionPosition,
                render::TextStyle{.size = k_captionSize, .color = color, .align = render::TextAlign::Center});
        }
    }
} //namespace game
