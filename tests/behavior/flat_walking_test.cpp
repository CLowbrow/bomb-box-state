#include "bomb_box/engine.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace {

using namespace bomb_box;

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] LevelDefinition grid_level(const Coordinate player_coordinate = {1, 1})
{
    LevelDefinition level;
    level.width = 3;
    level.height = 3;
    for (std::int32_t y = 0; y < 3; ++y) {
        for (std::int32_t x = 0; x < 3; ++x) {
            level.cells.push_back(Cell{Coordinate{x, y}, FlatCell{0}});
        }
    }
    level.entities = {
        Entity{17, EntityKind::player, player_coordinate, Height::from_elevation(0)},
    };
    return level;
}

[[nodiscard]] ResolvedState player_state(const Coordinate coordinate,
                                         const Height bottom = Height::from_elevation(0))
{
    return ResolvedState{
        {Entity{17, EntityKind::player, coordinate, bottom}},
        Outcome::ongoing,
    };
}

[[nodiscard]] MoveResult expected_move(const Direction direction,
                                       const Coordinate from,
                                       const Coordinate to,
                                       const Height bottom = Height::from_elevation(0))
{
    const ResolvedState initial = player_state(from, bottom);
    const ResolvedState final = player_state(to, bottom);
    return MoveResult{
        MoveStatus::moved,
        direction,
        {},
        initial,
        {TickResult{
            0,
            {GameplayEvent{EntityMovedEvent{
                17, from, to, bottom, bottom, MovementCause::player,
            }}},
            final,
        }},
        final,
        Outcome::ongoing,
    };
}

[[nodiscard]] MoveResult expected_rejection(const MoveStatus reason,
                                            const Direction direction,
                                            const ResolvedState& state)
{
    return MoveResult{
        reason,
        direction,
        {GameplayEvent{MoveBlockedEvent{direction, reason}}},
        std::nullopt,
        {},
        state,
        state.outcome,
    };
}

void walks_in_all_cardinal_directions()
{
    struct Scenario final {
        Direction direction;
        Coordinate destination;
    };
    const std::vector<Scenario> scenarios{
        {Direction::north, {1, 2}},
        {Direction::east, {2, 1}},
        {Direction::south, {1, 0}},
        {Direction::west, {0, 1}},
    };

    for (const Scenario& scenario : scenarios) {
        Engine engine;
        expect(engine.load_level(grid_level()).accepted(), "cardinal scenario level loads");
        const MoveResult actual = engine.move(scenario.direction);
        expect(actual == expected_move(scenario.direction, {1, 1}, scenario.destination),
               "a cardinal walk returns the complete expected turn result");
        expect(engine.resolved_state() == actual.final_state,
               "the returned final state is the authoritative current state");
    }
}

void honors_declared_axis_directions()
{
    LevelDefinition level = grid_level();
    level.coordinates.positive_x = HorizontalAxisDirection::west;
    level.coordinates.positive_y = VerticalAxisDirection::south;

    Engine east_engine;
    expect(east_engine.load_level(level).accepted(), "reversed-axis east level loads");
    expect(east_engine.move(Direction::east)
               == expected_move(Direction::east, {1, 1}, {0, 1}),
           "east follows the declared negative x direction");

    Engine north_engine;
    expect(north_engine.load_level(level).accepted(), "reversed-axis north level loads");
    expect(north_engine.move(Direction::north)
               == expected_move(Direction::north, {1, 1}, {1, 0}),
           "north follows the declared negative y direction");
}

void rejects_without_mutating_state_or_history()
{
    Engine boundary_engine;
    const LevelDefinition boundary_level = grid_level({0, 0});
    expect(boundary_engine.load_level(boundary_level).accepted(), "boundary level loads");
    const ResolvedState boundary_state = player_state({0, 0});
    expect(boundary_engine.move(Direction::west)
               == expected_rejection(MoveStatus::world_boundary, Direction::west, boundary_state),
           "the world edge returns a complete blocked result");
    expect(boundary_engine.rewind().status == RewindStatus::history_empty,
           "a blocked move does not create history");

    LevelDefinition high_ledge = grid_level();
    high_ledge.cells[5].geometry = FlatCell{1};
    Engine high_engine;
    expect(high_engine.load_level(high_ledge).accepted(), "high-ledge level loads");
    expect(high_engine.move(Direction::east)
               == expected_rejection(MoveStatus::ledge, Direction::east, player_state({1, 1})),
           "a player cannot climb a flat-cell ledge");

    LevelDefinition low_ledge = grid_level();
    low_ledge.cells[4].geometry = FlatCell{1};
    low_ledge.entities.front().bottom = Height::from_elevation(1);
    Engine low_engine;
    expect(low_engine.load_level(low_ledge).accepted(), "low-ledge level loads");
    expect(low_engine.move(Direction::east)
               == expected_rejection(MoveStatus::ledge,
                                     Direction::east,
                                     player_state({1, 1}, Height::from_elevation(1))),
           "a player cannot voluntarily walk down a flat-cell ledge");

    LevelDefinition occupied = grid_level();
    occupied.entities.push_back(
        Entity{4, EntityKind::box, Coordinate{2, 1}, Height::from_elevation(0)});
    Engine occupied_engine;
    expect(occupied_engine.load_level(occupied).accepted(), "occupied destination level loads");
    const MoveResult occupied_result = occupied_engine.move(Direction::east);
    const ResolvedState occupied_state{canonicalize_level(occupied).entities, Outcome::ongoing};
    expect(occupied_result == expected_rejection(MoveStatus::occupied,
                                                 Direction::east,
                                                 occupied_state),
           "an entity at the player's height returns a complete occupied rejection");
    expect(occupied_engine.resolved_state() == std::optional{occupied_state},
           "an occupied rejection preserves the complete state");
}

