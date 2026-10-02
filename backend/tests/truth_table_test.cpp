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
#include <vector>

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

digital_logic::Input* mutable_input(digital_logic::Circuit& circuit,
                                   digital_logic::ComponentId id) {
    for (const auto& component : circuit.components()) {
        if (component->id() == id) {
            return dynamic_cast<digital_logic::Input*>(component.get());
        }
    }
    return nullptr;
}

digital_logic::Output* mutable_output(digital_logic::Circuit& circuit,
                                     digital_logic::ComponentId id) {
    for (const auto& component : circuit.components()) {
        if (component->id() == id) {
            return dynamic_cast<digital_logic::Output*>(component.get());
        }
    }
    return nullptr;
}

bool has_error(const digital_logic::TruthTableResult& result,
               const std::string& fragment) {
    for (const auto& error : result.errors) {
        if (error.find(fragment) != std::string::npos) {
            return true;
        }
    }
    return false;
}

class ConditionalGate final : public digital_logic::Gate {
public:
    ConditionalGate(digital_logic::ComponentId id, bool undefined)
        : Gate(id, 1), undefined_(undefined) {}

    Signal compute(const std::vector<Signal>& inputs) const override {
        if (inputs.at(0) == Signal::Low) {
            if (undefined_) {
                return Signal::Undefined;
            }
            throw std::invalid_argument("conditional test failure");
        }
        return Signal::High;
    }

private:
    bool undefined_;
};

void test_not_and_and_tables() {
    {
        digital_logic::Circuit circuit;
        add<digital_logic::Input>(circuit, 1, "A", Signal::High);
        add<digital_logic::NOTGate>(circuit, 2);
        add<digital_logic::Output>(circuit, 3, "Y");
        connect(circuit, 1, 2, 0);
        connect(circuit, 2, 3, 0);
        const auto table = circuit.generate_truth_table();
        expect(table.success, "NOT truth table succeeds");
        expect(table.table.rows.size() == 2, "NOT truth table has two rows");
        if (table.success && table.table.rows.size() == 2) {
            expect(table.table.rows[0].inputs[0] == Signal::Low &&
                       table.table.rows[0].outputs[0] == Signal::High,
                   "NOT row 0 -> 1");
            expect(table.table.rows[1].inputs[0] == Signal::High &&
                       table.table.rows[1].outputs[0] == Signal::Low,
                   "NOT row 1 -> 0");
        }
    }
    {
        digital_logic::Circuit circuit;
        add<digital_logic::Input>(circuit, 1, "A");
        add<digital_logic::Input>(circuit, 2, "B");
        add<digital_logic::ANDGate>(circuit, 3);
        add<digital_logic::Output>(circuit, 4, "Y");
        connect(circuit, 1, 3, 0);
        connect(circuit, 2, 3, 1);
        connect(circuit, 3, 4, 0);
        const auto table = circuit.generate_truth_table();
        expect(table.success, "AND truth table succeeds");
        const Signal expected[] = {Signal::Low, Signal::Low,
                                   Signal::Low, Signal::High};
        expect(table.table.rows.size() == 4, "AND truth table has four rows");
        if (table.success && table.table.rows.size() == 4) {
            for (std::size_t i = 0; i < 4; ++i) {
                expect(table.table.rows[i].outputs[0] == expected[i],
                       "AND row output matches truth table");
                expect(table.table.rows[i].inputs[0] == ((i & 2) ? Signal::High : Signal::Low),
                       "first sorted input is most significant bit");
                expect(table.table.rows[i].inputs[1] == ((i & 1) ? Signal::High : Signal::Low),
                       "rows follow binary assignment order");
            }
        }
    }
}

void test_compound_table() {
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
    const auto table = circuit.generate_truth_table();
    expect(table.success, "compound truth table succeeds");
    const Signal expected[] = {Signal::Low, Signal::High, Signal::Low, Signal::High,
                               Signal::Low, Signal::High, Signal::High, Signal::High};
    expect(table.table.rows.size() == 8, "compound table has all eight rows");
    if (table.success && table.table.rows.size() == 8) {
        for (std::size_t i = 0; i < 8; ++i) {
            expect(table.table.rows[i].outputs[0] == expected[i],
                   "compound row matches (A AND B) OR C");
        }
    }
}

