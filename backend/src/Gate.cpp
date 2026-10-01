#include "Gate.h"

namespace digital_logic {

Gate::Gate(ComponentId id, std::size_t input_count) noexcept
    : Component(id), input_count_(input_count) {}

std::size_t Gate::input_count() const noexcept {
    return input_count_;
}

} // namespace digital_logic
