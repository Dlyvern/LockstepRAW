#include "Game.hpp"
#include "Shell/ShellLaunch.hpp"

#include <Lockstep/App/Run.hpp>
#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Core/Result.hpp>
#include <Lockstep/Net/SessionLaunch.hpp>
#include <Lockstep/Render/GraphicsConfig.hpp>
#include <Lockstep/Render/PresentMode.hpp>

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    constexpr std::string_view k_dataFlag{"--data"};

    int reportError(const lockstep::Error& error)
    {
        std::cerr << "Error (" << lockstep::toString(error.code) << "): " << error.message << '\n';
        return 1;
    }

    std::filesystem::path dataFolder(std::span<const char* const> options)
    {
        for (std::size_t index = 0; index + 1 < options.size(); ++index)
        {
            if (options[index] != nullptr && options[index + 1] != nullptr && std::string_view(options[index]) == k_dataFlag)
            {
                return options[index + 1];
            }
        }

        return LOCKSTEP_DATA_DIR;
    }

    void mountGameData(std::vector<std::filesystem::path>& content, std::span<const char* const> options)
    {
        const std::filesystem::path folder = dataFolder(options);
        std::error_code error;

        if (!std::filesystem::is_directory(folder, error))
        {
            std::clog << "Game: no data folder '" << folder.string() << "' (dialogue shows line ids)\n";
            return;
        }

        content.push_back(folder);
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

    if (game::isMenuLaunch(options) && !game::hasContentOption(options))
    {
        for (const std::filesystem::path& pak : game::defaultContentPaks(LOCKSTEP_CONTENT_DIR))
        {
            content.value().push_back(pak);
        }
    }

    mountGameData(content.value(), options);

    auto graphics = lockstep::render::loadGraphicsConfig(options);

    if (!graphics)
    {
        return reportError(graphics.error());
    }

    const lockstep::render::GraphicsConfig& config = graphics.value();
    std::clog << "Game: graphics " << lockstep::render::toString(config.quality.preset) << " (from " << config.origin
        << "), window " << config.windowWidth << 'x' << config.windowHeight << ", settings file '"
        << config.file.string() << "'\n";

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
            .window = {.title = title, .width = static_cast<int>(config.windowWidth),
                .height = static_cast<int>(config.windowHeight)},
            .renderer = {.presentMode = presentMode.value(), .quality = config.quality, .settingsFile = config.file},
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
