#include "XORGate.h"

#include <stdexcept>

namespace digital_logic {

XORGate::XORGate(ComponentId id) : Gate(id, 2) {}

Signal XORGate::compute(const std::vector<Signal>& inputs) const {
    if (inputs.size() != input_count()) {
        throw std::invalid_argument("XOR gate requires exactly two inputs");
    }

    for (const Signal input : inputs) {
        if (input != Signal::Low && input != Signal::High) {
            throw std::invalid_argument("XOR gate input must be Low or High");
        }
    }

    return inputs[0] != inputs[1] ? Signal::High : Signal::Low;
}

} // namespace digital_logic
