#include "Circuit.h"
#include "Input.h"
#include "Output.h"

#include <memory>

int main() {
    digital_logic::Circuit circuit;
    circuit.add_component(
        std::make_unique<digital_logic::Input>(1, "A", digital_logic::Signal::High));
    circuit.add_component(
        std::make_unique<digital_logic::Output>(2, "Result"));
    circuit.add_wire(digital_logic::Wire(1, 2, 0));

    return circuit.components().size() == 2 && circuit.wires().size() == 1
        ? 0
        : 1;
}
