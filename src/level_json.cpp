#include "bomb_box/level_json.hpp"

#include "level_json_yyjson.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bomb_box {
namespace {

[[nodiscard]] std::string_view string_value(yyjson_val* value) noexcept
{
    return std::string_view{yyjson_get_str(value), yyjson_get_len(value)};
}

class YyjsonDocument final {
  public:
    explicit YyjsonDocument(yyjson_doc* document) noexcept : document_(document) {}
    ~YyjsonDocument() { bomb_box_internal_yyjson_free(document_); }

    YyjsonDocument(const YyjsonDocument&) = delete;
    YyjsonDocument& operator=(const YyjsonDocument&) = delete;

    [[nodiscard]] yyjson_doc* get() const noexcept { return document_; }

  private:
    yyjson_doc* document_{};
};

[[nodiscard]] bool exceeds_nesting_depth(yyjson_val* value,
                                         const std::uint32_t depth,
                                         const std::uint32_t maximum) noexcept
{
    if (!yyjson_is_arr(value) && !yyjson_is_obj(value)) {
        return false;
    }
    if (depth >= maximum) {
        return true;
    }
    if (yyjson_is_arr(value)) {
        yyjson_arr_iter iterator = yyjson_arr_iter_with(value);
        while (yyjson_val* child = yyjson_arr_iter_next(&iterator)) {
            if (exceeds_nesting_depth(child, depth + 1, maximum)) {
                return true;
            }
        }
        return false;
    }

    yyjson_obj_iter iterator = yyjson_obj_iter_with(value);
    while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
        if (exceeds_nesting_depth(yyjson_obj_iter_get_val(key), depth + 1, maximum)) {
            return true;
        }
    }
    return false;
}

class LevelDecoder final {
  public:
    [[nodiscard]] std::optional<LevelDefinition> decode(yyjson_val* root)
    {
        if (!yyjson_is_obj(root)) {
            fail(LevelJsonErrorCode::root_not_object, {});
            return std::nullopt;
        }
        if (!members_are(root, {"format", "version", "coordinateSystem", "width", "height",
                                "cells", "fixtures", "entities"}, {})) {
            return std::nullopt;
        }

        yyjson_val* format = required(root, "format", {});
        yyjson_val* version = required(root, "version", {});
        yyjson_val* coordinates = required(root, "coordinateSystem", {});
        yyjson_val* width = required(root, "width", {});
        yyjson_val* height = required(root, "height", {});
        yyjson_val* cells = required(root, "cells", {});
        yyjson_val* fixtures = required(root, "fixtures", {});
        yyjson_val* entities = required(root, "entities", {});
        if (error_.has_value()) {
            return std::nullopt;
        }

        if (!yyjson_is_str(format)) {
            fail(LevelJsonErrorCode::invalid_member_type, "/format");
        } else if (string_value(format) != "bomb-box-level") {
            fail(LevelJsonErrorCode::invalid_format, "/format");
        }
        std::uint32_t decoded_version = 0;
        read_unsigned(version, decoded_version, "/version");
        if (!error_.has_value() && decoded_version != level_json_format_version) {
            fail(LevelJsonErrorCode::unsupported_version, "/version");
        }

        LevelDefinition level;
        read_coordinate_system(coordinates, level.coordinates);
        read_unsigned(width, level.width, "/width");
        read_unsigned(height, level.height, "/height");
        read_cells(cells, level.cells);
        read_fixtures(fixtures, level.fixtures);
        read_entities(entities, level.entities);
        if (error_.has_value()) {
            return std::nullopt;
        }
        return level;
    }

    [[nodiscard]] const std::optional<LevelJsonError>& error() const noexcept { return error_; }

