#include "Breakables/GlassDevTools.hpp"

#if LOCKSTEP_DEV_TOOLS

#include "GameState.hpp"

#include <Lockstep/Assets/SceneCollision.hpp>
#include <Lockstep/Net/Session.hpp>

#include <imgui.h>

#include <charconv>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

namespace game
{
    namespace
    {
        namespace net = lockstep::net;

        using lockstep::app::EngineContext;
        using lockstep::app::GameState;

        constexpr std::string_view k_breakFlag{"--dev-break-glass"};
        constexpr uint64_t k_breakTick{300};

        std::optional<std::size_t> requestedBreaks(std::span<const char* const> arguments)
        {
            for (std::size_t index = 0; index + 1 < arguments.size(); ++index)
            {
                if (arguments[index] == nullptr || arguments[index + 1] == nullptr
                    || std::string_view(arguments[index]) != k_breakFlag)
                {
                    continue;
                }

                const std::string_view value = arguments[index + 1];
                std::size_t count = 0;
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), count);

                if (parsed.ec == std::errc{})
                {
                    return count;
                }
            }

            return std::nullopt;
        }

        bool breakPane(GameState& state, EngineContext& engine, std::size_t pane)
        {
            const GlassPane& glassPane = state.glass.panes[pane];

            for (const uint32_t instance : glassPane.instances)
            {
                if (const std::optional<lockstep::physics::BodyId> body
                    = lockstep::assets::findInstanceBody(state.level->collision, instance))
                {
                    return shatterGlass(state.glass, *state.level, engine.physics, *body, glassPane.centre,
                        glassPane.normal);
                }
            }

            return false;
        }

        void breakIntactPanes(GameState& state, EngineContext& engine, std::size_t count)
        {
            for (std::size_t pane = 0; pane < state.glass.panes.size() && count > 0; ++pane)
            {
                if (!state.glass.isBroken[pane] && breakPane(state, engine, pane))
                {
                    --count;
                }
            }
        }

        std::optional<std::size_t> nearestIntactPane(const GameState& state)
        {
            std::optional<std::size_t> nearest;
            float nearestDistance = std::numeric_limits<float>::max();

            for (std::size_t pane = 0; pane < state.glass.panes.size(); ++pane)
            {
                const float distance = math::length(state.glass.panes[pane].centre - state.effects.eye);

                if (!state.glass.isBroken[pane] && distance < nearestDistance)
                {
                    nearest = pane;
                    nearestDistance = distance;
                }
            }

            return nearest;
        }
    } //namespace

    void updateGlassDevBreaks(GameState& state, EngineContext& engine)
    {
        if (!state.level || state.glass.panes.empty() || state.lastTick != k_breakTick)
        {
            return;
        }

        if (const std::optional<std::size_t> count = requestedBreaks(engine.arguments))
        {
            breakIntactPanes(state, engine, *count);
        }
    }

    void glassDevSection(GameState& state, EngineContext& engine)
    {
        if (!ImGui::CollapsingHeader("Breakable glass"))
        {
            return;
        }

        ImGui::Text("Panes: %zu, broken: %zu, floor shards: %zu", state.glass.panes.size(),
            brokenPaneCount(state.glass), state.glassShatter.debris.size());

        if (engine.session.role() == net::Role::Client || !state.level)
        {
            return;
        }

        if (ImGui::Button("Break nearest pane"))
        {
            if (const std::optional<std::size_t> pane = nearestIntactPane(state))
            {
                breakPane(state, engine, *pane);
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Break all panes"))
        {
            breakIntactPanes(state, engine, state.glass.panes.size());
        }
    }
} //namespace game

#endif
