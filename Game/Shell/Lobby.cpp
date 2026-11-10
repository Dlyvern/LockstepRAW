#include "Shell/Lobby.hpp"

#include <Lockstep/App/GameApi.hpp>
#include <Lockstep/Assets/FileSystem.hpp>
#include <Lockstep/Net/Session.hpp>

#include <array>
#include <iostream>
#include <span>
#include <utility>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;

        using lockstep::app::EngineContext;

        constexpr std::byte k_lobbyTag{0x59};
        constexpr std::byte k_playerKind{0x01};
        constexpr std::byte k_levelKind{0x02};
        constexpr std::byte k_startKind{0x03};
        constexpr std::size_t k_hostSeat{0};
        constexpr std::size_t k_clientSeat{1};
        constexpr std::size_t k_maxPathLength{255};
        constexpr float k_connectTimeoutSeconds{12.0f};

        struct LevelChoice
        {
            std::string_view label;
            std::string_view path;
        };

        constexpr std::array<LevelChoice, 4> k_levelChoices{{
            {"STORY - THE MORNING", "levels/district/scene"},
            {"HOSPITAL A (TEST)", "levels/hospital_a/scene"},
            {"STATION", "levels/station/scene"},
            {"HOSPITAL B", "levels/hospital_b/scene"}}};

        std::size_t localSeat(const Lobby& lobby)
        {
            return lobby.isHost ? k_hostSeat : k_clientSeat;
        }

        std::size_t remoteSeat(const Lobby& lobby)
        {
            return lobby.isHost ? k_clientSeat : k_hostSeat;
        }

        bool isConnected(const EngineContext& engine)
        {
            return engine.session.state() == net::ConnectionState::Connected;
        }

        void appendText(std::vector<std::byte>& bytes, std::string_view text, std::size_t limit)
        {
            const std::size_t length = std::min(text.size(), limit);
            bytes.push_back(static_cast<std::byte>(length));

            for (std::size_t index = 0; index < length; ++index)
            {
                bytes.push_back(static_cast<std::byte>(text[index]));
            }
        }

        std::optional<std::string> readText(std::span<const std::byte> bytes, std::size_t offset)
        {
            if (bytes.size() <= offset || bytes.size() != offset + 1 + static_cast<std::size_t>(bytes[offset]))
            {
                return std::nullopt;
            }

            std::string text;

            for (const std::byte byte : bytes.subspan(offset + 1))
            {
                text.push_back(static_cast<char>(byte));
            }

            return text;
        }

        void send(EngineContext& engine, const std::vector<std::byte>& message)
        {
            if (!isConnected(engine))
            {
                return;
            }

            if (auto sent = engine.session.send(message, net::Delivery::Reliable); !sent)
            {
                std::clog << "Game: lobby message not sent: " << sent.error().message << '\n';
            }
        }

        void sendPlayer(const Lobby& lobby, EngineContext& engine)
        {
            const LobbyPlayer& player = lobby.players[localSeat(lobby)];
            std::vector<std::byte> message{k_lobbyTag, k_playerKind, player.isReady ? std::byte{1} : std::byte{0}};
            appendText(message, player.name, k_maxPlayerNameLength);
            send(engine, message);
        }

        void sendLevel(const Lobby& lobby, EngineContext& engine)
        {
            std::vector<std::byte> message{k_lobbyTag, k_levelKind};
            appendText(message, lobby.levelLabels.empty() ? std::string_view() : lobby.levelLabels[lobby.levelIndex],
                k_maxPathLength);
            send(engine, message);
        }

        void fillLevels(Lobby& lobby, const EngineContext& engine)
        {
            lobby.levelLabels.clear();
            lobby.levelPaths.clear();

            for (const LevelChoice& choice : k_levelChoices)
            {
                if (engine.files.exists(choice.path))
                {
                    lobby.levelLabels.emplace_back(choice.label);
                    lobby.levelPaths.emplace_back(choice.path);
                }
            }

            if (lobby.levelLabels.empty())
            {
                lobby.levelLabels.emplace_back("TEST ARENA (NO CONTENT)");
                lobby.levelPaths.emplace_back();
            }

            lobby.levelIndex = 0;
        }

        void openLobby(Lobby& lobby, std::string_view name, bool isHost)
        {
            lobby = Lobby{};
            lobby.isOpen = true;
            lobby.isHost = isHost;
            LobbyPlayer& local = lobby.players[isHost ? k_hostSeat : k_clientSeat];
            local.name = std::string(name.substr(0, k_maxPlayerNameLength));
            local.isPresent = true;
        }

        void receive(Lobby& lobby, std::span<const std::byte> message)
        {
            if (message.size() < 2 || message[0] != k_lobbyTag)
            {
                return;
            }

            const std::byte kind = message[1];
            LobbyPlayer& remote = lobby.players[remoteSeat(lobby)];

            if (kind == k_playerKind && message.size() >= 3)
            {
                if (const std::optional<std::string> name = readText(message, 3))
                {
                    const bool isNew = !remote.isPresent;
                    remote.name = name->empty() ? std::string("OPERATOR") : *name;
                    remote.isReady = message[2] != std::byte{0};
                    remote.isPresent = true;

                    if (isNew)
                    {
                        std::clog << "Game: lobby: " << remote.name << " joined\n";
                    }
                }
            }
            else if (kind == k_levelKind && !lobby.isHost)
            {
                if (const std::optional<std::string> label = readText(message, 2))
                {
                    lobby.levelLabels = {*label};
                    lobby.levelPaths = {std::string{}};
                    lobby.levelIndex = 0;
                }
            }
            else if (kind == k_startKind && !lobby.isHost)
            {
                if (const std::optional<std::string> level = readText(message, 2))
                {
                    std::clog << "Game: lobby: the host starts '" << *level << "'\n";
                    lobby.startLevel = *level;
                }
            }
        }
    } //namespace

    lockstep::Status openHostLobby(Lobby& lobby, EngineContext& engine, std::string_view name, uint16_t port)
    {
        if (engine.session.role() != net::Role::Host)
        {
            engine.session.disconnect();

            if (auto hosted = engine.session.host(port); !hosted)
            {
                return hosted;
            }
        }

        openLobby(lobby, name, true);
        fillLevels(lobby, engine);
        lobby.players[k_hostSeat].isReady = false;
        lobby.status = "LISTENING ON PORT " + std::to_string(port) + " - WAITING FOR PARTNER";
        std::clog << "Game: lobby: hosting on port " << port << '\n';

        return {};
    }

    lockstep::Status openJoinLobby(Lobby& lobby, EngineContext& engine, std::string_view name, std::string_view address)
    {
        if (engine.session.role() != net::Role::Client)
        {
            engine.session.disconnect();

            if (auto connecting = engine.session.connect(address); !connecting)
            {
                return connecting;
            }
        }

        openLobby(lobby, name, false);
        fillLevels(lobby, engine);
        lobby.status = "CONNECTING TO " + std::string(address) + " ...";
        std::clog << "Game: lobby: connecting to " << address << '\n';

        return {};
    }

    void closeLobby(Lobby& lobby, EngineContext& engine)
    {
        if (lobby.isOpen)
        {
            engine.session.disconnect();
            std::clog << "Game: lobby closed\n";
        }

        lobby = Lobby{};
    }

    void tickLobby(Lobby& lobby, EngineContext& engine, float tickSeconds)
    {
        if (!lobby.isOpen)
        {
            return;
        }

        for (const std::vector<std::byte>& message : engine.session.received())
        {
            receive(lobby, message);
        }

        LobbyPlayer& remote = lobby.players[remoteSeat(lobby)];

        if (isConnected(engine))
        {
            if (!lobby.isIntroduced)
            {
                lobby.isIntroduced = true;
                sendPlayer(lobby, engine);

                if (lobby.isHost)
                {
                    sendLevel(lobby, engine);
                }
            }

            lobby.status = remote.isPresent ? (lobby.isHost ? "PARTNER CONNECTED" : "CONNECTED TO HOST")
                                            : "CONNECTED - WAITING FOR PARTNER INFO";
            return;
        }

        if (lobby.isIntroduced)
        {
            lobby.isIntroduced = false;
            std::clog << "Game: lobby: partner left\n";

            if (!lobby.isHost)
            {
                lobby.isFailed = true;
                lobby.status = "CONNECTION LOST: " + engine.session.lastEvent();
                return;
            }
        }

        remote = LobbyPlayer{};

        if (lobby.isHost || lobby.isFailed)
        {
            return;
        }

        lobby.connectSeconds += tickSeconds;

        if (engine.session.state() == net::ConnectionState::Offline || lobby.connectSeconds > k_connectTimeoutSeconds)
        {
            const std::string reason = engine.session.lastEvent().empty() ? std::string("no answer from the host")
                                                                          : engine.session.lastEvent();
            engine.session.disconnect();
            lobby.isFailed = true;
            lobby.status = "CONNECTION FAILED: " + reason;
            std::clog << "Game: lobby: " << lobby.status << '\n';
        }
    }

    void setLobbyReady(Lobby& lobby, EngineContext& engine, bool isReady)
    {
        lobby.players[localSeat(lobby)].isReady = isReady;
        sendPlayer(lobby, engine);
        std::clog << "Game: lobby: " << (isReady ? "ready" : "not ready") << '\n';
    }

    void setLobbyLevel(Lobby& lobby, EngineContext& engine, std::size_t index)
    {
        if (!lobby.isHost || lobby.levelLabels.empty())
        {
            return;
        }

        lobby.levelIndex = index % lobby.levelLabels.size();
        sendLevel(lobby, engine);
    }

    bool canStartLobby(const Lobby& lobby, const EngineContext& engine)
    {
        const LobbyPlayer& client = lobby.players[k_clientSeat];
        const bool isClientFine = !isConnected(engine) ? !client.isPresent : (client.isPresent && client.isReady);

        return lobby.isOpen && lobby.isHost && lobby.players[k_hostSeat].isReady && isClientFine;
    }

    void startLobby(Lobby& lobby, EngineContext& engine)
    {
        const std::string level = lobby.levelIndex < lobby.levelPaths.size() ? lobby.levelPaths[lobby.levelIndex]
                                                                             : std::string{};
        std::vector<std::byte> message{k_lobbyTag, k_startKind};
        appendText(message, level, k_maxPathLength);
        send(engine, message);
        lobby.startLevel = level;
        std::clog << "Game: lobby: starting '" << level << "'\n";
    }
} //namespace game
