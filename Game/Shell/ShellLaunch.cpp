#include "Shell/ShellLaunch.hpp"

#include <algorithm>
#include <array>
#include <system_error>

namespace game
{
    namespace
    {
        constexpr std::string_view k_menuFlag{"--menu"};
        constexpr std::string_view k_contentFlag{"--content"};
        constexpr std::string_view k_devPrefix{"--dev-"};
        constexpr std::array<std::string_view, 6> k_gameFlags{"--level", "--scene", "--host", "--connect", "--echo-play",
            "--armed"};
        constexpr std::array<std::string_view, 7> k_menuDevFlags{"--dev-capture", "--dev-capture-on", "--dev-gpu-timings",
            "--dev-menu", "--dev-auto-ready", "--dev-name", "--dev-leave"};
        constexpr std::array<std::string_view, 5> k_defaultPaks{"characters.pak", "district.pak", "station.pak",
            "hospital_a.pak", "hospital_b.pak"};

        bool isGameFlag(std::string_view argument)
        {
            if (std::find(k_gameFlags.begin(), k_gameFlags.end(), argument) != k_gameFlags.end())
            {
                return true;
            }

            return argument.starts_with(k_devPrefix)
                && std::find(k_menuDevFlags.begin(), k_menuDevFlags.end(), argument) == k_menuDevFlags.end();
        }
    } //namespace

    bool isMenuLaunch(std::span<const char* const> arguments)
    {
        bool isGame = false;

        for (const char* argument : arguments)
        {
            if (argument == nullptr)
            {
                continue;
            }

            if (std::string_view(argument) == k_menuFlag)
            {
                return true;
            }

            isGame = isGame || isGameFlag(argument);
        }

        return !isGame;
    }

    bool hasContentOption(std::span<const char* const> arguments)
    {
        return std::any_of(arguments.begin(), arguments.end(), [](const char* argument)
        {
            return argument != nullptr && std::string_view(argument) == k_contentFlag;
        });
    }

    std::vector<std::filesystem::path> defaultContentPaks(const std::filesystem::path& folder)
    {
        std::vector<std::filesystem::path> paks;

        for (const std::string_view name : k_defaultPaks)
        {
            const std::filesystem::path pak = folder / name;
            std::error_code error;

            if (std::filesystem::is_regular_file(pak, error))
            {
                paks.push_back(pak);
            }
        }

        return paks;
    }
} //namespace game
