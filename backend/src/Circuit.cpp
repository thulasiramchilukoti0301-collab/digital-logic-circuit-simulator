#include "Circuit.h"
#include "Gate.h"
#include "Input.h"
#include "Output.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace digital_logic {

namespace {

template <typename Action>
class ScopeExit final {
public:
    explicit ScopeExit(Action action) : action_(std::move(action)) {}
    ~ScopeExit() noexcept { action_(); }

    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;

private:
    Action action_;
};

} // namespace

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

void Circuit::set_output_value(ComponentId id, Signal value) noexcept {
    for (const auto& component : components_) {
        if (component->id() == id) {
            auto* output = dynamic_cast<Output*>(component.get());
            if (output != nullptr) {
                output->set_value(value);
            }
            return;
        }
    }
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

EvaluationResult Circuit::evaluate() {
    EvaluationResult result;
    const ValidationResult validation = validate();
    if (!validation.is_valid()) {
        result.errors = validation.errors;
        return result;
    }

    std::unordered_map<ComponentId, Signal> signals;
    std::unordered_map<ComponentId, std::vector<Signal>> destination_inputs;
    std::unordered_map<ComponentId, std::size_t> indegree;
    std::unordered_map<ComponentId, std::vector<const Wire*>> outgoing_wires;
    signals.reserve(components_.size());
    destination_inputs.reserve(components_.size());
    indegree.reserve(components_.size());

    for (const auto& component : components_) {
        indegree.emplace(component->id(), 0);
        if (const auto* input = dynamic_cast<const Input*>(component.get())) {
            signals.emplace(input->id(), input->value());
        } else if (const auto* gate = dynamic_cast<const Gate*>(component.get())) {
            destination_inputs.emplace(
                gate->id(), std::vector<Signal>(gate->input_count(), Signal::Undefined));
        } else if (dynamic_cast<const Output*>(component.get()) != nullptr) {
            destination_inputs.emplace(component->id(), std::vector<Signal>(1, Signal::Undefined));
        }
    }

    for (const Wire& wire : wires_) {
        ++indegree[wire.destination_id()];
        outgoing_wires[wire.source_id()].push_back(&wire);
    }

    std::queue<ComponentId> ready;
    for (const auto& component : components_) {
        if (indegree[component->id()] == 0) {
            ready.push(component->id());
        }
    }

    std::size_t processed = 0;
    while (!ready.empty()) {
        const ComponentId id = ready.front();
        ready.pop();
        ++processed;

        const Component* component = find_component(id);
        if (const auto* gate = dynamic_cast<const Gate*>(component)) {
            try {
                const std::vector<Signal>& inputs = destination_inputs.at(id);
                const Signal computed = gate->compute(inputs);
                if (computed != Signal::Low && computed != Signal::High) {
                    result.errors.emplace_back("gate " + std::to_string(id) +
                                               " produced an Undefined signal");
                    return result;
                }
                signals[id] = computed;
            } catch (const std::exception& error) {
                result.errors.emplace_back("gate " + std::to_string(id) +
                                           " computation failed: " + error.what());
                return result;
            } catch (...) {
                result.errors.emplace_back("gate " + std::to_string(id) +
                                           " computation failed with an unknown error");
                return result;
            }
        } else if (dynamic_cast<const Output*>(component) != nullptr) {
            const Signal value = destination_inputs.at(id).front();
            if (value != Signal::Low && value != Signal::High) {
                result.errors.emplace_back("output " + std::to_string(id) +
                                           " received an Undefined signal");
                return result;
            }
            result.outputs.emplace(id, value);
        }

        const auto outgoing = outgoing_wires.find(id);
        if (outgoing != outgoing_wires.end()) {
            const Signal source_signal = signals.at(id);
            for (const Wire* wire : outgoing->second) {
                auto& inputs = destination_inputs.at(wire->destination_id());
                inputs[wire->destination_pin()] = source_signal;
                std::size_t& remaining = indegree[wire->destination_id()];
                --remaining;
                if (remaining == 0) {
                    ready.push(wire->destination_id());
                }
            }
        }
    }

    if (processed != components_.size()) {
        result.outputs.clear();
        result.errors.emplace_back("circuit evaluation could not process every component; graph may contain a cycle");
        return result;
    }

    for (const auto& output : result.outputs) {
        set_output_value(output.first, output.second);
    }
    result.success = true;
    return result;
}

TruthTableResult Circuit::generate_truth_table(std::size_t max_rows) {
    TruthTableResult result;
    const ValidationResult validation = validate();
    if (!validation.is_valid()) {
        result.errors = validation.errors;
        return result;
    }

    std::vector<Input*> inputs;
    std::vector<Output*> outputs;
    for (const auto& component : components_) {
        if (auto* input = dynamic_cast<Input*>(component.get())) {
            inputs.push_back(input);
        } else if (auto* output = dynamic_cast<Output*>(component.get())) {
            outputs.push_back(output);
        }
    }
    const auto by_id = [](const Component* left, const Component* right) {
        return left->id() < right->id();
    };
    std::sort(inputs.begin(), inputs.end(), by_id);
    std::sort(outputs.begin(), outputs.end(), by_id);

    if (inputs.size() >= std::numeric_limits<std::size_t>::digits) {
        result.errors.emplace_back("truth table input count exceeds the supported row-count range");
        return result;
    }
    const std::size_t row_count = std::size_t{1} << inputs.size();
    if (row_count > max_rows) {
        result.errors.emplace_back("truth table requires " + std::to_string(row_count) +
                                   " rows, exceeding the configured maximum of " +
                                   std::to_string(max_rows));
        return result;
    }

    result.table.input_columns.reserve(inputs.size());
    std::vector<Signal> original_inputs;
    original_inputs.reserve(inputs.size());
    for (const Input* input : inputs) {
        result.table.input_columns.push_back({input->id(), input->name()});
        original_inputs.push_back(input->value());
    }

    result.table.output_columns.reserve(outputs.size());
    std::vector<Signal> original_outputs;
    original_outputs.reserve(outputs.size());
    for (const Output* output : outputs) {
        result.table.output_columns.push_back({output->id(), output->name()});
        original_outputs.push_back(output->value());
    }

    ScopeExit restore_state([&]() noexcept {
        for (std::size_t index = 0; index < inputs.size(); ++index) {
            inputs[index]->set_value(original_inputs[index]);
        }
        for (std::size_t index = 0; index < outputs.size(); ++index) {
            outputs[index]->set_value(original_outputs[index]);
        }
    });

    result.table.rows.reserve(row_count);
    for (std::size_t assignment = 0; assignment < row_count; ++assignment) {
        TruthTableRow row;
        row.inputs.reserve(inputs.size());
        for (std::size_t column = 0; column < inputs.size(); ++column) {
            const std::size_t bit_position = inputs.size() - column - 1;
            const bool high = ((assignment >> bit_position) & std::size_t{1}) != 0;
            const Signal value = high ? Signal::High : Signal::Low;
            inputs[column]->set_value(value);
            row.inputs.push_back(value);
        }

        const EvaluationResult evaluation = evaluate();
        if (!evaluation.success) {
            result.errors.emplace_back("truth-table evaluation failed at row " +
                                       std::to_string(assignment));
            result.errors.insert(result.errors.end(), evaluation.errors.begin(),
                                 evaluation.errors.end());
            result.table = TruthTable{};
            return result;
        }

        row.outputs.reserve(outputs.size());
        for (const Output* output : outputs) {
            const auto value = evaluation.outputs.find(output->id());
            if (value == evaluation.outputs.end()) {
                result.errors.emplace_back("evaluation did not return output " +
                                           std::to_string(output->id()));
                result.table = TruthTable{};
                return result;
            }
            row.outputs.push_back(value->second);
        }
        result.table.rows.push_back(std::move(row));
    }

    result.success = true;
    return result;
}

} // namespace digital_logic
