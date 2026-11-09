#ifndef LOCKSTEP_GAME_PRESENTATION_HUD_HPP
#define LOCKSTEP_GAME_PRESENTATION_HUD_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // A line of narration on screen ("Your flatmate silenced the alarm...").
    // text:    what it says.
    // seconds: Player seconds left on screen.
    struct HudLine
    {
        std::string text;
        float seconds{0.0f};
    };

    // The local player's on-screen text and the screen fade: what Game shows on top of the view.
    // Client-only presentation, never replicated. Every change is also logged.
    // localPlayer:  this machine's player index, for the log.
    // prompt:       what pressing E (or LMB) would do now, empty for none.
    // objective:    the small objective line, empty for none.
    // lines:        narration, oldest first.
    // fade:         how black the screen is, 0 to 1.
    // caption:      large centred text shown over the fade, empty for none.
    // subtitle:     the dialogue subtitle at the bottom of the screen, empty for none.
    struct Hud
    {
        std::size_t localPlayer{0};
        std::string prompt;
        std::string objective;
        std::vector<HudLine> lines;
        float fade{0.0f};
        std::string caption;
        std::string subtitle;
    };

    // Sets the prompt; logs it when it changes to a new non-empty one.
    // hud:    the HUD.
    // prompt: the prompt, empty for none.
    void setHudPrompt(Hud& hud, std::string_view prompt);

    // Sets the objective line; logs it when it changes.
    // hud:       the HUD.
    // objective: the line, empty for none.
    void setHudObjective(Hud& hud, std::string_view objective);

    // Sets the subtitle; logs it when it changes to a new non-empty one.
    // hud:      the HUD.
    // subtitle: the subtitle, empty for none.
    void setHudSubtitle(Hud& hud, std::string_view subtitle);

    // Shows a line of narration for a few seconds and logs it. Old lines make room. A line already
    // on screen is not added again; its timer starts over instead.
    // hud:  the HUD.
    // text: the line.
    void addHudLine(Hud& hud, std::string_view text);

    // Ages the narration lines by one tick.
    // hud:         the HUD.
    // playerDelta: Player domain delta of this tick.
    void stepHud(Hud& hud, float playerDelta);

    // Queues the HUD text for the next frame and fades the image to black by the HUD's fade
    // (Renderer::setScreenFade; the text stays visible over it).
    // renderer: the renderer.
    // hud:      the HUD.
    void drawHud(lockstep::render::Renderer& renderer, const Hud& hud);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_HUD_HPP
