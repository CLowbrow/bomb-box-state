#include "fixtures.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace game_rules::detail {
namespace {

constexpr std::array colors{
    SwitchColor::red,
    SwitchColor::green,
    SwitchColor::blue,
    SwitchColor::yellow,
};

[[nodiscard]] const Cell* find_cell(const LevelDefinition& level,
                                    const Coordinate coordinate) noexcept
{
    const auto found = std::find_if(level.cells.begin(), level.cells.end(),
                                    [coordinate](const Cell& cell) {
                                        return cell.coordinate == coordinate;
                                    });
    return found == level.cells.end() ? nullptr : &*found;
}

[[nodiscard]] bool contains(const std::vector<SwitchColor>& values,
                            const SwitchColor color) noexcept
{
    return std::find(values.begin(), values.end(), color) != values.end();
}

[[nodiscard]] bool contains(const std::vector<Coordinate>& values,
                            const Coordinate coordinate) noexcept
{
    return std::find(values.begin(), values.end(), coordinate) != values.end();
}

[[nodiscard]] bool occupied(const ResolvedState& state,
                            const Coordinate coordinate) noexcept
{
    return std::any_of(state.entities.begin(), state.entities.end(),
                       [coordinate](const Entity& entity) {
                           return entity.coordinate == coordinate;
                       });
}

[[nodiscard]] bool switch_pressed(const LevelDefinition& level,
                                  const ResolvedState& state,
                                  const Fixture& fixture) noexcept
{
    const Cell* const cell = find_cell(level, fixture.coordinate);
    if (cell == nullptr) {
        return false;
    }
    const auto* const flat = std::get_if<FlatCell>(&cell->geometry);
    if (flat == nullptr) {
        return false;
    }
    const Height floor = Height::from_elevation(flat->elevation);
    return std::any_of(state.entities.begin(), state.entities.end(),
                       [&fixture, floor](const Entity& entity) {
                           return entity.coordinate == fixture.coordinate
                               && entity.bottom == floor;
                       });
}

[[nodiscard]] bool player_touches_exit(const LevelDefinition& level,
                                       const ResolvedState& state) noexcept
{
    const auto player = std::find_if(state.entities.begin(), state.entities.end(),
                                     [](const Entity& entity) {
                                         return entity.kind == EntityKind::player;
                                     });
    if (player == state.entities.end()) {
        return false;
    }
    const auto fixture = std::find_if(level.fixtures.begin(), level.fixtures.end(),
                                      [player](const Fixture& value) {
                                          return value.coordinate == player->coordinate
                                              && std::holds_alternative<ExitTeleporter>(value.kind);
                                      });
    if (fixture == level.fixtures.end()) {
        return false;
    }
    const Cell* const cell = find_cell(level, player->coordinate);
    const auto* const flat = cell == nullptr ? nullptr : std::get_if<FlatCell>(&cell->geometry);
    return flat != nullptr && player->bottom == Height::from_elevation(flat->elevation);
}

void apply_win(ResolvedState& state, std::vector<GameplayEvent>& events)
{
    events.erase(std::remove_if(events.begin(), events.end(), [](const GameplayEvent& event) {
                     return std::holds_alternative<PlayerCrushedEvent>(event)
                         || std::holds_alternative<LevelLostEvent>(event);
                 }),
                 events.end());
    state.outcome = Outcome::won;
    events.emplace_back(LevelWonEvent{});
}

void derive_fixture_state(const LevelDefinition& level,
                          ResolvedState& state,
                          std::vector<GameplayEvent>& events)
{
    std::vector<SwitchColor> active_colors;
    for (const SwitchColor color : colors) {
        bool any = false;
        bool all_pressed = true;
        for (const Fixture& fixture : level.fixtures) {
            const auto* const switch_fixture = std::get_if<Switch>(&fixture.kind);
            if (switch_fixture == nullptr || switch_fixture->color != color) {
                continue;
            }
            any = true;
            all_pressed = all_pressed && switch_pressed(level, state, fixture);
        }
        if (any && all_pressed) {
            active_colors.push_back(color);
        }
    }

    for (const SwitchColor color : colors) {
        const bool was_active = contains(state.active_switch_colors, color);
        const bool is_active = contains(active_colors, color);
        if (was_active != is_active) {
            events.emplace_back(SwitchChangedEvent{color, is_active});
        }
    }

    std::vector<Coordinate> open_doors;
    for (const Fixture& fixture : level.fixtures) {
        const auto* const door = std::get_if<Door>(&fixture.kind);
        if (door == nullptr) {
            continue;
        }
        const bool open = contains(active_colors, door->color)
            || occupied(state, fixture.coordinate);
        if (open) {
            open_doors.push_back(fixture.coordinate);
        }
        const bool was_open = contains(state.open_doors, fixture.coordinate);
        if (open && !was_open) {
            events.emplace_back(DoorOpenedEvent{fixture.coordinate, door->color});
        } else if (!open && was_open) {
            events.emplace_back(DoorClosedEvent{fixture.coordinate, door->color});
        }
    }

    state.active_switch_colors = std::move(active_colors);
    state.open_doors = std::move(open_doors);
}

} // namespace

bool is_effectively_open_door(const LevelDefinition& level,
                              const ResolvedState& state,
                              const Coordinate coordinate) noexcept
{
    const auto fixture = std::find_if(level.fixtures.begin(), level.fixtures.end(),
                                      [coordinate](const Fixture& value) {
                                          return value.coordinate == coordinate
                                              && std::holds_alternative<Door>(value.kind);
                                      });
    return fixture != level.fixtures.end() && contains(state.open_doors, coordinate);
}

std::optional<TickResult> resolve_initial_fixture_tick(const LevelDefinition& level,
                                                       const ResolvedState& state,
                                                       const std::uint32_t tick_index)
{
    ResolvedState next = state;
    std::vector<GameplayEvent> events;
    if (player_touches_exit(level, next)) {
        apply_win(next, events);
    } else {
        derive_fixture_state(level, next, events);
    }
    if (events.empty() && next == state) {
        return std::nullopt;
    }
    return TickResult{tick_index, std::move(events), std::move(next)};
}

void resolve_fixtures_after_tick(const LevelDefinition& level, TickResult& tick)
{
    if (player_touches_exit(level, tick.state_after)) {
        apply_win(tick.state_after, tick.events);
        return;
    }

    std::vector<GameplayEvent> terminal_events;
    for (const GameplayEvent& event : tick.events) {
        if (std::holds_alternative<PlayerCrushedEvent>(event)
            || std::holds_alternative<LevelLostEvent>(event)) {
            terminal_events.push_back(event);
        }
    }
    tick.events.erase(std::remove_if(tick.events.begin(), tick.events.end(),
                                     [](const GameplayEvent& event) {
                                         return std::holds_alternative<PlayerCrushedEvent>(event)
                                             || std::holds_alternative<LevelLostEvent>(event);
                                     }),
                      tick.events.end());
    derive_fixture_state(level, tick.state_after, tick.events);
    tick.events.insert(tick.events.end(), terminal_events.begin(), terminal_events.end());
}

} // namespace game_rules::detail
