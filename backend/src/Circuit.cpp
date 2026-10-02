#include "Circuit.h"
#include "Output.h"

#include <stdexcept>
#include <utility>

namespace digital_logic {

void Circuit::add_component(std::unique_ptr<Component> component) {
    if (!component) {
        throw std::invalid_argument("cannot add a null component");
    }
    components_.push_back(std::move(component));
}

void Circuit::add_wire(const Wire& wire) {
    wires_.push_back(wire);
}

bool Circuit::update_output(ComponentId id, Signal value) noexcept {
    if (value != Signal::Low && value != Signal::High && value != Signal::Undefined) {
        return false;
    }
    for (const auto& component : components_) {
        if (component->id() == id) {
            auto* output = dynamic_cast<Output*>(component.get());
            if (output == nullptr) {
                return false;
            }
            output->set_value(value);
            return true;
        }
    }
    return false;
}

const std::vector<std::unique_ptr<Component>>& Circuit::components() const noexcept {
    return components_;
}

const std::vector<Wire>& Circuit::wires() const noexcept {
    return wires_;
}

const Component* Circuit::find_component(ComponentId id) const noexcept {
    for (const auto& component : components_) {
        if (component->id() == id) {
            return component.get();
        }
    }
    return nullptr;
}

} // namespace digital_logic
