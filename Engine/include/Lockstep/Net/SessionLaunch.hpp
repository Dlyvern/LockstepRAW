#ifndef LOCKSTEP_NET_SESSION_LAUNCH_HPP
#define LOCKSTEP_NET_SESSION_LAUNCH_HPP

#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Net/Session.hpp"

#include <cstdint>
#include <span>
#include <string>

namespace lockstep::net
{
    // What the session does as soon as the game starts.
    // role:    None stays offline, Host listens on `port`, Client connects to `address`.
    // address: host address for Client, as accepted by Session::connect().
    // port:    port to listen on for Host.
    struct SessionLaunch
    {
        Role role{Role::None};
        std::string address;
        uint16_t port{k_defaultPort};
    };

    // Reads the session launch options from the command line: `--host`, `--connect <address>`
    // and `--port <port>` (for --host). Other arguments are ignored.
    // arguments: the command-line arguments, without the program name.
    // Returns:   the launch options, or an error if --host and --connect are both given,
    //            --port is combined with --connect, the connect address is empty, or a value is
    //            missing or not a valid port.
    [[nodiscard]] Result<SessionLaunch> parseSessionLaunch(std::span<const char* const> arguments);
} //namespace lockstep::net

#endif //LOCKSTEP_NET_SESSION_LAUNCH_HPP
