#include "Lockstep/Net/Session.hpp"

#include "NetworkState.hpp"
#include "SessionState.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

namespace lockstep::net
{
    namespace
    {
        constexpr int k_sessionFullReason{k_ESteamNetConnectionEnd_App_Min + 1};
        constexpr int k_replacedReason{k_ESteamNetConnectionEnd_App_Min + 2};
        constexpr uint64_t k_unresponsivePeerPumps{60};

        bool isSameIp(const SteamNetworkingIPAddr& first, const SteamNetworkingIPAddr& second)
        {
            return std::equal(std::begin(first.m_ipv6), std::end(first.m_ipv6), std::begin(second.m_ipv6));
        }

        bool canReplacePeer(SessionState& session, const SteamNetConnectionStatusChangedCallback_t& info)
        {
            if (session.pumpsSinceHeartbeat >= k_unresponsivePeerPumps)
            {
                return true;
            }

            SteamNetConnectionInfo_t current{};

            return session.network->sockets->GetConnectionInfo(session.connection, &current)
                && isSameIp(current.m_addrRemote, info.m_info.m_addrRemote);
        }

        void closePeer(SessionState& session, std::string event)
        {
            session.network->sockets->CloseConnection(session.connection, 0, nullptr, false);
            resetConnectionData(session);
            session.state = session.role == Role::Host ? ConnectionState::Listening : ConnectionState::Offline;

            if (session.role == Role::Client)
            {
                session.role = Role::None;
            }

            setEvent(session, std::move(event));
        }

        void acceptIncoming(SessionState& session, const SteamNetConnectionStatusChangedCallback_t& info)
        {
            ISteamNetworkingSockets& sockets = *session.network->sockets;

            if (session.connection != k_HSteamNetConnection_Invalid)
            {
                if (!canReplacePeer(session, info))
                {
                    sockets.CloseConnection(info.m_hConn, k_sessionFullReason, "session full", false);
                    setEvent(session, "rejected a second player: the session is full");
                    return;
                }

                sockets.CloseConnection(session.connection, k_replacedReason, "replaced by a new connection", false);
                resetConnectionData(session);
                setEvent(session, "replaced the unresponsive or restarted player");
            }

            if (sockets.AcceptConnection(info.m_hConn) != k_EResultOK)
            {
                sockets.CloseConnection(info.m_hConn, 0, nullptr, false);
                setEvent(session, "could not accept an incoming player");
                return;
            }

            session.connection = info.m_hConn;
            session.state = ConnectionState::Connecting;
            setEvent(session, std::string("accepting a player from ") + info.m_info.m_szConnectionDescription);
        }
    } //namespace

    void setEvent(SessionState& session, std::string event)
    {
        std::clog << "Net (" << toString(session.role) << "): " << event << '\n';
        session.lastEvent = std::move(event);
    }

    void resetConnectionData(SessionState& session)
    {
        session.connection = k_HSteamNetConnection_Invalid;
        session.remoteTick.reset();
        session.heartbeatsReceived = 0;
        session.pumpsSinceHeartbeat = 0;
    }

    bool ownsConnection(const SessionState& session, const SteamNetConnectionStatusChangedCallback_t& info)
    {
        const bool onListenSocket = session.listenSocket != k_HSteamListenSocket_Invalid
            && info.m_info.m_hListenSocket == session.listenSocket;

        return onListenSocket || (session.connection != k_HSteamNetConnection_Invalid && info.m_hConn == session.connection);
    }

    void handleStatusChange(SessionState& session, const SteamNetConnectionStatusChangedCallback_t& info)
    {
        const bool isPeer = info.m_hConn == session.connection;

        switch (info.m_info.m_eState)
        {
            case k_ESteamNetworkingConnectionState_Connecting:
            {
                if (!isPeer && session.role == Role::Host)
                {
                    acceptIncoming(session, info);
                }

                break;
            }
            case k_ESteamNetworkingConnectionState_Connected:
            {
                if (isPeer)
                {
                    session.state = ConnectionState::Connected;
                    setEvent(session, std::string("connected to ") + info.m_info.m_szConnectionDescription);
                }

                break;
            }
            case k_ESteamNetworkingConnectionState_ClosedByPeer:
            case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            {
                if (isPeer)
                {
                    const char* cause = info.m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer
                        ? "closed by the remote player"
                        : "connection problem";
                    closePeer(session, std::string("disconnected (") + cause + "): " + info.m_info.m_szEndDebug);
                }
                else
                {
                    session.network->sockets->CloseConnection(info.m_hConn, 0, nullptr, false);
                }

                break;
            }
            default:
            {
                break;
            }
        }
    }
} //namespace lockstep::net
