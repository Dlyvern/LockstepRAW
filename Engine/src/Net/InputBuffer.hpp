#ifndef LOCKSTEP_NET_INPUT_BUFFER_HPP
#define LOCKSTEP_NET_INPUT_BUFFER_HPP

#include "SnapshotFormat.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace lockstep::net
{
    // One input the host hands to Game for a tick.
    // input:      the client's input; for a repeat, the previous input's bytes and view tick
    //             under the missing tick.
    // isRepeated: the input of this tick never arrived and the previous one stands in for it.
    struct ConsumedInput
    {
        InputMessage input;
        bool isRepeated{false};
    };

    // Counters of an InputBuffer.
    // repeated: missing inputs replaced by the previous one.
    // starved:  ticks with no input to hand out (the client's pawn waits).
    // skipped:  inputs dropped to cut a delay that grew too long.
    struct InputBufferCounters
    {
        uint64_t repeated{0};
        uint64_t starved{0};
        uint64_t skipped{0};
    };

    // The host's buffer of client inputs: puts redundant, reordered and duplicated arrivals back
    // into tick order and hands Game one input per host tick. When the buffer runs dry it hands
    // out nothing (the client's inputs are late, not lost, and come next tick); a tick missing
    // between later ones is lost and is replaced by the previous input. When it holds more than a
    // few ticks it hands out two per tick until the delay is back down; when it holds far too
    // many it drops the oldest.
    class InputBuffer
    {
    public:
        // Adds a received input. Inputs older than the next tick to hand out and duplicates are
        // ignored.
        // input: the input.
        void insert(const InputMessage& input);

        // Takes the inputs for the next host tick, oldest first: usually one, none while dry,
        // two while catching up.
        // Returns: the inputs.
        [[nodiscard]] std::vector<ConsumedInput> consume();

        // Forgets every input and the tick position (for a new connection).
        void clear();

        // Returns: the client ticks buffered ahead of the next one to hand out.
        [[nodiscard]] std::size_t depth() const;

        // Returns: the newest client tick handed out, or nothing.
        [[nodiscard]] std::optional<uint64_t> lastConsumedTick() const;

        // Returns: the counters since the last clear().
        [[nodiscard]] const InputBufferCounters& counters() const;

    private:
        std::vector<InputMessage> m_inputs;
        std::optional<uint64_t> m_nextTick;
        std::optional<InputMessage> m_last;
        InputBufferCounters m_counters{};
    };
} //namespace lockstep::net

#endif //LOCKSTEP_NET_INPUT_BUFFER_HPP
