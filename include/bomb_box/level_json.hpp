#pragma once

#include "bomb_box/world.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bomb_box {

inline constexpr std::uint32_t level_json_format_version = 1;

enum class LevelJsonErrorCode : std::uint8_t {
    invalid_json,
    document_too_large,
    nesting_too_deep,
    root_not_object,
    missing_member,
    unknown_member,
    duplicate_member,
    invalid_member_type,
    integer_out_of_range,
    invalid_enum_value,
    invalid_format,
    unsupported_version,
    invalid_entity_id,
};

[[nodiscard]] std::string_view to_string(LevelJsonErrorCode code) noexcept;

struct LevelJsonError final {
    LevelJsonErrorCode code{LevelJsonErrorCode::invalid_json};
    std::size_t byte_offset{};
    // JSON Pointer to the failing value. Empty for errors that cannot be
    // associated with a decoded member. byte_offset is the upstream syntax
    // error offset, or zero when a successfully parsed DOM value has no
    // retained source position and path is authoritative.
    std::string path{};

    [[nodiscard]] friend bool operator==(const LevelJsonError&,
                                         const LevelJsonError&) = default;
};

struct LevelJsonReadOptions final {
    // Bounds resource use when loading untrusted shared levels. Callers may
    // choose a different non-zero limit without changing the file format.
    std::size_t max_document_bytes{16U * 1024U * 1024U};
    std::uint32_t max_nesting_depth{32};
};

struct DecodeLevelJsonResult final {
    std::optional<LevelDefinition> level{};
    std::optional<LevelJsonError> json_error{};
    std::vector<ValidationError> validation_errors{};

    [[nodiscard]] bool accepted() const noexcept { return level.has_value(); }
};

struct EncodeLevelJsonResult final {
    std::optional<std::string> json{};
    std::vector<ValidationError> validation_errors{};

    [[nodiscard]] bool accepted() const noexcept { return json.has_value(); }
};

// Decodes the strict, versioned Bomb Box level JSON format and then runs the
// same structural validation used by Engine::load_level(). Unknown members are
// rejected so misspellings and unsupported newer data are never ignored.
[[nodiscard]] DecodeLevelJsonResult
decode_level_json(std::string_view json, LevelJsonReadOptions options = {});

// Validates and emits canonical UTF-8 JSON. Array order is canonicalized and
// the output ends with one newline, so equivalent levels serialize identically.
[[nodiscard]] EncodeLevelJsonResult encode_level_json(const LevelDefinition& level);

} // namespace bomb_box
