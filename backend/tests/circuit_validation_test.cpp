#include "ANDGate.h"
#include "Circuit.h"
#include "Input.h"
#include "NOTGate.h"
#include "Output.h"

#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

void expect_rejected(const std::string& label,
                     const std::function<void()>& operation) {
    try {
        operation();
        std::cerr << "FAIL: " << label << " was not rejected\n";
        ++failures;
    } catch (const std::invalid_argument&) {
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << label << " threw unexpected exception: "
                  << error.what() << '\n';
        ++failures;
    }
}

template <typename ComponentType, typename... Args>
void add(digital_logic::Circuit& circuit, Args&&... args) {
    circuit.add_component(
        std::make_unique<ComponentType>(std::forward<Args>(args)...));
}

} // namespace

int main() {
    {
        digital_logic::Circuit circuit;
        add<digital_logic::Input>(circuit, 1, "A");
        expect_rejected("duplicate component ID", [&] {
            add<digital_logic::Output>(circuit, 1, "Y");
        });
        expect_rejected("null component", [&] {
            circuit.add_component(nullptr);
        });
    }

    {
        digital_logic::Circuit circuit;
        add<digital_logic::Input>(circuit, 1, "A");
        add<digital_logic::ANDGate>(circuit, 2);
        expect_rejected("missing wire source", [&] {
            circuit.add_wire(digital_logic::Wire(99, 2, 0));
        });
        expect_rejected("missing wire destination", [&] {
            circuit.add_wire(digital_logic::Wire(1, 99, 0));
        });
        expect_rejected("invalid destination pin", [&] {
            circuit.add_wire(digital_logic::Wire(1, 2, 2));
        });
    }

    {
        digital_logic::Circuit circuit;
        add<digital_logic::Output>(circuit, 1, "Y");
        add<digital_logic::ANDGate>(circuit, 2);
        expect_rejected("invalid source component type", [&] {
            circuit.add_wire(digital_logic::Wire(1, 2, 0));
        });
    }
    {
        digital_logic::Circuit circuit;
        add<digital_logic::Input>(circuit, 1, "A");
        add<digital_logic::ANDGate>(circuit, 2);
        expect_rejected("invalid destination component type", [&] {
            circuit.add_wire(digital_logic::Wire(2, 1, 0));
        });
    }

    {
        digital_logic::Circuit circuit;
        add<digital_logic::Input>(circuit, 1, "A");
        add<digital_logic::ANDGate>(circuit, 2);
        circuit.add_wire(digital_logic::Wire(1, 2, 0));
        expect_rejected("duplicate destination pin", [&] {
            circuit.add_wire(digital_logic::Wire(1, 2, 0));
        });
        const auto result = circuit.validate();
        expect(!result.is_valid(), "unconnected required gate input is detected");
        expect(!result.errors.empty(), "validation provides error information");
    }

    {
        digital_logic::Circuit circuit;
        add<digital_logic::Input>(circuit, 1, "A");
        add<digital_logic::NOTGate>(circuit, 2);
        add<digital_logic::Output>(circuit, 3, "Y");
        expect_rejected("self-connection", [&] {
            circuit.add_wire(digital_logic::Wire(2, 2, 0));
        });
        const auto result = circuit.validate();
        expect(!result.is_valid(), "unconnected output is detected");
        expect(result.errors.size() >= 2,
               "validation reports both unconnected gate input and output");
    }

    {
        digital_logic::Circuit circuit;
        add<digital_logic::Input>(circuit, 1, "A");
        add<digital_logic::Input>(circuit, 5, "B");
        add<digital_logic::ANDGate>(circuit, 2);
        add<digital_logic::ANDGate>(circuit, 3);
        add<digital_logic::Output>(circuit, 4, "Y");
        circuit.add_wire(digital_logic::Wire(1, 2, 0));
        circuit.add_wire(digital_logic::Wire(5, 3, 0));
        circuit.add_wire(digital_logic::Wire(2, 3, 1));
        circuit.add_wire(digital_logic::Wire(3, 2, 1));
        circuit.add_wire(digital_logic::Wire(3, 4, 0));
        const auto result = circuit.validate();
        expect(!result.is_valid(), "combinational cycle is detected");
        bool cycle_reported = false;
        for (const std::string& error : result.errors) {
            if (error.find("cycle") != std::string::npos) {
                cycle_reported = true;
            }
        }
        expect(cycle_reported, "cycle validation error is descriptive");
    }

    {
        digital_logic::Circuit circuit;
        add<digital_logic::Input>(circuit, 1, "A");
        add<digital_logic::Input>(circuit, 2, "B");
        add<digital_logic::ANDGate>(circuit, 3);
        add<digital_logic::Output>(circuit, 4, "Y");
        circuit.add_wire(digital_logic::Wire(1, 3, 0));
        circuit.add_wire(digital_logic::Wire(2, 3, 1));
        circuit.add_wire(digital_logic::Wire(3, 4, 0));
        const auto result = circuit.validate();
        expect(result.is_valid(), "valid Input-to-AND-to-Output structure passes");
        expect(result.errors.empty(), "valid structure has no validation errors");
    }

    if (failures != 0) {
        std::cerr << failures << " circuit validation test(s) failed\n";
        return 1;
    }
    std::cout << "All circuit validation tests passed\n";
    return 0;
}
