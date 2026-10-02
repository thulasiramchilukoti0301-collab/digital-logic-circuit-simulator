#include "NANDGate.h"

#include <stdexcept>

namespace digital_logic {

NANDGate::NANDGate(ComponentId id, std::size_t input_count)
    : Gate(id, input_count) {
    if (input_count < 2) {
        throw std::invalid_argument("NAND gate requires at least two inputs");
    }
}

Signal NANDGate::compute(const std::vector<Signal>& inputs) const {
    if (inputs.size() != input_count()) {
        throw std::invalid_argument("NAND gate received an invalid input count");
    }

    bool all_high = true;
    for (const Signal input : inputs) {
        if (input == Signal::Low) {
            all_high = false;
        } else if (input != Signal::High) {
            throw std::invalid_argument("NAND gate input must be Low or High");
        }
    }
    return all_high ? Signal::Low : Signal::High;
}

} // namespace digital_logic
