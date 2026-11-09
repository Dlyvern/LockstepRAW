#ifndef LOCKSTEP_NET_SESSION_STATE_HPP
#define LOCKSTEP_NET_SESSION_STATE_HPP

#include "Lockstep/Core/Result.hpp"
#include "Lockstep/Net/Session.hpp"

#include <steam/steamnetworkingtypes.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace lockstep::net
{
    struct NetworkState;

    // Internal state of a Session: its GameNetworkingSockets handles and what it learned from
    // the remote player.
    // received:            Game messages of the last pump.
    // engineMessages:      whole Engine messages (kind byte included) of the last pump, other
    //                      than heartbeats; read by Replication.
    // pumpsSinceHeartbeat: pumps since the remote player's last heartbeat (or since connecting).
    // sendFailureReported: a failed send was logged and no send has succeeded since.
    struct SessionState
    {
        NetworkState* network{nullptr};
        Role role{Role::None};
        ConnectionState state{ConnectionState::Offline};
        HSteamListenSocket listenSocket{k_HSteamListenSocket_Invalid};
        HSteamNetConnection connection{k_HSteamNetConnection_Invalid};
        std::vector<std::vector<std::byte>> received;
        std::vector<std::vector<std::byte>> engineMessages;
        std::optional<uint64_t> remoteTick;
        uint64_t heartbeatsReceived{0};
        uint64_t pumpsSinceHeartbeat{0};
        bool sendFailureReported{false};
        std::string lastEvent;
    };

    // Returns: the internal state of `session`.
    // session: a Session that was not moved from (asserted).
    [[nodiscard]] SessionState& sessionState(Session& session);

    // Returns: the internal state of `session`.
    // session: a Session that was not moved from (asserted).
    [[nodiscard]] const SessionState& sessionState(const Session& session);

    // Queues a whole Engine message (kind byte included) for the remote player. It goes out at
    // the next pump(). A send failure (NetworkFailure) is logged once and recorded as the
    // session's last event, except "no connection" while the connection is closing.
    // session:  a connected session.
    // message:  the message bytes.
    // delivery: reliable or unreliable.
    // Returns:  success, or an error if not connected, the message is too large or the send
    //           failed.
    [[nodiscard]] Status sendEngineMessage(SessionState& session, std::span<const std::byte> message, Delivery delivery);

    // Logs a connection event and keeps it as the session's last event.
    // session: the session.
    // event:   one line describing what happened.
    void setEvent(SessionState& session, std::string event);

    // Forgets the current connection and what was learned from it (remote tick, heartbeats).
    // session: the session.
    void resetConnectionData(SessionState& session);

    // Returns: true if `info` is about a connection `session` owns or one arriving on its listen
    //          socket.
    // session: the session to check.
    // info:    a connection status callback.
    [[nodiscard]] bool ownsConnection(const SessionState& session, const SteamNetConnectionStatusChangedCallback_t& info);

    // Reacts to a connection status change: accepts or rejects incoming players, and tracks
    // connects and disconnects.
    // session: the session that owns the connection (see ownsConnection()).
    // info:    the status change.
    void handleStatusChange(SessionState& session, const SteamNetConnectionStatusChangedCallback_t& info);
} //namespace lockstep::net

#endif //LOCKSTEP_NET_SESSION_STATE_HPP
