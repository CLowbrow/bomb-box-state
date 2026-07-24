#include "bomb_box/engine.hpp"

#include <utility>

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

std::string_view to_string(const RewindStatus status) noexcept
{
    switch (status) {
    case RewindStatus::rewound:
        return "rewound";
    case RewindStatus::history_empty:
        return "history_empty";
    }
    return "unknown";
}

bool detail::ResolvedStateHistory::has_current() const noexcept
{
    return current_.has_value();
}

const std::optional<ResolvedState>& detail::ResolvedStateHistory::current() const noexcept
{
    return current_;
}

std::size_t detail::ResolvedStateHistory::earlier_count() const noexcept
{
    return earlier_.size();
}

void detail::ResolvedStateHistory::reset(ResolvedState initial_state)
{
    current_ = std::move(initial_state);
    earlier_.clear();
}

bool detail::ResolvedStateHistory::commit(ResolvedState next_state)
{
    if (!current_.has_value()) {
        return false;
    }

    earlier_.push_back(*current_);
    current_ = std::move(next_state);
    return true;
}

bool detail::ResolvedStateHistory::rewind()
{
    if (earlier_.empty()) {
        return false;
    }

    current_ = std::move(earlier_.back());
    earlier_.pop_back();
    return true;
}

bool Engine::has_level() const noexcept
{
    return level_.has_value();
}

std::optional<LevelDefinition> Engine::loaded_level() const
{
    return level_;
}

std::optional<ResolvedState> Engine::resolved_state() const
{
    return history_.current();
}

LoadResult Engine::load_level(const LevelDefinition& level)
{
    ValidationResult validation = validate_level(level);
    if (!validation.valid()) {
        return LoadResult{LoadStatus::invalid_level, std::move(validation.errors)};
    }

    LevelDefinition replacement = canonicalize_level(level);
    detail::ResolvedStateHistory replacement_history;
    replacement_history.reset(ResolvedState{replacement.entities, Outcome::ongoing});

    level_ = std::move(replacement);
    history_ = std::move(replacement_history);
    return LoadResult{LoadStatus::loaded, {}};
}

RewindResult Engine::rewind()
{
    const bool accepted = history_.rewind();
    return RewindResult{
        accepted ? RewindStatus::rewound : RewindStatus::history_empty,
        history_.current(),
    };
}

} // namespace bomb_box