  private:
    void read_coordinate_system(yyjson_val* value, CoordinateSystem& output)
    {
        constexpr std::string_view path = "/coordinateSystem";
        if (!expect_object(value, path)
            || !members_are(value, {"origin", "positiveX", "positiveY"}, path)) {
            return;
        }
        yyjson_val* origin = required(value, "origin", path);
        yyjson_val* positive_x = required(value, "positiveX", path);
        yyjson_val* positive_y = required(value, "positiveY", path);
        if (error_.has_value()) {
            return;
        }
        read_coordinate(origin, output.origin, "/coordinateSystem/origin");
        read_enum(positive_x, output.positive_x,
                  std::array{std::pair{std::string_view{"east"}, HorizontalAxisDirection::east},
                             std::pair{std::string_view{"west"}, HorizontalAxisDirection::west}},
                  "/coordinateSystem/positiveX");
        read_enum(positive_y, output.positive_y,
                  std::array{std::pair{std::string_view{"north"}, VerticalAxisDirection::north},
                             std::pair{std::string_view{"south"}, VerticalAxisDirection::south}},
                  "/coordinateSystem/positiveY");
    }

    void read_coordinate(yyjson_val* value, Coordinate& output, const std::string_view path)
    {
        if (!expect_object(value, path) || !members_are(value, {"x", "y"}, path)) {
            return;
        }
        yyjson_val* x = required(value, "x", path);
        yyjson_val* y = required(value, "y", path);
        if (error_.has_value()) {
            return;
        }
        read_signed(x, output.x, append(path, "x"));
        read_signed(y, output.y, append(path, "y"));
    }

    void read_cells(yyjson_val* value, std::vector<Cell>& output)
    {
        if (!expect_array(value, "/cells")) {
            return;
        }
        output.reserve(yyjson_arr_size(value));
        yyjson_arr_iter iterator = yyjson_arr_iter_with(value);
        std::size_t index = 0;
        while (yyjson_val* item = yyjson_arr_iter_next(&iterator)) {
            if (error_.has_value()) {
                return;
            }
            const std::string path = indexed("/cells", index++);
            if (!expect_object(item, path)
                || !members_are(item, {"coordinate", "type", "elevation", "lowDirection",
                                       "lowElevation"}, path)) {
                return;
            }
            yyjson_val* coordinate = required(item, "coordinate", path);
            yyjson_val* type = required(item, "type", path);
            if (error_.has_value()) {
                return;
            }
            Cell cell;
            read_coordinate(coordinate, cell.coordinate, append(path, "coordinate"));
            if (!expect_string(type, append(path, "type"))) {
                return;
            }
            if (string_value(type) == "flat") {
                if (!members_are(item, {"coordinate", "type", "elevation"}, path)) {
                    return;
                }
                yyjson_val* elevation = required(item, "elevation", path);
                FlatCell flat;
                if (elevation != nullptr) {
                    read_signed(elevation, flat.elevation, append(path, "elevation"));
                }
                cell.geometry = flat;
            } else if (string_value(type) == "ramp") {
                if (!members_are(item, {"coordinate", "type", "lowDirection", "lowElevation"}, path)) {
                    return;
                }
                yyjson_val* direction = required(item, "lowDirection", path);
                yyjson_val* elevation = required(item, "lowElevation", path);
                RampCell ramp;
                if (direction != nullptr) {
                    read_direction(direction, ramp.low_direction, append(path, "lowDirection"));
                }
                if (elevation != nullptr) {
                    read_signed(elevation, ramp.low_elevation, append(path, "lowElevation"));
                }
                cell.geometry = ramp;
            } else {
                fail(LevelJsonErrorCode::invalid_enum_value, append(path, "type"));
            }
            output.push_back(std::move(cell));
        }
    }

    void read_fixtures(yyjson_val* value, std::vector<Fixture>& output)
    {
        if (!expect_array(value, "/fixtures")) {
            return;
        }
        output.reserve(yyjson_arr_size(value));
        yyjson_arr_iter iterator = yyjson_arr_iter_with(value);
        std::size_t index = 0;
        while (yyjson_val* item = yyjson_arr_iter_next(&iterator)) {
            if (error_.has_value()) {
                return;
            }
            const std::string path = indexed("/fixtures", index++);
            if (!expect_object(item, path)
                || !members_are(item, {"coordinate", "type", "color"}, path)) {
                return;
            }
            yyjson_val* coordinate = required(item, "coordinate", path);
            yyjson_val* type = required(item, "type", path);
            if (error_.has_value()) {
                return;
            }
            Fixture fixture;
            read_coordinate(coordinate, fixture.coordinate, append(path, "coordinate"));
            if (!expect_string(type, append(path, "type"))) {
                return;
            }
            if (string_value(type) == "exit") {
                if (!members_are(item, {"coordinate", "type"}, path)) {
                    return;
                }
                fixture.kind = ExitTeleporter{};
            } else if (string_value(type) == "switch" || string_value(type) == "door") {
                yyjson_val* color = required(item, "color", path);
                SwitchColor decoded_color{};
                if (color != nullptr) {
                    read_color(color, decoded_color, append(path, "color"));
                }
                fixture.kind = string_value(type) == "switch" ? FixtureKind{Switch{decoded_color}}
                                                                : FixtureKind{Door{decoded_color}};
            } else {
                fail(LevelJsonErrorCode::invalid_enum_value, append(path, "type"));
            }
            output.push_back(std::move(fixture));
        }
    }

