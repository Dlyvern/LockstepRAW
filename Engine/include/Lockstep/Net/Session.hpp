#ifndef LOCKSTEP_NET_SESSION_HPP
#define LOCKSTEP_NET_SESSION_HPP

#include "Lockstep/Core/Result.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::net
{
    class Network;
    struct SessionState;

    // UDP port a host listens on when none is given.
    inline constexpr uint16_t k_defaultPort{27020};

    // Which side of the two-player game this session is.
    enum class Role : uint8_t
    {
        None,
        Host,
        Client
    };

    // Where the session's connection to the remote player stands.
    // Offline:    neither hosting nor connecting.
    // Listening:  hosting, waiting for the remote player.
    // Connecting: a connection to or from the remote player is being set up.
    // Connected:  messages flow both ways.
    enum class ConnectionState : uint8_t
    {
        Offline,
        Listening,
        Connecting,
        Connected
    };

    // How a message is delivered.
    // Reliable:   arrives exactly once, in order with other reliable messages.
    // Unreliable: may be lost; never resent.
    enum class Delivery : uint8_t
    {
        Reliable,
        Unreliable
    };

    // Live quality of the connection to the remote player.
    // pingMilliseconds:      current round-trip time.
    // packetLoss:            share of the remote's packets that did not arrive, in [0, 1].
    // sendBytesPerSecond:    current outgoing data rate.
    // receiveBytesPerSecond: current incoming data rate.
    // pendingReliableBytes:  reliable data queued but not yet acknowledged.
    struct ConnectionStats
    {
        float pingMilliseconds{0.0f};
        float packetLoss{0.0f};
        float sendBytesPerSecond{0.0f};
        float receiveBytesPerSecond{0.0f};
        uint32_t pendingReliableBytes{0};
    };

    // One player's side of a two-player game: either hosts (listens on a port and accepts
    // exactly one remote player) or connects to a host. Messages are opaque bytes; Game decides
    // what they mean.
    //
    // While a player is connected, a host rejects other players, except that a new player
    // replaces the current one when the current one has sent no heartbeat for a second or the
    // new one comes from the same IP address (a restarted client). A silent connection times
    // out after about 4 seconds.
    //
    // pump() runs once per fixed tick, after the simulation step. It sends a heartbeat carrying
    // the tick number, flushes everything sent during the tick, processes connection changes
    // and collects the messages that arrived; those are readable through received() until the
    // next pump().
    class Session
    {
    public:
        // Creates an offline session.
        // network: the networking library. Must outlive the session and must not be a
        //          moved-from Network (asserted).
        explicit Session(Network& network);

        // Disconnects and stops hosting.
        ~Session();

        // Takes over another session and its connection. `other` becomes unusable.
        Session(Session&& other) noexcept;

        // Disconnects this session, then takes over `other`. `other` becomes unusable.
        Session& operator=(Session&& other) noexcept;

        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        // Starts hosting: listens for the remote player on all local addresses.
        // port:    UDP port to listen on.
        // Returns: success, or an error if the session is not offline or the port can't be
        //          opened.
        [[nodiscard]] Status host(uint16_t port);

        // Starts connecting to a host. Watch state() to see when it is connected.
        // address: "ip" or "ip:port", IPv4 or IPv6 ("[::1]:27020"). Without a port,
        //          k_defaultPort is used.
        // Returns: success, or an error if the session is not offline or the address is invalid.
        [[nodiscard]] Status connect(std::string_view address);

        // Closes the connection and stops hosting. Queued reliable messages are still delivered
        // for up to a second in the background (the host's listen socket stays open that long);
        // destroying the Network waits for that. Hosting again ends the delivery early. The
        // session becomes offline. Does nothing when already offline.
        void disconnect();

        // Advances networking by one tick. Call exactly once per fixed tick, after the
        // simulation step, even while offline.
        // tick: number of the tick that just ran. Sent to the remote player as the heartbeat.
        void pump(uint64_t tick);

        // Queues a message for the remote player. It goes out at the next pump().
        // payload:  the message bytes. Copied; may be empty.
        // delivery: reliable or unreliable.
        // Returns:  success, or an error if not connected or the message is too large.
        [[nodiscard]] Status send(std::span<const std::byte> payload, Delivery delivery);

        // Returns: the messages from the remote player collected by the last pump(), in arrival
        //          order. Valid until the next pump().
        [[nodiscard]] std::span<const std::vector<std::byte>> received() const;

        // Returns: whether this session hosts, connects, or neither (offline).
        [[nodiscard]] Role role() const;

        // Returns: the state of the connection to the remote player.
        [[nodiscard]] ConnectionState state() const;

        // Returns: live connection quality, or nothing when not connected.
        [[nodiscard]] std::optional<ConnectionStats> stats() const;

        // Returns: the tick number of the newest heartbeat from the remote player on the current
        //          connection, or nothing if none arrived yet.
        [[nodiscard]] std::optional<uint64_t> remoteTick() const;

        // Returns: how many heartbeats arrived on the current connection.
        [[nodiscard]] uint64_t heartbeatsReceived() const;

        // Returns: a one-line description of the last connection event, such as why the remote
        //          player disconnected. Empty if nothing happened yet.
        [[nodiscard]] const std::string& lastEvent() const;

    private:
        friend SessionState& sessionState(Session& session);
        friend const SessionState& sessionState(const Session& session);

        std::unique_ptr<SessionState> m_state;
    };

    // Returns: a readable name for `role`, for logs and the dev tools.
    [[nodiscard]] const char* toString(Role role) noexcept;

    // Returns: a readable name for `state`, for logs and the dev tools.
    [[nodiscard]] const char* toString(ConnectionState state) noexcept;
} //namespace lockstep::net

#endif //LOCKSTEP_NET_SESSION_HPP
