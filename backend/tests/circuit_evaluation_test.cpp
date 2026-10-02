#include "ANDGate.h"
#include "Circuit.h"
#include "Gate.h"
#include "Input.h"
#include "NOTGate.h"
#include "ORGate.h"
#include "Output.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using digital_logic::Signal;

int failures = 0;

void expect(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

template <typename ComponentType, typename... Args>
void add(digital_logic::Circuit& circuit, Args&&... args) {
    circuit.add_component(
        std::make_unique<ComponentType>(std::forward<Args>(args)...));
}

void connect(digital_logic::Circuit& circuit, digital_logic::ComponentId source,
             digital_logic::ComponentId destination, std::size_t pin) {
    circuit.add_wire(digital_logic::Wire(source, destination, pin));
}

bool has_error(const digital_logic::EvaluationResult& result,
               const std::string& part) {
    for (const auto& error : result.errors) {
        if (error.find(part) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool output_equals(const digital_logic::EvaluationResult& result,
                   digital_logic::ComponentId id, Signal expected) {
    const auto output = result.outputs.find(id);
    return output != result.outputs.end() && output->second == expected;
}

class UndefinedGate final : public digital_logic::Gate {
public:
    explicit UndefinedGate(digital_logic::ComponentId id) : Gate(id, 1) {}

    Signal compute(const std::vector<Signal>&) const override {
        return Signal::Undefined;
    }
};

class ThrowingGate final : public digital_logic::Gate {
public:
    explicit ThrowingGate(digital_logic::ComponentId id) : Gate(id, 1) {}

    Signal compute(const std::vector<Signal>&) const override {
        throw std::invalid_argument("test computation error");
    }
};

void test_and_truth_table() {
    digital_logic::Circuit circuit;
    add<digital_logic::Input>(circuit, 1, "A");
    add<digital_logic::Input>(circuit, 2, "B");
    add<digital_logic::ANDGate>(circuit, 3);
    add<digital_logic::Output>(circuit, 4, "Y");
    connect(circuit, 1, 3, 0);
    connect(circuit, 2, 3, 1);
    connect(circuit, 3, 4, 0);

    const Signal cases[][3] = {
        {Signal::Low, Signal::Low, Signal::Low},
        {Signal::Low, Signal::High, Signal::Low},
        {Signal::High, Signal::Low, Signal::Low},
        {Signal::High, Signal::High, Signal::High},
    };
    auto* a = const_cast<digital_logic::Input*>(
        dynamic_cast<const digital_logic::Input*>(circuit.find_component(1)));
    auto* b = const_cast<digital_logic::Input*>(
        dynamic_cast<const digital_logic::Input*>(circuit.find_component(2)));
    for (const auto& test_case : cases) {
        a->set_value(test_case[0]);
        b->set_value(test_case[1]);
        const auto result = circuit.evaluate();
        expect(result.success, "AND input combination evaluates successfully");
        expect(output_equals(result, 4, test_case[2]), "AND output matches truth table");
    }
}

void test_compound_circuit() {
    digital_logic::Circuit circuit;
    add<digital_logic::Input>(circuit, 1, "A");
    add<digital_logic::Input>(circuit, 2, "B");
    add<digital_logic::Input>(circuit, 3, "C");
    add<digital_logic::ANDGate>(circuit, 4);
    add<digital_logic::ORGate>(circuit, 5);
    add<digital_logic::Output>(circuit, 6, "Y");
    connect(circuit, 1, 4, 0);
    connect(circuit, 2, 4, 1);
    connect(circuit, 4, 5, 0);
    connect(circuit, 3, 5, 1);
    connect(circuit, 5, 6, 0);

    auto* a = const_cast<digital_logic::Input*>(
        dynamic_cast<const digital_logic::Input*>(circuit.find_component(1)));
    auto* b = const_cast<digital_logic::Input*>(
        dynamic_cast<const digital_logic::Input*>(circuit.find_component(2)));
    auto* c = const_cast<digital_logic::Input*>(
        dynamic_cast<const digital_logic::Input*>(circuit.find_component(3)));
    const Signal cases[][4] = {
        {Signal::Low, Signal::Low, Signal::Low, Signal::Low},
        {Signal::Low, Signal::High, Signal::High, Signal::High},
        {Signal::High, Signal::High, Signal::Low, Signal::High},
        {Signal::High, Signal::Low, Signal::Low, Signal::Low},
        {Signal::High, Signal::High, Signal::High, Signal::High},
    };
    for (const auto& test_case : cases) {
        a->set_value(test_case[0]);
        b->set_value(test_case[1]);
        c->set_value(test_case[2]);
        const auto result = circuit.evaluate();
        expect(result.success, "compound circuit evaluates successfully");
        expect(output_equals(result, 6, test_case[3]),
               "compound output matches (A AND B) OR C");
    }
}

void test_fan_out_and_multiple_outputs() {
    digital_logic::Circuit circuit;
    add<digital_logic::Input>(circuit, 1, "A");
    add<digital_logic::NOTGate>(circuit, 2);
    add<digital_logic::ANDGate>(circuit, 3);
    add<digital_logic::Output>(circuit, 4, "A direct");
    add<digital_logic::Output>(circuit, 5, "NOT A");
    add<digital_logic::Output>(circuit, 6, "A AND NOT A");
    connect(circuit, 1, 2, 0);
    connect(circuit, 1, 3, 0);
    connect(circuit, 2, 3, 1);
    connect(circuit, 1, 4, 0);
    connect(circuit, 2, 5, 0);
    connect(circuit, 3, 6, 0);

    auto* input = const_cast<digital_logic::Input*>(
        dynamic_cast<const digital_logic::Input*>(circuit.find_component(1)));
    for (Signal value : {Signal::Low, Signal::High}) {
        input->set_value(value);
        const auto result = circuit.evaluate();
        expect(result.success, "fan-out circuit evaluates successfully");
        expect(result.outputs.size() == 3, "all output values are returned");
        expect(output_equals(result, 4, value), "direct fan-out output receives input");
        expect(output_equals(result, 5,
                   value == Signal::Low ? Signal::High : Signal::Low),
               "NOT branch receives fan-out signal");
        expect(output_equals(result, 6, Signal::Low),
               "AND branch receives fan-out signal correctly");
    }
}

void test_invalid_structure_preserves_outputs() {
    digital_logic::Circuit circuit;
    add<digital_logic::Input>(circuit, 1, "A");
    add<digital_logic::Output>(circuit, 2, "Y");
    connect(circuit, 1, 2, 0);
    const auto initial = circuit.evaluate();
    expect(initial.success && output_equals(initial, 2, Signal::Low),
           "valid circuit establishes initial output");
    auto* input = const_cast<digital_logic::Input*>(
        dynamic_cast<const digital_logic::Input*>(circuit.find_component(1)));
    input->set_value(Signal::High);
    add<digital_logic::ANDGate>(circuit, 3); // Unconnected gate makes structure invalid.
    const auto failed = circuit.evaluate();
    expect(!failed.success, "invalid structure returns failure");
    expect(!failed.errors.empty(), "invalid structure returns readable errors");
    const auto* output = dynamic_cast<const digital_logic::Output*>(
        circuit.find_component(2));
    expect(output != nullptr && output->value() == Signal::Low,
           "failed evaluation preserves previous output value");
}

void test_reverse_insertion_order() {
    digital_logic::Circuit circuit;
    add<digital_logic::Output>(circuit, 6, "Y");
    add<digital_logic::ORGate>(circuit, 5);
    add<digital_logic::ANDGate>(circuit, 4);
    add<digital_logic::Input>(circuit, 3, "C", Signal::High);
    add<digital_logic::Input>(circuit, 2, "B", Signal::High);
    add<digital_logic::Input>(circuit, 1, "A", Signal::High);
    connect(circuit, 1, 4, 0);
    connect(circuit, 2, 4, 1);
    connect(circuit, 4, 5, 0);
    connect(circuit, 3, 5, 1);
    connect(circuit, 5, 6, 0);

    const auto result = circuit.evaluate();
    expect(result.success, "reverse-insertion circuit evaluates successfully");
    expect(output_equals(result, 6, Signal::High),
           "topological evaluation ignores component insertion order");
}

void test_failure_returns_no_partial_outputs() {
    digital_logic::Circuit circuit;
    add<digital_logic::Output>(circuit, 9, "Direct");
    add<digital_logic::Output>(circuit, 8, "Failure branch");
    add<ThrowingGate>(circuit, 4);
    add<digital_logic::Input>(circuit, 1, "A", Signal::High);
    add<digital_logic::Input>(circuit, 2, "B", Signal::High);
    connect(circuit, 1, 9, 0);
    connect(circuit, 2, 4, 0);
    connect(circuit, 4, 8, 0);

    const auto result = circuit.evaluate();
    expect(!result.success, "a throwing gate fails circuit evaluation");
    expect(!result.errors.empty(), "failed evaluation includes error information");
    expect(result.outputs.empty(), "failed evaluation returns no partial outputs");
    const auto* direct_output = dynamic_cast<const digital_logic::Output*>(
        circuit.find_component(9));
    const auto* failing_output = dynamic_cast<const digital_logic::Output*>(
        circuit.find_component(8));
    expect(direct_output != nullptr && direct_output->value() == Signal::Undefined &&
               failing_output != nullptr && failing_output->value() == Signal::Undefined,
           "failed evaluation does not partially update stored outputs");
}

void test_gate_errors_and_undefined_results() {
    for (bool return_undefined : {false, true}) {
        digital_logic::Circuit circuit;
        add<digital_logic::Input>(circuit, 1, "A");
        if (return_undefined) {
            add<UndefinedGate>(circuit, 2);
        } else {
            add<ThrowingGate>(circuit, 2);
        }
        add<digital_logic::Output>(circuit, 3, "Y");
        connect(circuit, 1, 2, 0);
        connect(circuit, 2, 3, 0);
        const auto result = circuit.evaluate();
        expect(!result.success, "gate error returns evaluation failure");
        expect(result.outputs.empty(), "gate failure returns no output values");
        expect(has_error(result, "gate 2"), "gate error identifies component ID");
        if (return_undefined) {
            expect(has_error(result, "Undefined"),
                   "Undefined gate result is reported explicitly");
        } else {
            expect(has_error(result, "test computation error"),
                   "gate exception message is included in error");
        }
        const auto* output = dynamic_cast<const digital_logic::Output*>(
            circuit.find_component(3));
        expect(output != nullptr && output->value() == Signal::Undefined,
               "gate failure does not commit output state");
    }
}

void test_cycle_refusal() {
    digital_logic::Circuit circuit;
    add<digital_logic::Input>(circuit, 1, "A");
    add<digital_logic::ANDGate>(circuit, 2);
    add<digital_logic::ANDGate>(circuit, 3);
    add<digital_logic::Output>(circuit, 4, "Y");
    connect(circuit, 1, 2, 0);
    connect(circuit, 1, 3, 0);
    connect(circuit, 2, 3, 1);
    connect(circuit, 3, 2, 1);
    connect(circuit, 3, 4, 0);
    const auto result = circuit.evaluate();
    expect(!result.success, "cyclic circuit evaluation fails");
    expect(has_error(result, "cycle"), "cycle error is reported");
}

} // namespace

int main() {
    test_and_truth_table();
    test_compound_circuit();
    test_fan_out_and_multiple_outputs();
    test_invalid_structure_preserves_outputs();
    test_gate_errors_and_undefined_results();
    test_cycle_refusal();
    test_reverse_insertion_order();
    test_failure_returns_no_partial_outputs();

    if (failures != 0) {
        std::cerr << failures << " evaluation test(s) failed\n";
        return 1;
    }
    std::cout << "All circuit evaluation tests passed\n";
    return 0;
}
