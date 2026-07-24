#include "bomb_box/engine.hpp"

#include <iostream>
#include <string_view>

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

[[nodiscard]] LevelDefinition flat_level(const Coordinate origin = {}, const EntityId player_id = 1)
{
    LevelDefinition level;
    level.coordinates.origin = origin;
    level.width = 3;
    level.height = 1;
    level.cells = {
        Cell{origin, FlatCell{0}},
        Cell{Coordinate{origin.x + 1, origin.y}, FlatCell{0}},
        Cell{Coordinate{origin.x + 2, origin.y}, FlatCell{0}},
    };
    level.entities = {
        Entity{player_id, EntityKind::player, origin, Height::from_elevation(0)},
    };
    return level;
}

[[nodiscard]] ResolvedState state_at(const std::int32_t x, const Outcome outcome = Outcome::ongoing)
{
    return ResolvedState{
        {Entity{1, EntityKind::player, Coordinate{x, 0}, Height::from_elevation(0)}},
        outcome,
    };
}

void initialized_engine_boundary()
{
    Engine engine;
    expect(!engine.resolved_state().has_value(), "an engine without a level has no resolved state");

    const RewindResult before_load = engine.rewind();
    expect(!before_load.accepted(), "rewind before a level is loaded is rejected");
    expect(before_load.status == RewindStatus::history_empty,
           "rewind before load uses the history_empty reason");
    expect(!before_load.state.has_value(), "rewind before load has no state to return");
    expect(before_load.events.empty() && !before_load.outcome.has_value(),
           "rewind before load has no presentation event or invented outcome");

    const LevelDefinition level = flat_level();
    expect(engine.load_level(level).accepted(), "a valid level establishes an initialized state");

    const ResolvedState expected{canonicalize_level(level).entities, Outcome::ongoing};
    expect(engine.resolved_state() == std::optional{expected},
           "the canonical supplied entities form the phase-two initialized state");

    auto caller_state = engine.resolved_state();
    caller_state->entities.front().coordinate.x = 99;
    expect(engine.resolved_state() == std::optional{expected},
           "resolved-state snapshots are caller-owned values");

    const RewindResult empty = engine.rewind();
    expect(!empty.accepted(), "the initialized history boundary cannot rewind");
    expect(empty.status == RewindStatus::history_empty,
           "the initialized boundary reports history_empty");
    expect(empty.state == std::optional{expected}, "a rejected rewind returns the unchanged current state");
    expect(empty.events.empty() && empty.outcome == std::optional{Outcome::ongoing},
           "a rejected rewind returns the current outcome without a success event");
    expect(to_string(empty.status) == "history_empty", "rewind rejection has a stable string code");
    expect(to_string(RewindStatus::rewound) == "rewound", "rewind success has a stable string code");
}

void repeated_rewind_and_branching()
{
    detail::ResolvedStateHistory history;
    const ResolvedState initialized = state_at(0);
    const ResolvedState first = state_at(1);
    const ResolvedState abandoned = state_at(2, Outcome::won);
    const ResolvedState branch = state_at(-1, Outcome::lost);

    expect(!history.commit(first), "history cannot accept a turn before initialization");
    history.reset(initialized);
    expect(history.has_current(), "initialization establishes a current state");
    expect(history.earlier_count() == 0, "initialization creates no earlier rewind boundary");
    expect(history.commit(first), "an accepted turn preserves the initialized state");
    expect(history.commit(abandoned), "a second accepted turn preserves its predecessor");
    expect(history.current() == std::optional{abandoned},
           "a terminal resolved state can be current history");
    expect(history.earlier_count() == 2, "two accepted turns create two earlier states");

    expect(history.rewind(), "the latest resolved state can be rewound");
    expect(history.current() == std::optional{first}, "rewind restores the exact prior state");
    expect(history.earlier_count() == 1, "rewind consumes the restored history entry");

    expect(history.commit(branch), "a turn after rewind begins a new branch");
    expect(history.current() == std::optional{branch}, "the branch becomes current");
    expect(history.earlier_count() == 2, "the branch preserves only its actual predecessor");
    expect(history.rewind(), "the branch can rewind to its predecessor");
    expect(history.current() == std::optional{first},
           "rewinding the branch does not restore abandoned state");
    expect(history.rewind(), "rewind may be repeated to initialization");
    expect(history.current() == std::optional{initialized}, "repeated rewind reaches initialization exactly");
    expect(!history.rewind(), "rewind past initialization is rejected");
    expect(history.current() == std::optional{initialized}, "rejected rewind changes nothing");
}

void replacement_and_rejection_lifetime()
{
    detail::ResolvedStateHistory history;
    history.reset(state_at(0));
    expect(history.commit(state_at(1)), "the old level has an earlier state");
    history.reset(state_at(10));
    expect(history.current() == std::optional{state_at(10)},
           "replacement installs the new initialized state");
    expect(history.earlier_count() == 0, "replacement discards all old-level history");
    expect(!history.rewind(), "old-level state is unreachable after replacement");

    Engine engine;
    const LevelDefinition first = flat_level();
    expect(engine.load_level(first).accepted(), "the first engine level loads");
    const auto before_level = engine.loaded_level();
    const auto before_state = engine.resolved_state();

    LevelDefinition invalid = flat_level();
    invalid.entities.clear();
    expect(!engine.load_level(invalid).accepted(), "an invalid replacement is rejected");
    expect(engine.loaded_level() == before_level, "rejection preserves the loaded definition");
    expect(engine.resolved_state() == before_state, "rejection preserves the current resolved state");
    expect(engine.rewind().status == RewindStatus::history_empty,
           "rejection preserves the initialized history boundary");

    const LevelDefinition replacement = flat_level(Coordinate{20, -3}, 99);
    expect(engine.load_level(replacement).accepted(), "a valid replacement loads");
    const ResolvedState expected{canonicalize_level(replacement).entities, Outcome::ongoing};
    expect(engine.resolved_state() == std::optional{expected},
           "valid replacement installs only the new level's initialized state");
    expect(engine.rewind().status == RewindStatus::history_empty,
           "valid replacement begins with empty rewind history");
}

} // namespace

int main()
{
    initialized_engine_boundary();
    repeated_rewind_and_branching();
    replacement_and_rejection_lifetime();

    if (failures == 0) {
        std::cout << "All resolved-state history behavior checks passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
