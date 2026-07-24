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

// The authoritative dynamic state at a gameplay command boundary. Static cell
// geometry and fixtures remain in the loaded LevelDefinition.
struct ResolvedState final {
    std::vector<Entity> entities{};
    Outcome outcome{Outcome::ongoing};
    // Canonical ascending IDs. Arming is dynamic state rather than authored
    // level data, and therefore survives rewind but not level replacement.
    std::vector<EntityId> armed_barrels{};
    // Active colors follow SwitchColor declaration order. Effectively open
    // door coordinates are canonical row-major. Both are derived state and
    // are restored exactly by rewind.
    std::vector<SwitchColor> active_switch_colors{};
    std::vector<Coordinate> open_doors{};

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
    closed_door,
    teleporter_restriction,
    unsupported_geometry,
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

struct BarrelArmedEvent final {
    EntityId entity_id{};

    [[nodiscard]] friend constexpr bool operator==(BarrelArmedEvent,
                                                   BarrelArmedEvent) noexcept = default;
};

struct PlayerCrushedEvent final {
    EntityId player_id{};
    EntityId crushing_entity_id{};

    [[nodiscard]] friend constexpr bool operator==(PlayerCrushedEvent,
                                                   PlayerCrushedEvent) noexcept = default;
};

struct SwitchChangedEvent final {
    SwitchColor color{SwitchColor::red};
    bool active{};

    [[nodiscard]] friend constexpr bool operator==(SwitchChangedEvent,
                                                   SwitchChangedEvent) noexcept = default;
};

struct DoorOpenedEvent final {
    Coordinate coordinate{};
    SwitchColor color{SwitchColor::red};

    [[nodiscard]] friend constexpr bool operator==(DoorOpenedEvent,
                                                   DoorOpenedEvent) noexcept = default;
};

struct DoorClosedEvent final {
    Coordinate coordinate{};
    SwitchColor color{SwitchColor::red};

    [[nodiscard]] friend constexpr bool operator==(DoorClosedEvent,
                                                   DoorClosedEvent) noexcept = default;
};

struct LevelWonEvent final {
    [[nodiscard]] friend constexpr bool operator==(LevelWonEvent, LevelWonEvent) noexcept = default;
};

struct LevelLostEvent final {
    [[nodiscard]] friend constexpr bool operator==(LevelLostEvent, LevelLostEvent) noexcept = default;
};

using GameplayEvent = std::variant<MoveBlockedEvent,
                                   StateRewoundEvent,
                                   EntityMovedEvent,
                                   BarrelArmedEvent,
                                   PlayerCrushedEvent,
                                   SwitchChangedEvent,
                                   DoorOpenedEvent,
                                   DoorClosedEvent,
                                   LevelWonEvent,
                                   LevelLostEvent>;

struct TickResult final {
    std::uint32_t index{};
    std::vector<GameplayEvent> events{};
    ResolvedState state_after{};

    [[nodiscard]] friend bool operator==(const TickResult&, const TickResult&) = default;
};

struct LoadResult final {
    LoadStatus status{LoadStatus::invalid_level};
    std::vector<ValidationError> errors{};
    std::optional<ResolvedState> initial_state{};
    std::vector<TickResult> ticks{};
    std::optional<ResolvedState> final_state{};
    std::optional<Outcome> outcome{};

    [[nodiscard]] bool accepted() const noexcept { return status == LoadStatus::loaded; }
    [[nodiscard]] friend bool operator==(const LoadResult&, const LoadResult&) = default;
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

    // Validation and initialization happen before replacement. A successful
    // result includes every initialization tick; a rejected load leaves the
    // previously loaded level and its resolved-state history unchanged.
    [[nodiscard]] LoadResult load_level(const LevelDefinition& level);
    [[nodiscard]] MoveResult move(Direction direction);
    [[nodiscard]] RewindResult rewind();

  private:
    std::optional<LevelDefinition> level_{};
    detail::ResolvedStateHistory history_{};
};

} // namespace game_rules
