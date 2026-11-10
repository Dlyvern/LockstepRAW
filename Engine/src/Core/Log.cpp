#include "Lockstep/Core/Log.hpp"

#include <iostream>
#include <mutex>

namespace lockstep
{
    namespace
    {
        std::mutex& logMutex()
        {
            static std::mutex mutex;
            return mutex;
        }
    } //namespace

    void log(LogLevel level, std::string_view channel, std::string_view message)
    {
        const std::scoped_lock lock(logMutex());
        std::ostream& stream = level == LogLevel::Info ? std::cout : std::cerr;
        stream << '[' << channel << "] " << message << '\n';
        stream.flush();
    }
} //namespace lockstep
