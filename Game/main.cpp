#include "Game.hpp"

#include <Lockstep/App/Run.hpp>
#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Net/SessionLaunch.hpp>
#include <Lockstep/Render/PresentMode.hpp>

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
    const std::span<const char* const> options = arguments.empty() ? arguments : arguments.subspan(1);
    auto session = lockstep::net::parseSessionLaunch(options);

    if (!session)
    {
        return reportError(session.error());
    }

    auto presentMode = lockstep::render::parsePresentMode(options);

    if (!presentMode)
    {
        return reportError(presentMode.error());
    }

    auto content = lockstep::assets::parseContentMounts(options);

    if (!content)
    {
        return reportError(content.error());
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
            .renderer = {.presentMode = presentMode.value()},
            .session = std::move(session).value(),
            .arguments = options,
            .content = std::move(content).value()},
        game::gameApi());

    if (!status)
    {
        return reportError(status.error());
    }

    return 0;
}
