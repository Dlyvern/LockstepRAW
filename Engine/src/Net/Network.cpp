#include "Lockstep/Net/Network.hpp"

#include "NetworkState.hpp"
#include "SessionState.hpp"

#include <steam/steamnetworkingsockets.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace lockstep::net
{
    namespace
    {
        using Clock = std::chrono::steady_clock;

        constexpr float k_maxLossPercent{100.0f};
        constexpr std::chrono::milliseconds k_lingerTime{1000};
        constexpr std::chrono::milliseconds k_shutdownPollInterval{10};
        constexpr int32 k_connectedTimeoutMilliseconds{4000};

        NetworkState*& activeNetwork()
        {
            static NetworkState* network{nullptr};
            return network;
        }

        void onStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
        {
            NetworkState* network = activeNetwork();

            if (network == nullptr || info == nullptr)
            {
                return;
            }

            for (SessionState* session : network->sessions)
            {
                if (ownsConnection(*session, *info))
                {
                    handleStatusChange(*session, *info);
                    return;
                }
            }

            network->sockets->CloseConnection(info->m_hConn, 0, nullptr, false);
        }

        void onDebugOutput(ESteamNetworkingSocketsDebugOutputType, const char* message)
        {
            std::clog << "Net (GNS): " << message << '\n';
        }

        Error networkError(std::string message)
        {
            return Error{ErrorCode::NetworkFailure, std::move(message)};
        }

        bool hasUndeliveredReliableData(ISteamNetworkingSockets& sockets, HSteamNetConnection connection)
        {
            SteamNetConnectionRealTimeStatus_t status{};

            if (sockets.GetConnectionRealTimeStatus(connection, &status, 0, nullptr) != k_EResultOK)
            {
                return false;
            }

            return status.m_cbPendingReliable > 0 || status.m_cbSentUnackedReliable > 0;
        }

        void closeExpiredLingers(NetworkState& network, Clock::time_point now)
        {
            std::erase_if(network.lingers, [&](const Linger& linger)
            {
                if (linger.deadline > now)
                {
                    return false;
                }

                if (linger.listenSocket != k_HSteamListenSocket_Invalid)
                {
                    network.sockets->CloseListenSocket(linger.listenSocket);
                }

                return true;
            });
        }

        void waitForLingers(NetworkState& network)
        {
            while (!network.lingers.empty())
            {
                network.sockets->RunCallbacks();
                std::this_thread::sleep_for(k_shutdownPollInterval);
                closeExpiredLingers(network, Clock::now());
            }
        }
    } //namespace

    NetworkState::~NetworkState()
    {
        if (activeNetwork() == this)
        {
            waitForLingers(*this);
            activeNetwork() = nullptr;
            GameNetworkingSockets_Kill();
        }
    }

    NetworkState& networkState(Network& network)
    {
        assert(network.m_state != nullptr && "Network was moved from");
        return *network.m_state;
    }

    void closeWithLinger(NetworkState& network, HSteamNetConnection connection, HSteamListenSocket listenSocket,
        const char* reason)
    {
        const bool needsLinger = hasUndeliveredReliableData(*network.sockets, connection);
        network.sockets->CloseConnection(connection, 0, reason, true);

        if (needsLinger)
        {
            network.lingers.push_back(Linger{.listenSocket = listenSocket, .deadline = Clock::now() + k_lingerTime});
        }
        else if (listenSocket != k_HSteamListenSocket_Invalid)
        {
            network.sockets->CloseListenSocket(listenSocket);
        }
    }

    void endLingers(NetworkState& network)
    {
        closeExpiredLingers(network, Clock::time_point::max());
    }

    void addSession(NetworkState& network, SessionState& session)
    {
        network.sessions.push_back(&session);
    }

    void removeSession(NetworkState& network, SessionState& session)
    {
        std::erase(network.sessions, &session);
    }

    void runCallbacks(NetworkState& network)
    {
        network.sockets->RunCallbacks();
        closeExpiredLingers(network, Clock::now());
    }

    Result<Network> Network::create()
    {
        if (activeNetwork() != nullptr)
        {
            return Error{ErrorCode::InvalidArgument, "Network::create: a Network already exists in this process"};
        }

        SteamNetworkingErrMsg errorMessage{};

        if (!GameNetworkingSockets_Init(nullptr, errorMessage))
        {
            return networkError(std::string("Network::create: GameNetworkingSockets_Init failed: ") + errorMessage);
        }

        auto state = std::make_unique<NetworkState>();
        activeNetwork() = state.get();
        state->sockets = SteamNetworkingSockets();
        state->utils = SteamNetworkingUtils();
        state->utils->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Important, &onDebugOutput);

        if (!state->utils->SetGlobalCallback_SteamNetConnectionStatusChanged(&onStatusChanged))
        {
            return networkError("Network::create: could not install the connection status callback");
        }

        if (!state->utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_TimeoutConnected, k_connectedTimeoutMilliseconds))
        {
            return networkError("Network::create: could not set the connection timeout");
        }

        return Network(std::move(state));
    }

    Network::Network(std::unique_ptr<NetworkState> state)
        : m_state(std::move(state))
    {
    }

    Network::~Network() = default;

    Network::Network(Network&& other) noexcept = default;

    Network& Network::operator=(Network&& other) noexcept = default;

    Status Network::setSimulatedConditions(const SimulatedConditions& conditions)
    {
        if (!m_state)
        {
            return Error{ErrorCode::InvalidArgument, "Network::setSimulatedConditions: network was moved from"};
        }

        const float lossPercent = std::clamp(conditions.lossPercent, 0.0f, k_maxLossPercent);
        const int32 lagMilliseconds = static_cast<int32>(std::min<uint32_t>(conditions.lagMilliseconds, INT32_MAX));
        ISteamNetworkingUtils& utils = *m_state->utils;

        const bool applied = utils.SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send, lagMilliseconds)
            && utils.SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send, lossPercent);

        if (!applied)
        {
            return networkError("Network::setSimulatedConditions: the networking library rejected a setting");
        }

        m_state->conditions = SimulatedConditions{.lagMilliseconds = conditions.lagMilliseconds, .lossPercent = lossPercent};

        return {};
    }

    SimulatedConditions Network::simulatedConditions() const
    {
        return m_state ? m_state->conditions : SimulatedConditions{};
    }
} //namespace lockstep::net