void walks_onto_compatible_stack_support()
{
    LevelDefinition level;
    level.width = 2;
    level.height = 1;
    level.cells = {
        Cell{{0, 0}, FlatCell{1}},
        Cell{{1, 0}, FlatCell{0}},
    };
    level.entities = {
        Entity{17, EntityKind::player, {0, 0}, Height::from_elevation(1)},
        Entity{4, EntityKind::box, {1, 0}, Height::from_elevation(0)},
    };

    Engine engine;
    expect(engine.load_level(level).accepted(), "compatible stack-support level loads");
    const MoveResult result = engine.move(Direction::east);
    expect(result.accepted(), "the player may walk onto a box whose top matches its support height");
    expect(result.final_state.has_value() && result.final_state->entities.size() == 2,
           "stack walking returns every entity in the authoritative state");
    if (result.final_state.has_value() && result.final_state->entities.size() == 2) {
        expect(result.final_state->entities[1].coordinate == Coordinate{1, 0},
               "the player moves onto the destination stack");
        expect(result.final_state->entities[1].bottom == Height::from_elevation(1),
               "walking onto a stack preserves player height");
    }
}

void validates_input_and_reports_unsupported_phase_boundaries()
{
    Engine empty;
    const MoveResult no_level = empty.move(Direction::north);
    expect(no_level.status == MoveStatus::no_level && !no_level.final_state.has_value()
               && no_level.ticks.empty(),
           "movement before load is rejected without inventing a state or tick");

    Engine engine;
    expect(engine.load_level(grid_level()).accepted(), "invalid-direction level loads");
    const ResolvedState unchanged = player_state({1, 1});
    const Direction malformed = static_cast<Direction>(255);
    const MoveResult invalid = engine.move(malformed);
    expect(invalid.status == MoveStatus::invalid_direction && invalid.events.empty()
               && invalid.final_state == std::optional{unchanged},
           "a malformed direction is an API validation error without MoveBlocked");
    expect(engine.rewind().status == RewindStatus::history_empty,
           "an invalid direction does not create history");

    LevelDefinition ramp = grid_level();
    ramp.width = 3;
    ramp.height = 1;
    ramp.cells = {
        Cell{{0, 0}, FlatCell{0}},
        Cell{{1, 0}, RampCell{Direction::west, 0}},
        Cell{{2, 0}, FlatCell{1}},
    };
    ramp.entities = {
        Entity{17, EntityKind::player, {0, 0}, Height::from_elevation(0)},
    };
    Engine ramp_engine;
    expect(ramp_engine.load_level(ramp).accepted(), "valid ramp boundary level loads");
    expect(ramp_engine.move(Direction::east).status == MoveStatus::unsupported_geometry,
           "ramp traversal is explicitly deferred instead of partially resolved");

    LevelDefinition fixture = grid_level();
    fixture.fixtures = {
        Fixture{{2, 1}, Switch{SwitchColor::red}},
    };
    Engine fixture_engine;
    expect(fixture_engine.load_level(fixture).accepted(), "fixture boundary level loads");
    expect(fixture_engine.move(Direction::east).status == MoveStatus::unsupported_fixture,
           "fixture interactions are explicitly deferred instead of partially resolved");

    expect(to_string(MoveStatus::world_boundary) == "world_boundary"
               && to_string(MovementCause::player) == "player",
           "movement results expose stable status and cause strings");
}

void preserves_rewind_and_deterministic_branching()
{
    LevelDefinition line;
    line.width = 3;
    line.height = 1;
    line.cells = {
        Cell{{0, 0}, FlatCell{0}},
        Cell{{1, 0}, FlatCell{0}},
        Cell{{2, 0}, FlatCell{0}},
    };
    line.entities = {
        Entity{17, EntityKind::player, {0, 0}, Height::from_elevation(0)},
    };

    Engine first;
    Engine second;
    expect(first.load_level(line).accepted() && second.load_level(line).accepted(),
           "determinism comparison levels load");
    const MoveResult first_result = first.move(Direction::east);
    const MoveResult second_result = second.move(Direction::east);
    expect(first_result == second_result, "identical state and command produce identical complete output");

    expect(first.move(Direction::east).accepted(), "a second accepted move advances the line");
    const RewindResult first_rewind = first.rewind();
    expect(first_rewind.state == std::optional{player_state({1, 0})},
           "rewind restores the state before the second accepted move");
    expect(first_rewind.events == std::vector<GameplayEvent>{StateRewoundEvent{}}
               && first_rewind.outcome == std::optional{Outcome::ongoing},
           "accepted rewind returns its presentation event and authoritative outcome");
    expect(first.move(Direction::west).accepted(), "a move after rewind starts a new branch");
    expect(first.rewind().state == std::optional{player_state({1, 0})},
           "rewinding the branch restores its actual predecessor");
    expect(first.rewind().state == std::optional{player_state({0, 0})},
           "repeated rewind reaches the initialized state");
    expect(first.rewind().status == RewindStatus::history_empty,
           "the abandoned later state is not retained as redo history");
}

} // namespace

int main()
{
    walks_in_all_cardinal_directions();
    honors_declared_axis_directions();
    rejects_without_mutating_state_or_history();
    walks_onto_compatible_stack_support();
    validates_input_and_reports_unsupported_phase_boundaries();
    preserves_rewind_and_deterministic_branching();

    if (failures == 0) {
        std::cout << "All flat-walking behavior checks passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
