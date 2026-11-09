#ifndef LOCKSTEP_RENDER_PRESENT_MODE_HPP
#define LOCKSTEP_RENDER_PRESENT_MODE_HPP

#include "Lockstep/Core/Result.hpp"

#include <cstdint>
#include <span>

namespace lockstep::render
{
    // How finished frames reach the screen. The Renderer falls back Uncapped -> LowLatency ->
    // VSync when the window surface does not support the requested mode; VSync is always
    // available. The mode is fixed when the Renderer is created.
    // VSync:      waits for the display refresh (FIFO). Lowest power, adds up to one frame of
    //             latency and can block the CPU for a whole refresh when it falls behind.
    // LowLatency: renders unthrottled and shows the newest finished frame at each refresh
    //             (mailbox). No tearing, no blocking on present.
    // Uncapped:   presents immediately (immediate). May tear; for measuring raw frame rate.
    enum class PresentMode : uint8_t
    {
        VSync = 0,
        LowLatency,
        Uncapped
    };

    // The present mode used when nothing else is asked for: LowLatency in builds with dev tools,
    // VSync in shipping builds.
#if LOCKSTEP_DEV_TOOLS
    inline constexpr PresentMode k_defaultPresentMode{PresentMode::LowLatency};
#else
    inline constexpr PresentMode k_defaultPresentMode{PresentMode::VSync};
#endif

    // Returns: the command-line spelling of a present mode ("vsync", "lowlatency" or "uncapped").
    // mode:    the present mode.
    [[nodiscard]] const char* toString(PresentMode mode);

    // Reads `--present <vsync|lowlatency|uncapped>` from the command line. Other arguments are
    // ignored.
    // arguments: the command-line arguments, without the program name.
    // Returns:   the requested mode, k_defaultPresentMode if the option is absent, or an error if
    //            the value is missing or not one of the three names.
    [[nodiscard]] Result<PresentMode> parsePresentMode(std::span<const char* const> arguments);
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_PRESENT_MODE_HPP
