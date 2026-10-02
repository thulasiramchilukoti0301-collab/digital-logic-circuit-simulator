#include "NORGate.h"

#include <stdexcept>

namespace digital_logic {

NORGate::NORGate(ComponentId id, std::size_t input_count)
    : Gate(id, input_count) {
    if (input_count < 2) {
        throw std::invalid_argument("NOR gate requires at least two inputs");
    }
}

Signal NORGate::compute(const std::vector<Signal>& inputs) const {
    if (inputs.size() != input_count()) {
        throw std::invalid_argument("NOR gate received an invalid input count");
    }

    bool all_low = true;
    for (const Signal input : inputs) {
        if (input == Signal::High) {
            all_low = false;
        } else if (input != Signal::Low) {
            throw std::invalid_argument("NOR gate input must be Low or High");
        }
    }
    return all_low ? Signal::High : Signal::Low;
}

} // namespace digital_logic
