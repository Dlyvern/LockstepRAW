#include "CookerArguments.hpp"

#include <cstddef>
#include <iostream>
#include <span>
#include <variant>

namespace
{
    int reportError(const lockstep::Error& error)
    {
        std::cerr << "LockstepCooker: error (" << lockstep::toString(error.code) << "): " << error.message << '\n';
        return 1;
    }

    lockstep::Status runCommand(const lockstep::cooker::CookerCommand& command)
    {
        if (const auto* cook = std::get_if<lockstep::cooker::CookOptions>(&command))
        {
            return lockstep::cooker::cookScene(*cook);
        }

        return lockstep::cooker::writePak(std::get<lockstep::cooker::PackOptions>(command));
    }
} //namespace

int main(int argc, char** argv)
{
    const std::span<const char* const> arguments(argv, static_cast<std::size_t>(argc));
    auto command = lockstep::cooker::parseCookerArguments(arguments.empty() ? arguments : arguments.subspan(1));

    if (!command)
    {
        std::cerr << lockstep::cooker::k_usage;
        return reportError(command.error());
    }

    if (const lockstep::Status status = runCommand(command.value()); !status)
    {
        return reportError(status.error());
    }

    return 0;
}
