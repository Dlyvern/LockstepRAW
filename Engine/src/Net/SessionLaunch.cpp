#include "Lockstep/Net/SessionLaunch.hpp"

#include <charconv>
#include <limits>
#include <string_view>
#include <system_error>

namespace lockstep::net
{
    namespace
    {
        constexpr std::string_view k_hostFlag{"--host"};
        constexpr std::string_view k_connectFlag{"--connect"};
        constexpr std::string_view k_portFlag{"--port"};

        Error invalid(std::string message)
        {
            return Error{ErrorCode::InvalidArgument, std::move(message)};
        }

        Result<uint16_t> parsePort(std::string_view text)
        {
            unsigned value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

            if (error != std::errc{} || end != text.data() + text.size() || value == 0
                || value > std::numeric_limits<uint16_t>::max())
            {
                return invalid("parseSessionLaunch: '" + std::string(text) + "' is not a valid port");
            }

            return static_cast<uint16_t>(value);
        }
    } //namespace

    Result<SessionLaunch> parseSessionLaunch(std::span<const char* const> arguments)
    {
        SessionLaunch launch{};
        bool wantsHost = false;
        bool wantsConnect = false;
        bool hasPort = false;

        for (size_t index = 0; index < arguments.size(); ++index)
        {
            const std::string_view argument = arguments[index] != nullptr ? arguments[index] : "";
            const bool hasValue = index + 1 < arguments.size() && arguments[index + 1] != nullptr;

            if (argument == k_hostFlag)
            {
                wantsHost = true;
            }
            else if (argument == k_connectFlag || argument == k_portFlag)
            {
                if (!hasValue)
                {
                    return invalid("parseSessionLaunch: " + std::string(argument) + " needs a value");
                }

                const std::string_view value = arguments[++index];

                if (argument == k_connectFlag)
                {
                    if (value.empty())
                    {
                        return invalid("parseSessionLaunch: --connect needs a non-empty address");
                    }

                    wantsConnect = true;
                    launch.address = std::string(value);
                    continue;
                }

                auto port = parsePort(value);

                if (!port)
                {
                    return port.error();
                }

                hasPort = true;
                launch.port = port.value();
            }
        }

        if (wantsHost && wantsConnect)
        {
            return invalid("parseSessionLaunch: --host and --connect can't be used together");
        }

        if (wantsConnect && hasPort)
        {
            return invalid("parseSessionLaunch: --port is for --host; give the client's port in the address (ip:port)");
        }

        if (wantsHost)
        {
            launch.role = Role::Host;
        }
        else if (wantsConnect)
        {
            launch.role = Role::Client;
        }

        return launch;
    }
} //namespace lockstep::net
