#pragma once

#include "bomb_box/engine.hpp"
#include "bomb_box/world.hpp"

#include <gtest/gtest-printers.h>

#include <ostream>
#include <string_view>

namespace bomb_box {

namespace test_detail {

template <typename T>
void print_field(std::ostream* const output, const std::string_view name, const T& value)
{
    *output << name << '=' << ::testing::PrintToString(value);
}

} // namespace test_detail

inline void PrintTo(const Coordinate value, std::ostream* const output)
{
    *output << '(' << value.x << ", " << value.y << ')';
}

inline void PrintTo(const Height value, std::ostream* const output)
{
    *output << value.half_steps << " half-steps";
}

inline void PrintTo(const Direction value, std::ostream* const output)
{
    switch (value) {
    case Direction::north:
        *output << "north";
        return;
    case Direction::east:
        *output << "east";
        return;
    case Direction::south:
        *output << "south";
        return;
    case Direction::west:
        *output << "west";
        return;
    }
    *output << "Direction(" << static_cast<unsigned int>(value) << ')';
}

inline void PrintTo(const Outcome value, std::ostream* const output)
{
    switch (value) {
    case Outcome::ongoing:
        *output << "ongoing";
        return;
    case Outcome::won:
        *output << "won";
        return;
    case Outcome::lost:
        *output << "lost";
        return;
    }
    *output << "Outcome(" << static_cast<unsigned int>(value) << ')';
}

inline void PrintTo(const HorizontalAxisDirection value, std::ostream* const output)
{
    switch (value) {
    case HorizontalAxisDirection::east:
        *output << "east";
        return;
    case HorizontalAxisDirection::west:
        *output << "west";
        return;
    }
    *output << "HorizontalAxisDirection(" << static_cast<unsigned int>(value) << ')';
}

inline void PrintTo(const VerticalAxisDirection value, std::ostream* const output)
{
    switch (value) {
    case VerticalAxisDirection::north:
        *output << "north";
        return;
    case VerticalAxisDirection::south:
        *output << "south";
        return;
    }
    *output << "VerticalAxisDirection(" << static_cast<unsigned int>(value) << ')';
}

inline void PrintTo(const CoordinateSystem& value, std::ostream* const output)
{
    *output << "CoordinateSystem{";
    test_detail::print_field(output, "origin", value.origin);
    *output << ", ";
    test_detail::print_field(output, "positive_x", value.positive_x);
    *output << ", ";
    test_detail::print_field(output, "positive_y", value.positive_y);
    *output << '}';
}

inline void PrintTo(const FlatCell value, std::ostream* const output)
{
    *output << "FlatCell{elevation=" << value.elevation << '}';
}

inline void PrintTo(const RampCell& value, std::ostream* const output)
{
    *output << "RampCell{";
    test_detail::print_field(output, "low_direction", value.low_direction);
    *output << ", low_elevation=" << value.low_elevation << '}';
}

inline void PrintTo(const Cell& value, std::ostream* const output)
{
    *output << "Cell{";
    test_detail::print_field(output, "coordinate", value.coordinate);
    *output << ", ";
    test_detail::print_field(output, "geometry", value.geometry);
    *output << '}';
}

inline void PrintTo(const SwitchColor value, std::ostream* const output)
{
    switch (value) {
    case SwitchColor::red:
        *output << "red";
        return;
    case SwitchColor::green:
        *output << "green";
        return;
    case SwitchColor::blue:
        *output << "blue";
        return;
    case SwitchColor::yellow:
        *output << "yellow";
        return;
    }
    *output << "SwitchColor(" << static_cast<unsigned int>(value) << ')';
}

inline void PrintTo(const Switch value, std::ostream* const output)
{
    *output << "Switch{";
    test_detail::print_field(output, "color", value.color);
    *output << '}';
}

inline void PrintTo(const Door value, std::ostream* const output)
{
    *output << "Door{";
    test_detail::print_field(output, "color", value.color);
    *output << '}';
}

inline void PrintTo(const ExitTeleporter, std::ostream* const output)
{
    *output << "ExitTeleporter{}";
}

inline void PrintTo(const Fixture& value, std::ostream* const output)
{
    *output << "Fixture{";
    test_detail::print_field(output, "coordinate", value.coordinate);
    *output << ", ";
    test_detail::print_field(output, "kind", value.kind);
    *output << '}';
}

inline void PrintTo(const EntityKind value, std::ostream* const output)
{
    switch (value) {
    case EntityKind::player:
        *output << "player";
        return;
    case EntityKind::box:
        *output << "box";
        return;
    case EntityKind::barrel:
        *output << "barrel";
        return;
    }
    *output << "EntityKind(" << static_cast<unsigned int>(value) << ')';
}

inline void PrintTo(const Entity& value, std::ostream* const output)
{
    *output << "Entity{";
    test_detail::print_field(output, "id", value.id);
    *output << ", ";
    test_detail::print_field(output, "kind", value.kind);
    *output << ", ";
    test_detail::print_field(output, "coordinate", value.coordinate);
    *output << ", ";
    test_detail::print_field(output, "bottom", value.bottom);
    *output << '}';
}

inline void PrintTo(const ResolvedState& value, std::ostream* const output)
{
    *output << "ResolvedState{";
    test_detail::print_field(output, "entities", value.entities);
    *output << ", ";
    test_detail::print_field(output, "outcome", value.outcome);
    *output << '}';
}

inline void PrintTo(const MoveStatus value, std::ostream* const output)
{
    *output << to_string(value);
}

inline void PrintTo(const MovementCause value, std::ostream* const output)
{
    *output << to_string(value);
}

inline void PrintTo(const MoveBlockedEvent& value, std::ostream* const output)
{
    *output << "MoveBlocked{";
    test_detail::print_field(output, "direction", value.direction);
    *output << ", ";
    test_detail::print_field(output, "reason", value.reason);
    *output << '}';
}

inline void PrintTo(const StateRewoundEvent&, std::ostream* const output)
{
    *output << "StateRewound{}";
}

inline void PrintTo(const EntityMovedEvent& value, std::ostream* const output)
{
    *output << "EntityMoved{";
    test_detail::print_field(output, "entity_id", value.entity_id);
    *output << ", ";
    test_detail::print_field(output, "from", value.from);
    *output << ", ";
    test_detail::print_field(output, "to", value.to);
    *output << ", ";
    test_detail::print_field(output, "old_bottom", value.old_bottom);
    *output << ", ";
    test_detail::print_field(output, "new_bottom", value.new_bottom);
    *output << ", ";
    test_detail::print_field(output, "cause", value.cause);
    *output << '}';
}

inline void PrintTo(const TickResult& value, std::ostream* const output)
{
    *output << "TickResult{";
    test_detail::print_field(output, "index", value.index);
    *output << ", ";
    test_detail::print_field(output, "events", value.events);
    *output << ", ";
    test_detail::print_field(output, "state_after", value.state_after);
    *output << '}';
}

inline void PrintTo(const MoveResult& value, std::ostream* const output)
{
    *output << "MoveResult{";
    test_detail::print_field(output, "status", value.status);
    *output << ", ";
    test_detail::print_field(output, "direction", value.direction);
    *output << ", ";
    test_detail::print_field(output, "events", value.events);
    *output << ", ";
    test_detail::print_field(output, "initial_state", value.initial_state);
    *output << ", ";
    test_detail::print_field(output, "ticks", value.ticks);
    *output << ", ";
    test_detail::print_field(output, "final_state", value.final_state);
    *output << ", ";
    test_detail::print_field(output, "outcome", value.outcome);
    *output << '}';
}

inline void PrintTo(const RewindStatus value, std::ostream* const output)
{
    *output << to_string(value);
}

inline void PrintTo(const RewindResult& value, std::ostream* const output)
{
    *output << "RewindResult{";
    test_detail::print_field(output, "status", value.status);
    *output << ", ";
    test_detail::print_field(output, "state", value.state);
    *output << ", ";
    test_detail::print_field(output, "events", value.events);
    *output << ", ";
    test_detail::print_field(output, "outcome", value.outcome);
    *output << '}';
}

inline void PrintTo(const ValidationErrorCode value, std::ostream* const output)
{
    *output << to_string(value);
}

inline void PrintTo(const ValidationError& value, std::ostream* const output)
{
    *output << "ValidationError{";
    test_detail::print_field(output, "code", value.code);
    *output << ", ";
    test_detail::print_field(output, "coordinate", value.coordinate);
    *output << ", ";
    test_detail::print_field(output, "entity_id", value.entity_id);
    *output << '}';
}

inline void PrintTo(const LevelDefinition& value, std::ostream* const output)
{
    *output << "LevelDefinition{";
    test_detail::print_field(output, "coordinates", value.coordinates);
    *output << ", ";
    test_detail::print_field(output, "width", value.width);
    *output << ", ";
    test_detail::print_field(output, "height", value.height);
    *output << ", ";
    test_detail::print_field(output, "cells", value.cells);
    *output << ", ";
    test_detail::print_field(output, "fixtures", value.fixtures);
    *output << ", ";
    test_detail::print_field(output, "entities", value.entities);
    *output << '}';
}

} // namespace bomb_box
