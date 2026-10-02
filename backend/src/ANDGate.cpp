#include "ANDGate.h"

#include <stdexcept>

namespace digital_logic {

ANDGate::ANDGate(ComponentId id, std::size_t input_count)
    : Gate(id, input_count) {
    if (input_count < 2) {
        throw std::invalid_argument("AND gate requires at least two inputs");
    }
}

Signal ANDGate::compute(const std::vector<Signal>& inputs) const {
    if (inputs.size() != input_count()) {
        throw std::invalid_argument("AND gate received an invalid input count");
    }

    bool has_low_input = false;
    for (const Signal input : inputs) {
        if (input == Signal::Low) {
            has_low_input = true;
        } else if (input != Signal::High) {
            throw std::invalid_argument("AND gate input must be Low or High");
        }
    }
    return has_low_input ? Signal::Low : Signal::High;
}

} // namespace digital_logic
