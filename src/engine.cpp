#include "bomb_box/engine.hpp"

namespace bomb_box {

std::string_view to_string(const EngineStatus status) noexcept
{
    switch (status) {
    case EngineStatus::schema_ready:
        return "schema_ready";
    }

    return "unknown";
}

EngineStatus Engine::status() const noexcept
{
    return EngineStatus::schema_ready;
}

std::string_view to_string(const LoadStatus status) noexcept
{
    switch (status) {
    case LoadStatus::loaded:
        return "loaded";
    case LoadStatus::invalid_level:
        return "invalid_level";
    }
    return "unknown";
}

bool Engine::has_level() const noexcept
{
    return level_.has_value();
}

std::optional<LevelDefinition> Engine::loaded_level() const
{
    return level_;
}

LoadResult Engine::load_level(const LevelDefinition& level)
{
    ValidationResult validation = validate_level(level);
    if (!validation.valid()) {
        return LoadResult{LoadStatus::invalid_level, std::move(validation.errors)};
    }

    LevelDefinition replacement = canonicalize_level(level);
    level_ = std::move(replacement);
    return LoadResult{LoadStatus::loaded, {}};
}

} // namespace bomb_box
