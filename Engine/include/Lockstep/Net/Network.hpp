#ifndef LOCKSTEP_NET_NETWORK_HPP
#define LOCKSTEP_NET_NETWORK_HPP

#include "Lockstep/Core/Result.hpp"

#include <cstdint>
#include <memory>

namespace lockstep::net
{
    struct NetworkState;

    // Artificial network trouble for testing, applied to every packet this process sends. With
    // both peers on one machine (or both sessions in one process) the round trip gets the lag
    // twice.
    // lagMilliseconds: delay added to each sent packet.
    // lossPercent:     share of sent packets dropped, in [0, 100].
    struct SimulatedConditions
    {
        uint32_t lagMilliseconds{0};
        float lossPercent{0.0f};
    };

    // The networking library for this process. Exactly one may exist at a time; every Session
    // is created from it and must be destroyed before it. Host and client sessions can share
    // one Network, so both can run in a single process.
    class Network
    {
    public:
        // Starts the networking library.
        // Returns: the network, or an error if one already exists in this process or the
        //          library failed to start.
        [[nodiscard]] static Result<Network> create();

        // Shuts the networking library down. All Sessions must already be destroyed.
        ~Network();

        // Takes over another Network. `other` becomes empty.
        Network(Network&& other) noexcept;

        // Shuts this Network down, then takes over `other`. `other` becomes empty.
        Network& operator=(Network&& other) noexcept;

        Network(const Network&) = delete;
        Network& operator=(const Network&) = delete;

        // Sets the simulated lag and loss for all sessions of this process.
        // conditions: the lag and loss to simulate. lossPercent is clamped to [0, 100].
        // Returns:    success, or an error if the library rejected a setting.
        [[nodiscard]] Status setSimulatedConditions(const SimulatedConditions& conditions);

        // Returns: the simulated lag and loss currently applied.
        [[nodiscard]] SimulatedConditions simulatedConditions() const;

    private:
        friend NetworkState& networkState(Network& network);

        explicit Network(std::unique_ptr<NetworkState> state);

        std::unique_ptr<NetworkState> m_state;
    };
} //namespace lockstep::net

#endif //LOCKSTEP_NET_NETWORK_HPP
