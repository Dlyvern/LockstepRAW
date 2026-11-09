#include "ReplicationState.hpp"

#include "SessionState.hpp"
#include "WireFormat.hpp"

#include <algorithm>
#include <iostream>
#include <cstddef>
#include <span>
#include <utility>

namespace lockstep::net
{
    namespace
    {
        constexpr std::size_t k_maxPendingInputs{120};

        void dropAcknowledged(ReplicationState& state, uint64_t inputTick)
        {
            while (!state.pendingInputs.empty() && state.pendingInputs.front().tick <= inputTick)
            {
                state.pendingInputs.pop_front();
            }
        }

        void exposeConsumed(ReplicationState& state)
        {
            state.remoteInputs.clear();

            for (const ConsumedInput& consumed : state.consumedInputs)
            {
                state.remoteInputs.push_back(RemoteInput{
                    .tick = consumed.input.tick,
                    .viewTick = consumed.input.viewTick,
                    .bytes = consumed.input.bytes,
                    .isRepeated = consumed.isRepeated});
            }

            const auto newestSeen = std::find_if(state.consumedInputs.rbegin(), state.consumedInputs.rend(),
                [](const ConsumedInput& consumed) { return !consumed.isRepeated && consumed.input.viewTick; });

            if (newestSeen != state.consumedInputs.rend())
            {
                const double applyTick = static_cast<double>(state.lastTick + 1);
                state.counters.viewLagTicks = applyTick - *newestSeen->input.viewTick;
            }
        }
    } //namespace

    void log(const ReplicationState& state, const std::string& message)
    {
        std::clog << "Net (" << toString(state.session.role()) << "): " << message << '\n';
    }

    void reportSend(ReplicationState& state, const Status& sent, const char* what)
    {
        if (sent)
        {
            state.sendFailureReported = false;
            return;
        }

        const bool isReportedBySession = sent.error().code == ErrorCode::NetworkFailure;

        if (!state.sendFailureReported && !isReportedBySession)
        {
            state.sendFailureReported = true;
            log(state, std::string("sending ") + what + " failed: " + sent.error().message);
        }
    }

    void sendInputs(ReplicationState& state, uint64_t tick)
    {
        if (state.localInput)
        {
            state.pendingInputs.push_back(InputMessage{
                .tick = tick, .viewTick = state.snapshots.renderTick(), .bytes = std::move(*state.localInput)});
        }

        state.localInput.reset();

        while (state.pendingInputs.size() > k_maxPendingInputs)
        {
            state.pendingInputs.pop_front();
        }

        if (state.pendingInputs.empty())
        {
            return;
        }

        const std::size_t count = std::min({state.pendingInputs.size(), k_inputRedundancy, k_maxInputsPerMessage});
        const std::vector<InputMessage> newest(state.pendingInputs.end() - static_cast<std::ptrdiff_t>(count),
            state.pendingInputs.end());
        const Status sent = sendEngineMessage(sessionState(state.session), encodeInputs(newest), Delivery::Unreliable);
        reportSend(state, sent, "input");

        if (sent)
        {
            ++state.counters.inputsSent;
        }
    }

    void receiveInputs(ReplicationState& state)
    {
        for (const std::vector<std::byte>& message : sessionState(state.session).engineMessages)
        {
            if (messageKind(message) != MessageKind::Input)
            {
                continue;
            }

            const std::optional<std::vector<InputMessage>> inputs = decodeInputs(message);

            if (!inputs)
            {
                continue;
            }

            ++state.counters.inputsReceived;

            for (const InputMessage& input : *inputs)
            {
                state.inputBuffer.insert(input);
            }
        }

        state.consumedInputs = state.inputBuffer.consume();
        exposeConsumed(state);
    }

    void receiveAcknowledgement(ReplicationState& state, const Snapshot& snapshot)
    {
        if (state.ack && snapshot.tick <= state.ack->hostTick)
        {
            return;
        }

        state.ack = ReplicationState::Acknowledgement{
            .hostTick = snapshot.tick, .inputTick = snapshot.ackInputTick, .clientState = snapshot.clientState};
        state.hasNewAck = true;

        if (snapshot.ackInputTick)
        {
            dropAcknowledged(state, *snapshot.ackInputTick);
        }
    }

    void clearInputs(ReplicationState& state)
    {
        state.localInput.reset();
        state.pendingInputs.clear();
        state.ack.reset();
        state.hasNewAck = false;
        state.inputBuffer.clear();
        state.consumedInputs.clear();
        state.remoteInputs.clear();
        state.clientState.clear();
    }
} //namespace lockstep::net
