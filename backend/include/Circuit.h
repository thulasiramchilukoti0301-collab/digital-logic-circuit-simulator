#pragma once

#include "Component.h"
#include "Signal.h"
#include "Wire.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace digital_logic {

class Output;

struct ValidationResult {
    std::vector<std::string> errors;

    bool is_valid() const noexcept { return errors.empty(); }
};

struct EvaluationResult {
    bool success = false;
    std::vector<std::string> errors;
    std::unordered_map<ComponentId, Signal> outputs;
};

class Circuit final {
public:
    void add_component(std::unique_ptr<Component> component);
    void add_wire(const Wire& wire);

    const std::vector<std::unique_ptr<Component>>& components() const noexcept;
    const std::vector<Wire>& wires() const noexcept;
    const Component* find_component(ComponentId id) const noexcept;
    ValidationResult validate() const;
    EvaluationResult evaluate();

private:
    void validate_wire(const Wire& wire) const;
    void set_output_value(ComponentId id, Signal value) noexcept;

    std::vector<std::unique_ptr<Component>> components_;
    std::vector<Wire> wires_;
};

} // namespace digital_logic
