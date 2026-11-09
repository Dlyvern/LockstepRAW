#include "Lockstep/Render/PresentMode.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace lockstep::render
{
    namespace
    {
        constexpr std::string_view k_presentFlag{"--present"};
        constexpr std::array k_modes{PresentMode::VSync, PresentMode::LowLatency, PresentMode::Uncapped};

        Error invalid(std::string message)
        {
            return Error{ErrorCode::InvalidArgument, std::move(message)};
        }
    } //namespace

    const char* toString(PresentMode mode)
    {
        switch (mode)
        {
            case PresentMode::VSync:
            {
                return "vsync";
            }
            case PresentMode::LowLatency:
            {
                return "lowlatency";
            }
            case PresentMode::Uncapped:
            {
                return "uncapped";
            }
        }

        return "vsync";
    }

    Result<PresentMode> parsePresentMode(std::span<const char* const> arguments)
    {
        PresentMode mode = k_defaultPresentMode;

        for (size_t index = 0; index < arguments.size(); ++index)
        {
            const std::string_view argument = arguments[index] != nullptr ? arguments[index] : "";

            if (argument != k_presentFlag)
            {
                continue;
            }

            if (index + 1 >= arguments.size() || arguments[index + 1] == nullptr)
            {
                return invalid("parsePresentMode: --present needs a value (vsync, lowlatency or uncapped)");
            }

            const std::string_view value = arguments[++index];
            bool known = false;

            for (const PresentMode candidate : k_modes)
            {
                if (value == toString(candidate))
                {
                    mode = candidate;
                    known = true;
                }
            }

            if (!known)
            {
                return invalid("parsePresentMode: '" + std::string(value)
                    + "' is not a present mode (vsync, lowlatency or uncapped)");
            }
        }

        return mode;
    }
} //namespace lockstep::render
