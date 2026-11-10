#include "Presentation/DevCapture.hpp"

#include <Lockstep/Render/Renderer.hpp>

#include <algorithm>
#include <charconv>
#include <iostream>
#include <string>
#include <string_view>

namespace game
{
#if LOCKSTEP_DEV_TOOLS
    namespace
    {
        constexpr std::string_view k_captureFlag{"--dev-capture"};
        constexpr std::string_view k_triggerFlag{"--dev-capture-on"};

        bool parseNumber(std::string_view text, uint64_t& value)
        {
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            return error == std::errc{} && end == text.data() + text.size();
        }

        bool addTrigger(DevCapture& capture, std::string_view event, std::string_view frames, std::string_view prefix)
        {
            const std::size_t colon = frames.find(':');
            uint64_t step = 0;
            uint64_t count = 0;

            if ((event != "reload" && event != "switch") || colon == std::string_view::npos
                || !parseNumber(frames.substr(0, colon), step) || !parseNumber(frames.substr(colon + 1), count) || step == 0)
            {
                return false;
            }

            capture.triggers.push_back(DevCaptureTrigger{.event = std::string(event), .step = step, .count = count,
                .prefix = std::string(prefix)});
            return true;
        }

        bool addShots(DevCapture& capture, std::string_view frames, std::string_view path)
        {
            const std::size_t first = frames.find(':');

            if (first == std::string_view::npos)
            {
                uint64_t frame = 0;

                if (!parseNumber(frames, frame))
                {
                    return false;
                }

                capture.shots.push_back(DevCaptureShot{.frame = frame, .path = std::string(path)});
                return true;
            }

            const std::size_t second = frames.find(':', first + 1);
            uint64_t start = 0;
            uint64_t step = 0;
            uint64_t count = 0;

            if (second == std::string_view::npos || !parseNumber(frames.substr(0, first), start)
                || !parseNumber(frames.substr(first + 1, second - first - 1), step)
                || !parseNumber(frames.substr(second + 1), count) || step == 0)
            {
                return false;
            }

            for (uint64_t index = 0; index < count; ++index)
            {
                const uint64_t frame = start + index * step;
                capture.shots.push_back(DevCaptureShot{.frame = frame,
                    .path = std::string(path) + "_" + std::to_string(frame) + ".png"});
            }

            return true;
        }
    } //namespace
#endif

    namespace
    {
        void startTriggers(DevCapture& capture, const DevCaptureEvents& events)
        {
            for (DevCaptureTrigger& trigger : capture.triggers)
            {
                const bool isOn = trigger.event == "reload" ? events.isReloading : events.isSwitching;

                if (isOn && !trigger.isOn)
                {
                    ++trigger.events;

                    for (uint64_t index = 0; index < trigger.count; ++index)
                    {
                        const uint64_t frame = capture.frame + index * trigger.step;
                        capture.shots.push_back(DevCaptureShot{.frame = frame,
                            .path = trigger.prefix + "_" + std::to_string(trigger.events) + "_" + std::to_string(frame)
                                + ".png"});
                    }

                    std::sort(capture.shots.begin(), capture.shots.end(),
                        [](const DevCaptureShot& a, const DevCaptureShot& b) { return a.frame < b.frame; });
                }

                trigger.isOn = isOn;
            }
        }
    } //namespace

    DevCapture parseDevCapture(std::span<const char* const> arguments)
    {
        DevCapture capture{};
#if LOCKSTEP_DEV_TOOLS
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            if (std::string_view(arguments[index]) == k_triggerFlag)
            {
                if (index + 3 >= arguments.size() || !addTrigger(capture, arguments[index + 1], arguments[index + 2],
                        arguments[index + 3]))
                {
                    std::clog << "Game: dev: --dev-capture-on needs <reload|switch> <step>:<count> <prefix>\n";
                }

                continue;
            }

            if (std::string_view(arguments[index]) != k_captureFlag)
            {
                continue;
            }

            if (index + 2 >= arguments.size() || !addShots(capture, arguments[index + 1], arguments[index + 2]))
            {
                std::clog << "Game: dev: --dev-capture needs <frame> <file.png> or <first>:<step>:<count> <prefix>\n";
            }
        }

        std::sort(capture.shots.begin(), capture.shots.end(), [](const DevCaptureShot& a, const DevCaptureShot& b)
        {
            return a.frame < b.frame;
        });
#else
        static_cast<void>(arguments);
#endif
        return capture;
    }

    void updateDevCapture(DevCapture& capture, lockstep::render::Renderer& renderer, const DevCaptureEvents& events)
    {
        startTriggers(capture, events);
        const uint64_t frame = capture.frame++;

        if (capture.shots.empty() || capture.shots.front().frame > frame)
        {
            return;
        }

        if (auto requested = renderer.captureNextFrame(capture.shots.front().path); !requested)
        {
            return;
        }

        std::clog << "Game: dev: capturing frame " << frame << " to '" << capture.shots.front().path << "'\n";
        capture.shots.erase(capture.shots.begin());
    }
} //namespace game
