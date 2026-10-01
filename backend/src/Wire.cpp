#include "Wire.h"

namespace digital_logic {

Wire::Wire(ComponentId source_id, ComponentId destination_id,
           std::size_t destination_pin) noexcept
    : source_id_(source_id), destination_id_(destination_id),
      destination_pin_(destination_pin) {}

ComponentId Wire::source_id() const noexcept {
    return source_id_;
}

ComponentId Wire::destination_id() const noexcept {
    return destination_id_;
}

std::size_t Wire::destination_pin() const noexcept {
    return destination_pin_;
}

} // namespace digital_logic
