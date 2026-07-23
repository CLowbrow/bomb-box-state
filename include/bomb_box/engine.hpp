#pragma once

#include "bomb_box/world.hpp"

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

class Engine final {
  public:
    Engine() noexcept = default;

    [[nodiscard]] EngineStatus status() const noexcept;
    [[nodiscard]] bool has_level() const noexcept;
    [[nodiscard]] std::optional<LevelDefinition> loaded_level() const;

    // Validation happens before replacement. A rejected load leaves the
    // previously loaded level unchanged.
    [[nodiscard]] LoadResult load_level(const LevelDefinition& level);

  private:
    std::optional<LevelDefinition> level_{};
};

} // namespace bomb_box
