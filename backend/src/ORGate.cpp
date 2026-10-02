#include "ORGate.h"

#include <stdexcept>

namespace digital_logic {

ORGate::ORGate(ComponentId id, std::size_t input_count)
    : Gate(id, input_count) {
    if (input_count < 2) {
        throw std::invalid_argument("OR gate requires at least two inputs");
    }
}

Signal ORGate::compute(const std::vector<Signal>& inputs) const {
    if (inputs.size() != input_count()) {
        throw std::invalid_argument("OR gate received an invalid input count");
    }

    bool has_high_input = false;
    for (const Signal input : inputs) {
        if (input == Signal::High) {
            has_high_input = true;
        } else if (input != Signal::Low) {
            throw std::invalid_argument("OR gate input must be Low or High");
        }
    }
    return has_high_input ? Signal::High : Signal::Low;
}

} // namespace digital_logic
