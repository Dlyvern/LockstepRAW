#ifndef LOCKSTEP_GAME_PRESENTATION_DEV_CAPTURE_HPP
#define LOCKSTEP_GAME_PRESENTATION_DEV_CAPTURE_HPP

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace lockstep::render
{
    class Renderer;
} //namespace lockstep::render

namespace game
{
    // One frame to save.
    // frame: the presented frame number (0 is the first frame).
    // path:  the PNG to write.
    struct DevCaptureShot
    {
        uint64_t frame{0};
        std::string path;
    };

    // Dev frame captures from the command line (dev builds only), for checking how things look
    // without a screenshot tool: `--dev-capture <frame> <file.png>` (repeatable), or
    // `--dev-capture <first>:<step>:<count> <prefix>` for <prefix>_<frame>.png every <step> frames.
    // Captures less than a few frames apart are delayed until the previous one is written.
    // shots: the frames still to save, in order.
    // frame: frames presented so far.
    struct DevCapture
    {
        std::vector<DevCaptureShot> shots;
        uint64_t frame{0};
    };

    // Reads the --dev-capture flags (none outside dev builds); malformed ones are logged and skipped.
    // arguments: the command line.
    // Returns:   the captures asked for.
    [[nodiscard]] DevCapture parseDevCapture(std::span<const char* const> arguments);

    // Asks the renderer to save this frame when one is due. Call once per presented frame.
    // capture:  the captures, updated.
    // renderer: the renderer.
    void updateDevCapture(DevCapture& capture, lockstep::render::Renderer& renderer);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_DEV_CAPTURE_HPP