void test_order_metadata_and_restore() {
    digital_logic::Circuit circuit;
    add<digital_logic::Output>(circuit, 90, "Later output");
    add<digital_logic::Input>(circuit, 20, "B", Signal::High);
    add<digital_logic::Output>(circuit, 40, "Earlier output");
    add<digital_logic::Input>(circuit, 10, "A", Signal::Low);
    add<digital_logic::ORGate>(circuit, 30);
    connect(circuit, 10, 30, 0);
    connect(circuit, 20, 30, 1);
    connect(circuit, 30, 40, 0);
    connect(circuit, 20, 90, 0);
    const auto initial = circuit.evaluate();
    expect(initial.success, "ordering fixture evaluates before table generation");
    const auto table = circuit.generate_truth_table();
    expect(table.success, "ordering and restoration table succeeds");
    expect(table.table.input_columns.size() == 2 &&
               table.table.input_columns[0].id == 10 &&
               table.table.input_columns[1].id == 20,
           "input columns sorted by ID");
    expect(table.table.input_columns[0].name == "A" &&
               table.table.input_columns[1].name == "B",
           "input metadata preserves names");
    expect(table.table.output_columns.size() == 2 &&
               table.table.output_columns[0].id == 40 &&
               table.table.output_columns[1].id == 90,
           "output columns sorted by ID");
    expect(table.table.output_columns[0].name == "Earlier output" &&
               table.table.output_columns[1].name == "Later output",
           "output metadata preserves names");
    expect(mutable_input(circuit, 10)->value() == Signal::Low &&
               mutable_input(circuit, 20)->value() == Signal::High,
           "original input values are restored");
    expect(mutable_output(circuit, 40)->value() == Signal::High &&
               mutable_output(circuit, 90)->value() == Signal::High,
           "original output values are restored");
}

void test_errors_and_limits_restore_state() {
    {
        digital_logic::Circuit circuit;
        add<digital_logic::Input>(circuit, 1, "A", Signal::High);
        add<digital_logic::Output>(circuit, 2, "Y");
        connect(circuit, 1, 2, 0);
        circuit.evaluate();
        add<digital_logic::ANDGate>(circuit, 3);
        const auto result = circuit.generate_truth_table();
        expect(!result.success && !result.errors.empty(),
               "invalid circuit returns generation failure");
        expect(result.table.rows.empty(), "invalid circuit returns no partial table");
        expect(mutable_input(circuit, 1)->value() == Signal::High &&
                   mutable_output(circuit, 2)->value() == Signal::High,
               "invalid circuit preserves all state");
    }

    for (bool undefined : {false, true}) {
        digital_logic::Circuit circuit;
        add<digital_logic::Input>(circuit, 1, "A", Signal::High);
        add<ConditionalGate>(circuit, 2, undefined);
        add<digital_logic::Output>(circuit, 3, "Y");
        connect(circuit, 1, 2, 0);
        connect(circuit, 2, 3, 0);
        circuit.evaluate(); // Save a prior calculated Output value of High.
        mutable_input(circuit, 1)->set_value(Signal::Low);
        const auto result = circuit.generate_truth_table();
        expect(!result.success, "gate computation failure aborts table generation");
        expect(result.table.rows.empty(), "failed generation has no partial rows");
        expect(mutable_input(circuit, 1)->value() == Signal::Low,
               "gate failure restores original Input value");
        expect(mutable_output(circuit, 3)->value() == Signal::High,
               "gate failure restores original Output value");
        expect(has_error(result, undefined ? "Undefined" : "conditional test failure"),
               "gate error is retained in generation result");
    }

    {
        digital_logic::Circuit circuit;
        add<digital_logic::Input>(circuit, 1, "A", Signal::High);
        add<digital_logic::NOTGate>(circuit, 2);
        add<digital_logic::Output>(circuit, 3, "Y");
        connect(circuit, 1, 2, 0);
        connect(circuit, 2, 3, 0);
        circuit.evaluate();
        const auto result = circuit.generate_truth_table(1);
        expect(!result.success && has_error(result, "maximum"),
               "max_rows rejects oversized truth table");
        expect(mutable_input(circuit, 1)->value() == Signal::High &&
                   mutable_output(circuit, 3)->value() == Signal::Low,
               "row limit rejection happens before modifying state");
    }
}

void test_zero_input_circuit() {
    digital_logic::Circuit circuit;
    const auto result = circuit.generate_truth_table();
    expect(result.success, "empty zero-input circuit generates successfully");
    expect(result.table.rows.size() == 1,
           "zero-input circuit has exactly one assignment");
    if (result.success && result.table.rows.size() == 1) {
        expect(result.table.rows[0].inputs.empty() &&
                   result.table.rows[0].outputs.empty(),
               "zero-input and zero-output row is empty");
    }
}

} // namespace

int main() {
    test_not_and_and_tables();
    test_compound_table();
    test_order_metadata_and_restore();
    test_errors_and_limits_restore_state();
    test_zero_input_circuit();

    if (failures != 0) {
        std::cerr << failures << " truth-table test(s) failed\n";
        return 1;
    }
    std::cout << "All truth-table tests passed\n";
    return 0;
}
