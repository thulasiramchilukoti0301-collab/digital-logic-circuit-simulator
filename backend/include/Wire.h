#pragma once

#include "Component.h"

#include <cstddef>

namespace digital_logic {

class Wire final {
public:
    Wire(ComponentId source_id, ComponentId destination_id,
         std::size_t destination_pin) noexcept;

    ComponentId source_id() const noexcept;
    ComponentId destination_id() const noexcept;
    std::size_t destination_pin() const noexcept;

private:
    ComponentId source_id_;
    ComponentId destination_id_;
    std::size_t destination_pin_;
};

} // namespace digital_logic
