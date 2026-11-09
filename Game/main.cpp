#include "Game.hpp"

#include <Lockstep/App/Run.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Net/SessionLaunch.hpp>

#include <cstddef>
#include <iostream>
#include <span>
#include <utility>

namespace
{
    int reportError(const lockstep::Error& error)
    {
        std::cerr << "Error (" << lockstep::toString(error.code) << "): " << error.message << '\n';
        return 1;
    }
} //namespace

int main(int argc, char** argv)
{
    const std::span<const char* const> arguments(argv, static_cast<size_t>(argc));
    auto session = lockstep::net::parseSessionLaunch(arguments.empty() ? arguments : arguments.subspan(1));

    if (!session)
    {
        return reportError(session.error());
    }

    const char* title = "Lockstep";

    if (session.value().role == lockstep::net::Role::Host)
    {
        title = "Lockstep (host)";
    }
    else if (session.value().role == lockstep::net::Role::Client)
    {
        title = "Lockstep (client)";
    }

    const lockstep::Status status = lockstep::app::run({
            .window = {.title = title, .width = 1280, .height = 720},
            .session = std::move(session).value()},
        game::gameApi());

    if (!status)
    {
        return reportError(status.error());
    }

    return 0;
}
