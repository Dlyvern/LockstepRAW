#ifndef LOCKSTEP_GAME_SHELL_LOBBY_HPP
#define LOCKSTEP_GAME_SHELL_LOBBY_HPP

#include <Lockstep/Core/Result.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::app
{
    struct EngineContext;
} //namespace lockstep::app

namespace game
{
    // Most characters of a player name.
    inline constexpr std::size_t k_maxPlayerNameLength{20};

    // One seat of the lobby (0 the host, 1 the client).
    // name:      the player's name.
    // isReady:   the player pressed Ready.
    // isPresent: someone sits there (the client seat is empty until a client is connected and
    //            has introduced itself).
    struct LobbyPlayer
    {
        std::string name;
        bool isReady{false};
        bool isPresent{false};
    };

    // The pre-game lobby over the network session: both players with their names and ready
    // states, and the level the host picked. Messages are reliable Game messages with their own
    // tag; the host's choice of level and its Start reach the client the same way.
    // isOpen:       the lobby runs (the session hosts or connects).
    // isHost:       this machine hosts.
    // players:      the host's and the client's seat.
    // levelLabels:  the levels the host can pick, by name.
    // levelPaths:   their virtual paths (empty: the arena).
    // levelIndex:   the picked level.
    // status:       the connection line shown under the seats (waiting, connecting, errors).
    // isFailed:     joining failed; status says why.
    // connectSeconds: client: real seconds since connecting started.
    // isIntroduced: the local player's name and ready state were sent on this connection.
    // startLevel:   set once the game should start, with the level's virtual path.
    struct Lobby
    {
        bool isOpen{false};
        bool isHost{false};
        std::array<LobbyPlayer, 2> players{};
        std::vector<std::string> levelLabels;
        std::vector<std::string> levelPaths;
        std::size_t levelIndex{0};
        std::string status;
        bool isFailed{false};
        float connectSeconds{0.0f};
        bool isIntroduced{false};
        std::optional<std::string> startLevel;
    };

    // Starts hosting and opens the lobby (keeps an existing listen started by `--host`).
    // lobby:   the lobby.
    // engine:  Engine services (session, mounted content for the level list).
    // name:    the local player's name.
    // port:    the port to listen on.
    // Returns: success, or why hosting failed.
    [[nodiscard]] lockstep::Status openHostLobby(Lobby& lobby, lockstep::app::EngineContext& engine,
        std::string_view name, uint16_t port);

    // Starts connecting and opens the lobby (keeps a connection started by `--connect`).
    // lobby:   the lobby.
    // engine:  Engine services.
    // name:    the local player's name.
    // address: "ip" or "ip:port".
    // Returns: success, or why the address was refused.
    [[nodiscard]] lockstep::Status openJoinLobby(Lobby& lobby, lockstep::app::EngineContext& engine,
        std::string_view name, std::string_view address);

    // Leaves the lobby and closes the connection.
    // lobby:  the lobby.
    // engine: Engine services.
    void closeLobby(Lobby& lobby, lockstep::app::EngineContext& engine);

    // Once per tick while the lobby is open: reads lobby messages, introduces the local player
    // after connecting, tracks the partner joining and leaving and the client's connection
    // progress and timeout.
    // lobby:       the lobby.
    // engine:      Engine services.
    // tickSeconds: the tick length.
    void tickLobby(Lobby& lobby, lockstep::app::EngineContext& engine, float tickSeconds);

    // Sets and sends the local player's ready state.
    // lobby:   the lobby.
    // engine:  Engine services.
    // isReady: the new state.
    void setLobbyReady(Lobby& lobby, lockstep::app::EngineContext& engine, bool isReady);

    // Host: picks the level and tells the client.
    // lobby:  the lobby.
    // engine: Engine services.
    // index:  index into levelLabels.
    void setLobbyLevel(Lobby& lobby, lockstep::app::EngineContext& engine, std::size_t index);

    // lobby:   the lobby.
    // engine:  Engine services.
    // Returns: true if the host may start: it is ready and the client seat is empty (solo) or
    //          ready.
    [[nodiscard]] bool canStartLobby(const Lobby& lobby, const lockstep::app::EngineContext& engine);

    // Host: tells the client to start and sets startLevel.
    // lobby:  the lobby.
    // engine: Engine services.
    void startLobby(Lobby& lobby, lockstep::app::EngineContext& engine);
} //namespace game

#endif //LOCKSTEP_GAME_SHELL_LOBBY_HPP
