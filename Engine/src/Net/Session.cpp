#include "Lockstep/Net/Session.hpp"

#include "Lockstep/Net/Network.hpp"

#include "NetworkState.hpp"
#include "SessionState.hpp"
#include "WireFormat.hpp"

#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace lockstep::net
{
    namespace
    {
        constexpr size_t k_maxGamePayloadBytes{k_cbMaxSteamNetworkingSocketsMessageSizeSend - k_kindBytes};
        constexpr int k_receiveBatch{32};
        constexpr uint64_t k_heartbeatLogInterval{120};

        Error networkError(std::string message)
        {
            return Error{ErrorCode::NetworkFailure, std::move(message)};
        }

        void recordSendResult(SessionState& session, EResult result, const char* what)
        {
            if (result == k_EResultOK)
            {
                session.sendFailureReported = false;
                return;
            }

            const bool isClosing = result == k_EResultNoConnection;

            if (isClosing)
            {
                return;
            }

            if (!session.sendFailureReported)
            {
                session.sendFailureReported = true;
                setEvent(session, std::string(what) + " failed with EResult " + std::to_string(static_cast<int>(result)));
            }
        }

        void receiveHeartbeat(SessionState& session, std::span<const std::byte> message)
        {
            const std::optional<uint64_t> decoded = decodeHeartbeat(message);

            if (!decoded)
            {
                return;
            }

            const uint64_t tick = *decoded;

            if (!session.remoteTick || tick > *session.remoteTick)
            {
                session.remoteTick = tick;
            }

            ++session.heartbeatsReceived;
            session.pumpsSinceHeartbeat = 0;

            if (session.heartbeatsReceived == 1 || session.heartbeatsReceived % k_heartbeatLogInterval == 0)
            {
                std::clog << "Net (" << toString(session.role) << "): heartbeat #" << session.heartbeatsReceived
                          << ", remote tick " << tick << '\n';
            }
        }

        void receiveMessages(SessionState& session)
        {
            std::array<SteamNetworkingMessage_t*, k_receiveBatch> messages{};

            while (session.connection != k_HSteamNetConnection_Invalid)
            {
                const int count = session.network->sockets->ReceiveMessagesOnConnection(session.connection,
                    messages.data(), k_receiveBatch);

                for (int index = 0; index < count; ++index)
                {
                    SteamNetworkingMessage_t* message = messages[static_cast<size_t>(index)];
                    const std::span<const std::byte> bytes(static_cast<const std::byte*>(message->GetData()),
                        static_cast<size_t>(message->GetSize()));

                    const std::optional<MessageKind> kind = messageKind(bytes);

                    if (kind == MessageKind::Heartbeat)
                    {
                        receiveHeartbeat(session, bytes);
                    }
                    else if (kind == MessageKind::Game)
                    {
                        const std::span<const std::byte> payload = gamePayload(bytes);
                        session.received.emplace_back(payload.begin(), payload.end());
                    }
                    else if (kind.has_value())
                    {
                        session.engineMessages.emplace_back(bytes.begin(), bytes.end());
                    }

                    message->Release();
                }

                if (count < k_receiveBatch)
                {
                    return;
                }
            }
        }

        void sendHeartbeat(SessionState& session, uint64_t tick)
        {
            const std::array<std::byte, k_heartbeatBytes> message = encodeHeartbeat(tick);

            const EResult result = session.network->sockets->SendMessageToConnection(session.connection, message.data(),
                static_cast<uint32>(message.size()), k_nSteamNetworkingSend_Unreliable, nullptr);
            recordSendResult(session, result, "sending the heartbeat");
        }

        Status sendMessage(SessionState& session, std::span<const std::byte> message, Delivery delivery)
        {
            const int flags = delivery == Delivery::Reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable;
            const EResult result = session.network->sockets->SendMessageToConnection(session.connection, message.data(),
                static_cast<uint32>(message.size()), flags, nullptr);
            recordSendResult(session, result, "sending a message");

            if (result != k_EResultOK)
            {
                return networkError("Session::send: send failed with EResult " + std::to_string(static_cast<int>(result)));
            }

            return {};
        }
    } //namespace

    SessionState& sessionState(Session& session)
    {
        assert(session.m_state != nullptr && "Session was moved from");
        return *session.m_state;
    }

    const SessionState& sessionState(const Session& session)
    {
        assert(session.m_state != nullptr && "Session was moved from");
        return *session.m_state;
    }

    Status sendEngineMessage(SessionState& session, std::span<const std::byte> message, Delivery delivery)
    {
        if (session.state != ConnectionState::Connected)
        {
            return Error{ErrorCode::InvalidArgument, "sendEngineMessage: not connected"};
        }

        if (message.size() > k_cbMaxSteamNetworkingSocketsMessageSizeSend)
        {
            return Error{ErrorCode::InvalidArgument, "sendEngineMessage: message too large"};
        }

        return sendMessage(session, message, delivery);
    }

    Session::Session(Network& network)
        : m_state(std::make_unique<SessionState>())
    {
        m_state->network = &networkState(network);
        addSession(*m_state->network, *m_state);
    }

    Session::~Session()
    {
        if (m_state)
        {
            disconnect();
            removeSession(*m_state->network, *m_state);
        }
    }

    Session::Session(Session&& other) noexcept = default;

    Session& Session::operator=(Session&& other) noexcept
    {
        if (this != &other)
        {
            if (m_state)
            {
                disconnect();
                removeSession(*m_state->network, *m_state);
            }

            m_state = std::move(other.m_state);
        }

        return *this;
    }

    Status Session::host(uint16_t port)
    {
        if (!m_state || m_state->state != ConnectionState::Offline)
        {
            return Error{ErrorCode::InvalidArgument, "Session::host: the session is not offline"};
        }

        endLingers(*m_state->network);

        SteamNetworkingIPAddr address{};
        address.Clear();
        address.m_port = port;

        const HSteamListenSocket socket = m_state->network->sockets->CreateListenSocketIP(address, 0, nullptr);

        if (socket == k_HSteamListenSocket_Invalid)
        {
            return networkError("Session::host: could not listen on port " + std::to_string(port));
        }

        m_state->listenSocket = socket;
        m_state->role = Role::Host;
        m_state->state = ConnectionState::Listening;
        setEvent(*m_state, "hosting on port " + std::to_string(port));

        return {};
    }

    Status Session::connect(std::string_view address)
    {
        if (!m_state || m_state->state != ConnectionState::Offline)
        {
            return Error{ErrorCode::InvalidArgument, "Session::connect: the session is not offline"};
        }

        const std::string text(address);
        SteamNetworkingIPAddr remote{};

        if (!remote.ParseString(text.c_str()))
        {
            return Error{ErrorCode::InvalidArgument, "Session::connect: invalid address '" + text + "'"};
        }

        if (remote.m_port == 0)
        {
            remote.m_port = k_defaultPort;
        }

        const HSteamNetConnection connection = m_state->network->sockets->ConnectByIPAddress(remote, 0, nullptr);

        if (connection == k_HSteamNetConnection_Invalid)
        {
            return networkError("Session::connect: could not start connecting to '" + text + "'");
        }

        std::array<char, SteamNetworkingIPAddr::k_cchMaxString> remoteText{};
        remote.ToString(remoteText.data(), remoteText.size(), true);

        m_state->connection = connection;
        m_state->role = Role::Client;
        m_state->state = ConnectionState::Connecting;
        setEvent(*m_state, std::string("connecting to ") + remoteText.data());

        return {};
    }

    void Session::disconnect()
    {
        if (!m_state || m_state->state == ConnectionState::Offline)
        {
            return;
        }

        ISteamNetworkingSockets& sockets = *m_state->network->sockets;

        if (m_state->connection != k_HSteamNetConnection_Invalid)
        {
            closeWithLinger(*m_state->network, m_state->connection, m_state->listenSocket, "disconnect");
        }
        else if (m_state->listenSocket != k_HSteamListenSocket_Invalid)
        {
            sockets.CloseListenSocket(m_state->listenSocket);
        }

        m_state->listenSocket = k_HSteamListenSocket_Invalid;
        resetConnectionData(*m_state);
        m_state->received.clear();
        m_state->engineMessages.clear();
        m_state->role = Role::None;
        m_state->state = ConnectionState::Offline;
        setEvent(*m_state, "disconnected");
    }

    void Session::pump(uint64_t tick)
    {
        if (!m_state)
        {
            return;
        }

        m_state->received.clear();
        m_state->engineMessages.clear();

        if (m_state->connection != k_HSteamNetConnection_Invalid)
        {
            ++m_state->pumpsSinceHeartbeat;
        }

        if (m_state->state == ConnectionState::Connected)
        {
            sendHeartbeat(*m_state, tick);
            const EResult flushed = m_state->network->sockets->FlushMessagesOnConnection(m_state->connection);
            recordSendResult(*m_state, flushed, "flushing messages");
        }

        runCallbacks(*m_state->network);
        receiveMessages(*m_state);
    }

    Status Session::send(std::span<const std::byte> payload, Delivery delivery)
    {
        if (!m_state || m_state->state != ConnectionState::Connected)
        {
            return Error{ErrorCode::InvalidArgument, "Session::send: not connected"};
        }

        if (payload.size() > k_maxGamePayloadBytes)
        {
            return Error{ErrorCode::InvalidArgument, "Session::send: message too large"};
        }

        return sendMessage(*m_state, encodeGameMessage(payload), delivery);
    }

    std::span<const std::vector<std::byte>> Session::received() const
    {
        if (!m_state)
        {
            return {};
        }

        return m_state->received;
    }

    Role Session::role() const
    {
        return m_state ? m_state->role : Role::None;
    }

    ConnectionState Session::state() const
    {
        return m_state ? m_state->state : ConnectionState::Offline;
    }

    std::optional<ConnectionStats> Session::stats() const
    {
        if (!m_state || m_state->state != ConnectionState::Connected)
        {
            return std::nullopt;
        }

        SteamNetConnectionRealTimeStatus_t status{};

        if (m_state->network->sockets->GetConnectionRealTimeStatus(m_state->connection, &status, 0, nullptr) != k_EResultOK)
        {
            return std::nullopt;
        }

        const float delivered = status.m_flConnectionQualityLocal;

        return ConnectionStats{
            .pingMilliseconds = static_cast<float>(status.m_nPing),
            .packetLoss = delivered >= 0.0f ? 1.0f - delivered : 0.0f,
            .sendBytesPerSecond = status.m_flOutBytesPerSec,
            .receiveBytesPerSecond = status.m_flInBytesPerSec,
            .pendingReliableBytes = static_cast<uint32_t>(status.m_cbPendingReliable)};
    }

    std::optional<uint64_t> Session::remoteTick() const
    {
        return m_state ? m_state->remoteTick : std::nullopt;
    }

    uint64_t Session::heartbeatsReceived() const
    {
        return m_state ? m_state->heartbeatsReceived : 0;
    }

    const std::string& Session::lastEvent() const
    {
        static const std::string k_none;
        return m_state ? m_state->lastEvent : k_none;
    }

    const char* toString(Role role) noexcept
    {
        switch (role)
        {
            case Role::None:
            {
                return "none";
            }
            case Role::Host:
            {
                return "host";
            }
            case Role::Client:
            {
                return "client";
            }
        }

        return "none";
    }

    const char* toString(ConnectionState state) noexcept
    {
        switch (state)
        {
            case ConnectionState::Offline:
            {
                return "offline";
            }
            case ConnectionState::Listening:
            {
                return "listening";
            }
            case ConnectionState::Connecting:
            {
                return "connecting";
            }
            case ConnectionState::Connected:
            {
                return "connected";
            }
        }

        return "offline";
    }
} //namespace lockstep::net
