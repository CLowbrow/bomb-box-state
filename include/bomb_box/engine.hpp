#pragma once

#include "bomb_box/world.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace bomb_box {

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

enum class RewindStatus : std::uint8_t {
    rewound,
    history_empty,
};

[[nodiscard]] std::string_view to_string(RewindStatus status) noexcept;

struct RewindResult final {
    RewindStatus status{RewindStatus::history_empty};
    std::optional<ResolvedState> state{};

    [[nodiscard]] bool accepted() const noexcept { return status == RewindStatus::rewound; }
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
    [[nodiscard]] RewindResult rewind();

  private:
    std::optional<LevelDefinition> level_{};
    detail::ResolvedStateHistory history_{};
};

} // namespace bomb_box
