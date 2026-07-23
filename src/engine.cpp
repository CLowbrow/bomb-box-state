#include "bomb_box/engine.hpp"

namespace bomb_box {

std::string_view to_string(const EngineStatus status) noexcept
{
    switch (status) {
    case EngineStatus::not_implemented:
        return "not_implemented";
    }

    return "unknown";
}

EngineStatus Engine::status() const noexcept
{
    return EngineStatus::not_implemented;
}

} // namespace bomb_box

