#ifndef LOCKSTEP_DEBUG_NET_PANEL_HPP
#define LOCKSTEP_DEBUG_NET_PANEL_HPP

#include "Lockstep/Net/Network.hpp"
#include "Lockstep/Net/Replication.hpp"
#include "Lockstep/Net/Session.hpp"

#include <array>
#include <string>

namespace lockstep::debug
{
    // What the Net panel remembers between frames: the edited address and port, and the last
    // error from a button.
    struct NetPanelState
    {
        std::array<char, 64> address{"127.0.0.1"};
        int port{net::k_defaultPort};
        std::string lastError;
    };

    // Draws the "Net" ImGui window: host / connect / disconnect controls, connection state,
    // ping, packet loss, data rates, heartbeat ticks, sliders for simulated lag and loss,
    // replication counters, the replicated World scale and the interpolation delay slider.
    // Must be called inside an ImGui frame.
    // panel:       the panel's remembered state.
    // network:     the networking library, for the simulated conditions.
    // session:     the session to show and control.
    // replication: the session's replication, to show and tune.
    void drawNetPanel(NetPanelState& panel, net::Network& network, net::Session& session, net::Replication& replication);
} //namespace lockstep::debug

#endif //LOCKSTEP_DEBUG_NET_PANEL_HPP
