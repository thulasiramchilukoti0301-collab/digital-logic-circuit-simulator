#include "Circuit.h"
#include "Gate.h"
#include "Input.h"
#include "Output.h"

#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace digital_logic {

void Circuit::add_component(std::unique_ptr<Component> component) {
    if (!component) {
        throw std::invalid_argument("cannot add a null component");
    }
    if (find_component(component->id()) != nullptr) {
        throw std::invalid_argument("duplicate component ID: " +
                                    std::to_string(component->id()));
    }
    components_.push_back(std::move(component));
}

void Circuit::add_wire(const Wire& wire) {
    validate_wire(wire);
    wires_.push_back(wire);
}

void Circuit::validate_wire(const Wire& wire) const {
    const Component* source = find_component(wire.source_id());
    const Component* destination = find_component(wire.destination_id());

    if (source == nullptr) {
        throw std::invalid_argument("wire source component does not exist: " +
                                    std::to_string(wire.source_id()));
    }
    if (destination == nullptr) {
        throw std::invalid_argument("wire destination component does not exist: " +
                                    std::to_string(wire.destination_id()));
    }
    if (wire.source_id() == wire.destination_id()) {
        throw std::invalid_argument("wire cannot connect a component to itself");
    }

    const bool source_can_drive =
        dynamic_cast<const Input*>(source) != nullptr ||
        dynamic_cast<const Gate*>(source) != nullptr;
    if (!source_can_drive) {
        throw std::invalid_argument("wire source must be an Input or Gate");
    }

    const auto* destination_gate = dynamic_cast<const Gate*>(destination);
    const bool destination_is_output = dynamic_cast<const Output*>(destination) != nullptr;
    if (destination_gate == nullptr && !destination_is_output) {
        throw std::invalid_argument("wire destination must be a Gate or Output");
    }

    const std::size_t pin_count = destination_gate != nullptr
        ? destination_gate->input_count()
        : 1;
    if (wire.destination_pin() >= pin_count) {
        throw std::invalid_argument("wire destination pin is out of range");
    }

    for (const Wire& existing : wires_) {
        if (existing.destination_id() == wire.destination_id() &&
            existing.destination_pin() == wire.destination_pin()) {
            throw std::invalid_argument("destination pin already has an incoming wire");
        }
        if (existing.source_id() == wire.source_id() &&
            existing.destination_id() == wire.destination_id() &&
            existing.destination_pin() == wire.destination_pin()) {
            throw std::invalid_argument("duplicate wire connection");
        }
    }
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

ValidationResult Circuit::validate() const {
    ValidationResult result;
    std::unordered_map<ComponentId, const Component*> components_by_id;
    components_by_id.reserve(components_.size());
    for (const auto& component : components_) {
        if (!component) {
            result.errors.emplace_back("circuit contains a null component");
            continue;
        }
        if (!components_by_id.emplace(component->id(), component.get()).second) {
            result.errors.emplace_back("duplicate component ID: " +
                                       std::to_string(component->id()));
        }
    }

    std::unordered_map<ComponentId, std::unordered_set<std::size_t>> connected_pins;
    std::unordered_map<ComponentId, std::vector<ComponentId>> adjacency;
    for (const Wire& wire : wires_) {
        const auto source_it = components_by_id.find(wire.source_id());
        const auto destination_it = components_by_id.find(wire.destination_id());
        if (source_it == components_by_id.end()) {
            result.errors.emplace_back("wire source component does not exist: " +
                                       std::to_string(wire.source_id()));
            continue;
        }
        if (destination_it == components_by_id.end()) {
            result.errors.emplace_back("wire destination component does not exist: " +
                                       std::to_string(wire.destination_id()));
            continue;
        }
        if (wire.source_id() == wire.destination_id()) {
            result.errors.emplace_back("wire cannot connect a component to itself: " +
                                       std::to_string(wire.source_id()));
        }

        const Component* source = source_it->second;
        const Component* destination = destination_it->second;
        if (dynamic_cast<const Input*>(source) == nullptr &&
            dynamic_cast<const Gate*>(source) == nullptr) {
            result.errors.emplace_back("wire source must be an Input or Gate: " +
                                       std::to_string(wire.source_id()));
        }

        const auto* gate = dynamic_cast<const Gate*>(destination);
        const bool is_output = dynamic_cast<const Output*>(destination) != nullptr;
        if (gate == nullptr && !is_output) {
            result.errors.emplace_back("wire destination must be a Gate or Output: " +
                                       std::to_string(wire.destination_id()));
            continue;
        }

        const std::size_t pin_count = gate != nullptr ? gate->input_count() : 1;
        if (wire.destination_pin() >= pin_count) {
            result.errors.emplace_back("wire destination pin is out of range for component: " +
                                       std::to_string(wire.destination_id()));
            continue;
        }
        if (!connected_pins[wire.destination_id()].insert(wire.destination_pin()).second) {
            result.errors.emplace_back("multiple wires connect to destination pin " +
                                       std::to_string(wire.destination_pin()) +
                                       " on component " +
                                       std::to_string(wire.destination_id()));
        }
        adjacency[wire.source_id()].push_back(wire.destination_id());
    }

    for (const auto& entry : components_by_id) {
        const ComponentId id = entry.first;
        const Component* component = entry.second;
        if (const auto* gate = dynamic_cast<const Gate*>(component)) {
            for (std::size_t pin = 0; pin < gate->input_count(); ++pin) {
                if (connected_pins[id].count(pin) == 0) {
                    result.errors.emplace_back("gate " + std::to_string(id) +
                                               " has an unconnected input pin " +
                                               std::to_string(pin));
                }
            }
        } else if (dynamic_cast<const Output*>(component) != nullptr &&
                   connected_pins[id].count(0) == 0) {
            result.errors.emplace_back("output " + std::to_string(id) +
                                       " has no incoming source");
        }
    }

    enum class VisitState { Unvisited, Visiting, Visited };
    std::unordered_map<ComponentId, VisitState> visit_state;
    std::function<bool(ComponentId)> has_cycle = [&](ComponentId id) {
        VisitState& state = visit_state[id];
        if (state == VisitState::Visiting) {
            return true;
        }
        if (state == VisitState::Visited) {
            return false;
        }
        state = VisitState::Visiting;
        const auto edges = adjacency.find(id);
        if (edges != adjacency.end()) {
            for (ComponentId next : edges->second) {
                if (has_cycle(next)) {
                    return true;
                }
            }
        }
        state = VisitState::Visited;
        return false;
    };

    for (const auto& entry : components_by_id) {
        if (visit_state[entry.first] == VisitState::Unvisited && has_cycle(entry.first)) {
            result.errors.emplace_back("circuit contains a combinational cycle");
            break;
        }
    }

    return result;
}

} // namespace digital_logic