    void read_entities(yyjson_val* value, std::vector<Entity>& output)
    {
        if (!expect_array(value, "/entities")) {
            return;
        }
        output.reserve(yyjson_arr_size(value));
        yyjson_arr_iter iterator = yyjson_arr_iter_with(value);
        std::size_t index = 0;
        while (yyjson_val* item = yyjson_arr_iter_next(&iterator)) {
            if (error_.has_value()) {
                return;
            }
            const std::string path = indexed("/entities", index++);
            if (!expect_object(item, path)
                || !members_are(item, {"id", "type", "coordinate", "bottomHalfSteps"}, path)) {
                return;
            }
            yyjson_val* id = required(item, "id", path);
            yyjson_val* type = required(item, "type", path);
            yyjson_val* coordinate = required(item, "coordinate", path);
            yyjson_val* bottom = required(item, "bottomHalfSteps", path);
            if (error_.has_value()) {
                return;
            }
            Entity entity;
            read_entity_id(id, entity.id, append(path, "id"));
            read_entity_kind(type, entity.kind, append(path, "type"));
            read_coordinate(coordinate, entity.coordinate, append(path, "coordinate"));
            read_signed(bottom, entity.bottom.half_steps, append(path, "bottomHalfSteps"));
            output.push_back(entity);
        }
    }

    void read_direction(yyjson_val* value, Direction& output, const std::string_view path)
    {
        read_enum(value, output,
                  std::array{std::pair{std::string_view{"north"}, Direction::north},
                             std::pair{std::string_view{"east"}, Direction::east},
                             std::pair{std::string_view{"south"}, Direction::south},
                             std::pair{std::string_view{"west"}, Direction::west}}, path);
    }

    void read_color(yyjson_val* value, SwitchColor& output, const std::string_view path)
    {
        read_enum(value, output,
                  std::array{std::pair{std::string_view{"red"}, SwitchColor::red},
                             std::pair{std::string_view{"green"}, SwitchColor::green},
                             std::pair{std::string_view{"blue"}, SwitchColor::blue},
                             std::pair{std::string_view{"yellow"}, SwitchColor::yellow}}, path);
    }

    void read_entity_kind(yyjson_val* value, EntityKind& output, const std::string_view path)
    {
        read_enum(value, output,
                  std::array{std::pair{std::string_view{"player"}, EntityKind::player},
                             std::pair{std::string_view{"box"}, EntityKind::box},
                             std::pair{std::string_view{"barrel"}, EntityKind::barrel}}, path);
    }

    template <typename Enum, std::size_t Size>
    void read_enum(yyjson_val* value,
                   Enum& output,
                   const std::array<std::pair<std::string_view, Enum>, Size>& values,
                   const std::string_view path)
    {
        if (!expect_string(value, path)) {
            return;
        }
        const std::string_view decoded = string_value(value);
        const auto found = std::find_if(values.begin(), values.end(), [decoded](const auto& candidate) {
            return candidate.first == decoded;
        });
        if (found == values.end()) {
            fail(LevelJsonErrorCode::invalid_enum_value, path);
            return;
        }
        output = found->second;
    }

    void read_entity_id(yyjson_val* value, EntityId& output, const std::string_view path)
    {
        if (!expect_string(value, path)) {
            return;
        }
        const std::string_view decoded = string_value(value);
        if (decoded.empty() || decoded.front() == '0' || decoded.front() == '-') {
            fail(LevelJsonErrorCode::invalid_entity_id, path);
            return;
        }
        const auto converted =
            std::from_chars(decoded.data(), decoded.data() + decoded.size(), output);
        if (converted.ec != std::errc{} || converted.ptr != decoded.data() + decoded.size()) {
            fail(LevelJsonErrorCode::invalid_entity_id, path);
        }
    }

