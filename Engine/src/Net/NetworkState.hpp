#ifndef LOCKSTEP_NET_NETWORK_STATE_HPP
#define LOCKSTEP_NET_NETWORK_STATE_HPP

#include "Lockstep/Net/Network.hpp"

#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include <chrono>
#include <vector>

namespace lockstep::net
{
    struct SessionState;

    // A connection closed with linger: GameNetworkingSockets keeps delivering its queued reliable
    // data in the background, but no longer reports on it, so the linger is given a fixed time.
    // listenSocket: the listen socket the connection was accepted on, kept open until the
    //               deadline (closing it would destroy the lingering connection), or invalid.
    // deadline:     when the linger is assumed to be over.
    struct Linger
    {
        HSteamListenSocket listenSocket{k_HSteamListenSocket_Invalid};
        std::chrono::steady_clock::time_point deadline;
    };

    // The started GameNetworkingSockets library and the sessions using it. Connection status
    // callbacks from the library are routed to the session that owns the connection.
    struct NetworkState
    {
        NetworkState() = default;

        // Waits (bounded) for lingering connections to deliver their reliable data, then shuts
        // GameNetworkingSockets down.
        ~NetworkState();

        NetworkState(const NetworkState&) = delete;
        NetworkState& operator=(const NetworkState&) = delete;
        NetworkState(NetworkState&&) = delete;
        NetworkState& operator=(NetworkState&&) = delete;

        ISteamNetworkingSockets* sockets{nullptr};
        ISteamNetworkingUtils* utils{nullptr};
        std::vector<SessionState*> sessions;
        SimulatedConditions conditions{};
        std::vector<Linger> lingers;
    };

    // Returns: the internal state of `network`.
    // network: a Network that was not moved from (asserted).
    NetworkState& networkState(Network& network);

    // Closes a connection, letting it deliver its queued reliable data first. If it has any, the
    // connection lingers and `listenSocket` stays open until the linger time is over; otherwise
    // `listenSocket` is closed right away.
    // network:      the network that owns the connection.
    // connection:   the connection to close.
    // listenSocket: the listen socket to close after the connection, or invalid.
    // reason:       debug text sent to the remote player.
    void closeWithLinger(NetworkState& network, HSteamNetConnection connection, HSteamListenSocket listenSocket,
        const char* reason);

    // Closes the listen sockets kept open for lingering connections right away, ending those
    // lingers. Used before listening again, which may need the same port.
    // network: the network whose lingers to end.
    void endLingers(NetworkState& network);

    // Adds `session` to the sessions that receive connection status callbacks.
    // network: the network the session uses.
    // session: the session. Must be removed with removeSession() before it is destroyed.
    void addSession(NetworkState& network, SessionState& session);

    // Stops routing connection status callbacks to `session`.
    // network: the network the session uses.
    // session: a session added with addSession().
    void removeSession(NetworkState& network, SessionState& session);

    // Delivers queued connection status callbacks to their sessions and closes the listen sockets
    // of lingers that are over.
    // network: the network whose callbacks to run.
    void runCallbacks(NetworkState& network);
} //namespace lockstep::net

#endif //LOCKSTEP_NET_NETWORK_STATE_HPP
