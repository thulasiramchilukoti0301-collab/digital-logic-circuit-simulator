#include "ANDGate.h"
#include "Circuit.h"
#include "Input.h"
#include "Output.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using digital_logic::Signal;

int failures = 0;

void expect(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

template <typename Function>
void expect_invalid_argument(const std::string& label, Function&& function) {
    try {
        function();
        std::cerr << "FAIL: " << label << " did not throw std::invalid_argument\n";
        ++failures;
    } catch (const std::invalid_argument&) {
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << label << " threw an unexpected exception: "
                  << error.what() << '\n';
        ++failures;
    }
}

} // namespace

int main() {
    digital_logic::Input input(101, "Enable");
    expect(input.id() == 101, "Input retains its component ID");
    expect(input.name() == "Enable", "Input retains its name");
    expect(input.value() == Signal::Low, "Input defaults to Low");
    input.set_value(Signal::High);
    expect(input.value() == Signal::High, "Input can be set to High");
    input.set_value(Signal::Low);
    expect(input.value() == Signal::Low, "Input can be set to Low");
    expect_invalid_argument("Input constructor rejects Undefined", [] {
        digital_logic::Input invalid(102, "Invalid", Signal::Undefined);
    });
    expect_invalid_argument("Input setter rejects Undefined", [&] {
        input.set_value(Signal::Undefined);
    });
    expect(input.value() == Signal::Low,
           "Rejected Input update leaves the previous value intact");

    digital_logic::Output output(201, "Result");
    expect(output.id() == 201, "Output retains its component ID");
    expect(output.name() == "Result", "Output retains its name");
    expect(output.value() == Signal::Undefined,
           "Output starts with an Undefined value");

    digital_logic::Wire wire(101, 201, 3);
    expect(wire.source_id() == 101, "Wire reports its source component ID");
    expect(wire.destination_id() == 201,
           "Wire reports its destination component ID");
    expect(wire.destination_pin() == 3, "Wire reports its destination pin");

    digital_logic::Circuit circuit;
    circuit.add_component(std::make_unique<digital_logic::Input>(301, "A"));
    circuit.add_component(std::make_unique<digital_logic::Output>(302, "Y"));
    circuit.add_component(std::make_unique<digital_logic::ANDGate>(303));
    circuit.add_wire(digital_logic::Wire(301, 303, 0));

    expect(circuit.components().size() == 3,
           "Circuit owns Input, Output, and Gate components");
    expect(circuit.wires().size() == 1, "Circuit stores Wire by value");
    expect(circuit.find_component(301) != nullptr &&
               dynamic_cast<const digital_logic::Input*>(circuit.find_component(301)) != nullptr,
           "Circuit lookup finds a polymorphically owned Input");
    expect(circuit.find_component(302) != nullptr &&
               dynamic_cast<const digital_logic::Output*>(circuit.find_component(302)) != nullptr,
           "Circuit lookup finds a polymorphically owned Output");
    const auto* stored_gate =
        dynamic_cast<const digital_logic::Gate*>(circuit.find_component(303));
    expect(stored_gate != nullptr,
           "Circuit lookup finds a polymorphically owned Gate");
    expect(circuit.find_component(999) == nullptr,
           "Circuit lookup returns nullptr for an unknown ID");
    const auto* stored_output =
        dynamic_cast<const digital_logic::Output*>(circuit.find_component(302));
    expect(circuit.update_output(302, Signal::Low),
           "Circuit can update a stored Output to Low");
    expect(stored_output != nullptr && stored_output->value() == Signal::Low,
           "Output reports its updated Low value");
    expect(circuit.update_output(302, Signal::High),
           "Circuit can update a stored Output to High");
    expect(stored_output != nullptr && stored_output->value() == Signal::High,
           "Output reports its updated High value");
    expect(!circuit.update_output(301, Signal::Low),
           "Circuit refuses to update a non-Output component");
    expect(!circuit.update_output(999, Signal::High),
           "Circuit refuses to update a missing component ID");
    if (stored_gate != nullptr) {
        expect(stored_gate->compute({Signal::High, Signal::High}) == Signal::High,
               "Stored Gate remains usable through the Gate interface");
    }

    expect_invalid_argument("Circuit rejects null component ownership", [&] {
        circuit.add_component(nullptr);
    });
    expect(circuit.components().size() == 3,
           "Rejecting a null component leaves Circuit unchanged");

    if (failures != 0) {
        std::cerr << failures << " component test(s) failed\n";
        return 1;
    }

    std::cout << "All component tests passed\n";
    return 0;
}
