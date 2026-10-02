#pragma once

#include "Component.h"
#include "Signal.h"
#include "Wire.h"

#include <memory>
#include <string>
#include <vector>

namespace digital_logic {

class Output;

struct ValidationResult {
    std::vector<std::string> errors;

    bool is_valid() const noexcept { return errors.empty(); }
};

class Circuit final {
public:
    void add_component(std::unique_ptr<Component> component);
    void add_wire(const Wire& wire);
    bool update_output(ComponentId id, Signal value) noexcept;

    const std::vector<std::unique_ptr<Component>>& components() const noexcept;
    const std::vector<Wire>& wires() const noexcept;
    const Component* find_component(ComponentId id) const noexcept;
    ValidationResult validate() const;

private:
    void validate_wire(const Wire& wire) const;

    std::vector<std::unique_ptr<Component>> components_;
    std::vector<Wire> wires_;
};

} // namespace digital_logic
