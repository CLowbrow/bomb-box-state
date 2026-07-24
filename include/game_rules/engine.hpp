#pragma once

#include "game_rules/world.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace game_rules {

inline constexpr std::uint32_t api_version = 1;

enum class EngineStatus : std::uint8_t {
    schema_ready,
};

[[nodiscard]] std::string_view to_string(EngineStatus status) noexcept;

enum class LoadStatus : std::uint8_t {
    loaded,
    invalid_level,
};

[[nodiscard]] std::string_view to_string(LoadStatus status) noexcept;

struct LoadResult final {
    LoadStatus status{LoadStatus::invalid_level};
    std::vector<ValidationError> errors{};

    [[nodiscard]] bool accepted() const noexcept { return status == LoadStatus::loaded; }
};

// The authoritative dynamic state at a gameplay command boundary. Static cell
// geometry and fixtures remain in the loaded LevelDefinition.
struct ResolvedState final {
    std::vector<Entity> entities{};
    Outcome outcome{Outcome::ongoing};

    [[nodiscard]] friend bool operator==(const ResolvedState&, const ResolvedState&) = default;
};

enum class MoveStatus : std::uint8_t {
    moved,
    no_level,
    invalid_direction,
    world_boundary,
    ledge,
    occupied,
    stacked_push_target,
    unsupported_gravity,
    unsupported_geometry,
    unsupported_fixture,
    level_terminal,
};

[[nodiscard]] std::string_view to_string(MoveStatus status) noexcept;

enum class MovementCause : std::uint8_t {
    player,
    blast,
    fall,
    slide,
};

[[nodiscard]] std::string_view to_string(MovementCause cause) noexcept;

struct MoveBlockedEvent final {
    Direction direction{Direction::north};
    MoveStatus reason{MoveStatus::no_level};

    [[nodiscard]] friend constexpr bool operator==(MoveBlockedEvent, MoveBlockedEvent) noexcept = default;
};

struct StateRewoundEvent final {
    [[nodiscard]] friend constexpr bool operator==(StateRewoundEvent, StateRewoundEvent) noexcept = default;
};

struct EntityMovedEvent final {
    EntityId entity_id{};
    Coordinate from{};
    Coordinate to{};
    Height old_bottom{};
    Height new_bottom{};
    MovementCause cause{MovementCause::player};

    [[nodiscard]] friend constexpr bool operator==(const EntityMovedEvent&,
                                                   const EntityMovedEvent&) noexcept = default;
};

using GameplayEvent = std::variant<MoveBlockedEvent, StateRewoundEvent, EntityMovedEvent>;

struct TickResult final {
    std::uint32_t index{};
    std::vector<GameplayEvent> events{};
    ResolvedState state_after{};

    [[nodiscard]] friend bool operator==(const TickResult&, const TickResult&) = default;
};

struct MoveResult final {
    MoveStatus status{MoveStatus::no_level};
    Direction direction{Direction::north};
    // Presentation-only events for a rejected command. Accepted movement
    // events belong to their world tick instead.
    std::vector<GameplayEvent> events{};
    std::optional<ResolvedState> initial_state{};
    std::vector<TickResult> ticks{};
    std::optional<ResolvedState> final_state{};
    std::optional<Outcome> outcome{};

    [[nodiscard]] bool accepted() const noexcept { return status == MoveStatus::moved; }
    [[nodiscard]] friend bool operator==(const MoveResult&, const MoveResult&) = default;
};

enum class RewindStatus : std::uint8_t {
    rewound,
    history_empty,
};

[[nodiscard]] std::string_view to_string(RewindStatus status) noexcept;

struct RewindResult final {
    RewindStatus status{RewindStatus::history_empty};
    std::optional<ResolvedState> state{};
    std::vector<GameplayEvent> events{};
    std::optional<Outcome> outcome{};

    [[nodiscard]] bool accepted() const noexcept { return status == RewindStatus::rewound; }
    [[nodiscard]] friend bool operator==(const RewindResult&, const RewindResult&) = default;
};

namespace detail {

// Internal transition storage shared by level loading, rewind, and future turn
// orchestration. It is visible here only because Engine owns it by value; it is
// not a supported caller-facing API.
class ResolvedStateHistory final {
  public:
    [[nodiscard]] bool has_current() const noexcept;
    [[nodiscard]] const std::optional<ResolvedState>& current() const noexcept;
    [[nodiscard]] std::size_t earlier_count() const noexcept;

    void reset(ResolvedState initial_state);
    [[nodiscard]] bool commit(ResolvedState next_state);
    [[nodiscard]] bool rewind();

  private:
    std::optional<ResolvedState> current_{};
    std::vector<ResolvedState> earlier_{};
};

} // namespace detail

class Engine final {
  public:
    Engine() noexcept = default;

    [[nodiscard]] EngineStatus status() const noexcept;
    [[nodiscard]] bool has_level() const noexcept;
    [[nodiscard]] std::optional<LevelDefinition> loaded_level() const;
    [[nodiscard]] std::optional<ResolvedState> resolved_state() const;

    // Validation happens before replacement. A rejected load leaves the
    // previously loaded level and its resolved-state history unchanged.
    [[nodiscard]] LoadResult load_level(const LevelDefinition& level);
    [[nodiscard]] MoveResult move(Direction direction);
    [[nodiscard]] RewindResult rewind();

  private:
    std::optional<LevelDefinition> level_{};
    detail::ResolvedStateHistory history_{};
};

} // namespace game_rules