    template <typename Integer>
    void read_signed(yyjson_val* value, Integer& output, const std::string_view path)
    {
        static_assert(std::numeric_limits<Integer>::is_signed);
        if (yyjson_is_sint(value)) {
            const std::int64_t decoded = yyjson_get_sint(value);
            if (decoded < static_cast<std::int64_t>(std::numeric_limits<Integer>::min())
                || decoded > static_cast<std::int64_t>(std::numeric_limits<Integer>::max())) {
                fail(LevelJsonErrorCode::integer_out_of_range, path);
                return;
            }
            output = static_cast<Integer>(decoded);
            return;
        }
        if (yyjson_is_uint(value)) {
            const std::uint64_t decoded = yyjson_get_uint(value);
            if (decoded > static_cast<std::uint64_t>(std::numeric_limits<Integer>::max())) {
                fail(LevelJsonErrorCode::integer_out_of_range, path);
                return;
            }
            output = static_cast<Integer>(decoded);
            return;
        }
        fail(yyjson_is_num(value) ? LevelJsonErrorCode::integer_out_of_range
                                  : LevelJsonErrorCode::invalid_member_type,
             path);
    }

    template <typename Integer>
    void read_unsigned(yyjson_val* value, Integer& output, const std::string_view path)
    {
        static_assert(!std::numeric_limits<Integer>::is_signed);
        if (!yyjson_is_uint(value)) {
            fail(yyjson_is_num(value) ? LevelJsonErrorCode::integer_out_of_range
                                      : LevelJsonErrorCode::invalid_member_type,
                 path);
            return;
        }
        const std::uint64_t decoded = yyjson_get_uint(value);
        if (decoded > static_cast<std::uint64_t>(std::numeric_limits<Integer>::max())) {
            fail(LevelJsonErrorCode::integer_out_of_range, path);
            return;
        }
        output = static_cast<Integer>(decoded);
    }

    [[nodiscard]] bool members_are(yyjson_val* object,
                                   const std::initializer_list<std::string_view> allowed,
                                   const std::string_view path)
    {
        std::vector<std::string_view> seen;
        seen.reserve(yyjson_obj_size(object));
        yyjson_obj_iter iterator = yyjson_obj_iter_with(object);
        while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
            const std::string_view name = string_value(key);
            if (std::find(seen.begin(), seen.end(), name) != seen.end()) {
                fail(LevelJsonErrorCode::duplicate_member, append(path, name));
                return false;
            }
            seen.push_back(name);
            if (std::find(allowed.begin(), allowed.end(), name) == allowed.end()) {
                fail(LevelJsonErrorCode::unknown_member, append(path, name));
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] yyjson_val* required(yyjson_val* object,
                                       const std::string_view name,
                                       const std::string_view path)
    {
        yyjson_obj_iter iterator = yyjson_obj_iter_with(object);
        while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
            if (string_value(key) == name) {
                return yyjson_obj_iter_get_val(key);
            }
        }
        fail(LevelJsonErrorCode::missing_member, append(path, name));
        return nullptr;
    }

    [[nodiscard]] bool expect_object(yyjson_val* value, const std::string_view path)
    {
        if (yyjson_is_obj(value)) {
            return true;
        }
        fail(LevelJsonErrorCode::invalid_member_type, path);
        return false;
    }

    [[nodiscard]] bool expect_array(yyjson_val* value, const std::string_view path)
    {
        if (yyjson_is_arr(value)) {
            return true;
        }
        fail(LevelJsonErrorCode::invalid_member_type, path);
        return false;
    }

    [[nodiscard]] bool expect_string(yyjson_val* value, const std::string_view path)
    {
        if (yyjson_is_str(value)) {
            return true;
        }
        fail(LevelJsonErrorCode::invalid_member_type, path);
        return false;
    }

    void fail(const LevelJsonErrorCode code, const std::string_view path)
    {
        if (!error_.has_value()) {
            // yyjson exposes syntax-error offsets but does not retain source
            // offsets for successfully decoded DOM values. Shape errors use
            // their precise JSON Pointer with byte offset zero.
            error_ = LevelJsonError{code, 0, std::string{path}};
        }
    }

