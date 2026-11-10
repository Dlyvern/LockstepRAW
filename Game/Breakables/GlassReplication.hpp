#ifndef LOCKSTEP_GAME_BREAKABLES_GLASS_REPLICATION_HPP
#define LOCKSTEP_GAME_BREAKABLES_GLASS_REPLICATION_HPP

#include "Breakables/BreakableGlass.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace lockstep::net
{
    class Session;
} //namespace lockstep::net

namespace game
{
    // Most breaks one glass message carries (more in one tick still arrive in the broken set,
    // just without shards).
    inline constexpr std::size_t k_maxGlassBreaksPerMessage{64};

    // The host's glass message: the whole broken set (so a late joiner catches up) and the breaks
    // since the previous message (so the client shows them).
    // isBroken: per pane.
    // breaks:   new breaks, at most k_maxGlassBreaksPerMessage.
    struct GlassMessage
    {
        std::vector<bool> isBroken;
        std::vector<GlassBreak> breaks;
    };

    // Packs a glass message (reliable Game message).
    // message: the message; at most k_maxGlassPanes panes.
    // Returns: the bytes.
    [[nodiscard]] std::vector<std::byte> encodeGlassMessage(const GlassMessage& message);

    // Unpacks a glass message.
    // bytes:   a received Game message.
    // Returns: the message, or nothing if `bytes` is not a valid glass message.
    [[nodiscard]] std::optional<GlassMessage> decodeGlassMessage(std::span<const std::byte> bytes);

    // Host, end of a tick: sends the broken set with the new breaks when something broke, or the
    // whole set once after the client connects. Levels without panes send nothing.
    // glass:   the level's glass.
    // session: the network session.
    void publishGlass(BreakableGlass& glass, lockstep::net::Session& session);

    // Client: takes a Game message if it is the glass message, and applies it to the loaded level:
    // its new breaks are shown, other panes it lists as broken break silently (late join). A
    // message for a different pane count (another level) is dropped.
    // glass:   the level's glass.
    // level:   the loaded level, or null (the arena).
    // physics: the physics world.
    // bytes:   a received Game message.
    // Returns: true if it was a glass message.
    bool receiveGlassMessage(BreakableGlass& glass, SceneLevel* level, lockstep::physics::PhysicsWorld& physics,
        std::span<const std::byte> bytes);
} //namespace game

#endif //LOCKSTEP_GAME_BREAKABLES_GLASS_REPLICATION_HPP
