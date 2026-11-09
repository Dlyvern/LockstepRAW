#include "VelaLog.hpp"

#include "Lockstep/Core/Log.hpp"

#include <Vela/Log.hpp>

namespace lockstep
{
    namespace
    {
        LogLevel toLogLevel(vela::LogLevel level)
        {
            switch (level)
            {
                case vela::LogLevel::Warning:
                {
                    return LogLevel::Warning;
                }
                case vela::LogLevel::Error:
                case vela::LogLevel::Off:
                {
                    return LogLevel::Error;
                }
                case vela::LogLevel::Debug:
                case vela::LogLevel::Info:
                {
                    break;
                }
            }

            return LogLevel::Info;
        }
    } //namespace

    void routeVelaLogs()
    {
        vela::setLogLevel(vela::LogLevel::Info);
        vela::setLogCallback([](vela::LogLevel level, std::string_view message)
        {
            log(toLogLevel(level), "Vela", message);
        });
    }
} //namespace lockstep