    [[nodiscard]] static std::string append(const std::string_view base,
                                            const std::string_view member)
    {
        std::string result{base};
        result.push_back('/');
        for (const char character : member) {
            if (character == '~') {
                result += "~0";
            } else if (character == '/') {
                result += "~1";
            } else {
                result.push_back(character);
            }
        }
        return result;
    }

    [[nodiscard]] static std::string indexed(const std::string_view base, const std::size_t index)
    {
        return std::string{base} + '/' + std::to_string(index);
    }

    std::optional<LevelJsonError> error_{};
};

[[nodiscard]] std::string_view direction_name(const Direction value) noexcept
{
    switch (value) {
    case Direction::north: return "north";
    case Direction::east: return "east";
    case Direction::south: return "south";
    case Direction::west: return "west";
    }
    return "unknown";
}

[[nodiscard]] std::string_view color_name(const SwitchColor value) noexcept
{
    switch (value) {
    case SwitchColor::red: return "red";
    case SwitchColor::green: return "green";
    case SwitchColor::blue: return "blue";
    case SwitchColor::yellow: return "yellow";
    }
    return "unknown";
}

[[nodiscard]] std::string_view entity_name(const EntityKind value) noexcept
{
    switch (value) {
    case EntityKind::player: return "player";
    case EntityKind::box: return "box";
    case EntityKind::barrel: return "barrel";
    }
    return "unknown";
}

template <typename Integer>
void append_integer(std::string& output, const Integer value)
{
    std::array<char, std::numeric_limits<Integer>::digits10 + 4> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    output.append(buffer.data(), result.ptr);
}

void append_coordinate(std::string& output, const Coordinate coordinate)
{
    output += "{\"x\":";
    append_integer(output, coordinate.x);
    output += ",\"y\":";
    append_integer(output, coordinate.y);
    output.push_back('}');
}

} // namespace

std::string_view to_string(const LevelJsonErrorCode code) noexcept
{
    switch (code) {
    case LevelJsonErrorCode::invalid_json: return "invalid_json";
    case LevelJsonErrorCode::document_too_large: return "document_too_large";
    case LevelJsonErrorCode::nesting_too_deep: return "nesting_too_deep";
    case LevelJsonErrorCode::root_not_object: return "root_not_object";
    case LevelJsonErrorCode::missing_member: return "missing_member";
    case LevelJsonErrorCode::unknown_member: return "unknown_member";
    case LevelJsonErrorCode::duplicate_member: return "duplicate_member";
    case LevelJsonErrorCode::invalid_member_type: return "invalid_member_type";
    case LevelJsonErrorCode::integer_out_of_range: return "integer_out_of_range";
    case LevelJsonErrorCode::invalid_enum_value: return "invalid_enum_value";
    case LevelJsonErrorCode::invalid_format: return "invalid_format";
    case LevelJsonErrorCode::unsupported_version: return "unsupported_version";
    case LevelJsonErrorCode::invalid_entity_id: return "invalid_entity_id";
    }
    return "unknown";
}

DecodeLevelJsonResult decode_level_json(const std::string_view json,
                                        const LevelJsonReadOptions options)
{
    DecodeLevelJsonResult result;
    if (options.max_document_bytes == 0 || json.size() > options.max_document_bytes) {
        result.json_error = LevelJsonError{LevelJsonErrorCode::document_too_large, 0, {}};
        return result;
    }
    if (options.max_nesting_depth == 0) {
        result.json_error = LevelJsonError{LevelJsonErrorCode::nesting_too_deep, 0, {}};
        return result;
    }

    yyjson_read_err read_error{};
    YyjsonDocument document{
        bomb_box_internal_yyjson_read(json.data(), json.size(), &read_error)};
    if (document.get() == nullptr) {
        result.json_error =
            LevelJsonError{LevelJsonErrorCode::invalid_json, read_error.pos, {}};
        return result;
    }

    yyjson_val* root = yyjson_doc_get_root(document.get());
    if (exceeds_nesting_depth(root, 0, options.max_nesting_depth)) {
        result.json_error = LevelJsonError{LevelJsonErrorCode::nesting_too_deep, 0, {}};
        return result;
    }

    LevelDecoder decoder;
    auto level = decoder.decode(root);
    if (!level.has_value()) {
        result.json_error = decoder.error();
        return result;
    }
    ValidationResult validation = validate_level(*level);
    if (!validation.valid()) {
        result.validation_errors = std::move(validation.errors);
        return result;
    }
    result.level = canonicalize_level(std::move(*level));
    return result;
}

