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

    // A run of captures that starts when a local event begins (`--dev-capture-on`).
    // event:  "reload" or "switch".
    // step, count: every `step` frames, `count` captures.
    // prefix: files are <prefix>_<n>_<frame>.png (n counts the events).
    // isOn:   the event ran last frame.
    // events: events seen so far.
    struct DevCaptureTrigger
    {
        std::string event;
        uint64_t step{1};
        uint64_t count{0};
        std::string prefix;
        bool isOn{false};
        uint32_t events{0};
    };

    // Dev frame captures from the command line (dev builds only), for checking how things look
    // without a screenshot tool: `--dev-capture <frame> <file.png>` (repeatable), or
    // `--dev-capture <first>:<step>:<count> <prefix>` for <prefix>_<frame>.png every <step> frames,
    // or `--dev-capture-on <reload|switch> <step>:<count> <prefix>` to capture from the start of
    // each local reload or weapon switch. Captures less than a few frames apart are delayed until
    // the previous one is written.
    // shots:    the frames still to save, in order.
    // triggers: the event-started runs.
    // frame:    frames presented so far.
    struct DevCapture
    {
        std::vector<DevCaptureShot> shots;
        std::vector<DevCaptureTrigger> triggers;
        uint64_t frame{0};
    };

    // Local events --dev-capture-on starts captures from.
    // isReloading: a local reload runs.
    // isSwitching: a local weapon switch runs.
    struct DevCaptureEvents
    {
        bool isReloading{false};
        bool isSwitching{false};
    };

    // Reads the --dev-capture flags (none outside dev builds); malformed ones are logged and skipped.
    // arguments: the command line.
    // Returns:   the captures asked for.
    [[nodiscard]] DevCapture parseDevCapture(std::span<const char* const> arguments);

    // Asks the renderer to save this frame when one is due. Call once per presented frame.
    // capture:  the captures, updated.
    // renderer: the renderer.
    // events:   this frame's local events (for --dev-capture-on).
    void updateDevCapture(DevCapture& capture, lockstep::render::Renderer& renderer, const DevCaptureEvents& events);
} //namespace game

#endif //LOCKSTEP_GAME_PRESENTATION_DEV_CAPTURE_HPP
