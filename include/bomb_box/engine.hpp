#pragma once

#include <cstdint>
#include <string_view>

namespace bomb_box {

inline constexpr std::uint32_t api_version = 1;

enum class EngineStatus : std::uint8_t {
    not_implemented,
};

[[nodiscard]] std::string_view to_string(EngineStatus status) noexcept;

// This is intentionally only a lifecycle stub. The public gameplay API should be
// introduced alongside the world-schema implementation and its behavior tests.
class Engine final {
  public:
    Engine() noexcept = default;

    [[nodiscard]] EngineStatus status() const noexcept;
};

} // namespace bomb_box