EncodeLevelJsonResult encode_level_json(const LevelDefinition& level)
{
    EncodeLevelJsonResult result;
    ValidationResult validation = validate_level(level);
    if (!validation.valid()) {
        result.validation_errors = std::move(validation.errors);
        return result;
    }
    const LevelDefinition canonical = canonicalize_level(level);
    std::string output;
    output.reserve(512 + canonical.cells.size() * 80 + canonical.fixtures.size() * 80
                   + canonical.entities.size() * 100);
    output += "{\n  \"format\": \"bomb-box-level\",\n  \"version\": 1,\n";
    output += "  \"coordinateSystem\": {\"origin\":";
    append_coordinate(output, canonical.coordinates.origin);
    output += ",\"positiveX\":\"";
    output += canonical.coordinates.positive_x == HorizontalAxisDirection::east ? "east" : "west";
    output += "\",\"positiveY\":\"";
    output += canonical.coordinates.positive_y == VerticalAxisDirection::north ? "north" : "south";
    output += "\"},\n  \"width\": ";
    append_integer(output, canonical.width);
    output += ",\n  \"height\": ";
    append_integer(output, canonical.height);
    output += ",\n  \"cells\": [";
    for (std::size_t index = 0; index < canonical.cells.size(); ++index) {
        const Cell& cell = canonical.cells[index];
        output += index == 0 ? "\n    {\"coordinate\":" : ",\n    {\"coordinate\":";
        append_coordinate(output, cell.coordinate);
        if (const auto* flat = std::get_if<FlatCell>(&cell.geometry)) {
            output += ",\"type\":\"flat\",\"elevation\":";
            append_integer(output, flat->elevation);
        } else {
            const auto& ramp = std::get<RampCell>(cell.geometry);
            output += ",\"type\":\"ramp\",\"lowDirection\":\"";
            output += direction_name(ramp.low_direction);
            output += "\",\"lowElevation\":";
            append_integer(output, ramp.low_elevation);
        }
        output.push_back('}');
    }
    if (!canonical.cells.empty()) {
        output.push_back('\n');
        output += "  ";
    }
    output += "],\n  \"fixtures\": [";
    for (std::size_t index = 0; index < canonical.fixtures.size(); ++index) {
        const Fixture& fixture = canonical.fixtures[index];
        output += index == 0 ? "\n    {\"coordinate\":" : ",\n    {\"coordinate\":";
        append_coordinate(output, fixture.coordinate);
        if (const auto* switch_fixture = std::get_if<Switch>(&fixture.kind)) {
            output += ",\"type\":\"switch\",\"color\":\"";
            output += color_name(switch_fixture->color);
            output += "\"}";
        } else if (const auto* door = std::get_if<Door>(&fixture.kind)) {
            output += ",\"type\":\"door\",\"color\":\"";
            output += color_name(door->color);
            output += "\"}";
        } else {
            output += ",\"type\":\"exit\"}";
        }
    }
    if (!canonical.fixtures.empty()) {
        output.push_back('\n');
        output += "  ";
    }
    output += "],\n  \"entities\": [";
    for (std::size_t index = 0; index < canonical.entities.size(); ++index) {
        const Entity& entity = canonical.entities[index];
        output += index == 0 ? "\n    {\"id\":\"" : ",\n    {\"id\":\"";
        append_integer(output, entity.id);
        output += "\",\"type\":\"";
        output += entity_name(entity.kind);
        output += "\",\"coordinate\":";
        append_coordinate(output, entity.coordinate);
        output += ",\"bottomHalfSteps\":";
        append_integer(output, entity.bottom.half_steps);
        output.push_back('}');
    }
    if (!canonical.entities.empty()) {
        output.push_back('\n');
        output += "  ";
    }
    output += "]\n}\n";
    result.json = std::move(output);
    return result;
}

} // namespace bomb_box
