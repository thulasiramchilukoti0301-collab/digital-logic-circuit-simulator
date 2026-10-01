#pragma once

#include "Component.h"
#include "Wire.h"

#include <memory>
#include <vector>

namespace digital_logic {

class Circuit final {
public:
    void add_component(std::unique_ptr<Component> component);
    void add_wire(const Wire& wire);

    const std::vector<std::unique_ptr<Component>>& components() const noexcept;
    const std::vector<Wire>& wires() const noexcept;

private:
    std::vector<std::unique_ptr<Component>> components_;
    std::vector<Wire> wires_;
};

} // namespace digital_logic
