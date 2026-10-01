#include "Circuit.h"

#include <utility>

namespace digital_logic {

void Circuit::add_component(std::unique_ptr<Component> component) {
    components_.push_back(std::move(component));
}

void Circuit::add_wire(const Wire& wire) {
    wires_.push_back(wire);
}

const std::vector<std::unique_ptr<Component>>& Circuit::components() const noexcept {
    return components_;
}

const std::vector<Wire>& Circuit::wires() const noexcept {
    return wires_;
}

} // namespace digital_logic
