#ifndef LOCKSTEP_CORE_LOG_HPP
#define LOCKSTEP_CORE_LOG_HPP

#include <cstdint>
#include <string_view>

namespace lockstep
{
    // Severity of a log message.
    // Info:    progress and one-off facts (device selection, loaded assets).
    // Warning: something went wrong but was handled (fallbacks, validation warnings).
    // Error:   something failed (validation errors, a failed upload).
    enum class LogLevel : uint8_t
    {
        Info = 0,
        Warning,
        Error
    };

    // Writes one line "[channel] message": Info to stdout, Warning and Error to stderr. Safe to
    // call from any thread; lines from different threads never interleave.
    // level:   severity of the message.
    // channel: the module or library the message comes from ("Render", "Vela", ...).
    // message: the text, without a trailing newline.
    void log(LogLevel level, std::string_view channel, std::string_view message);
} //namespace lockstep

#endif //LOCKSTEP_CORE_LOG_HPP
