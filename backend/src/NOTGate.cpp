#include "NOTGate.h"

#include <stdexcept>

namespace digital_logic {

NOTGate::NOTGate(ComponentId id) : Gate(id, 1) {}

Signal NOTGate::compute(const std::vector<Signal>& inputs) const {
    if (inputs.size() != input_count()) {
        throw std::invalid_argument("NOT gate requires exactly one input");
    }

    if (inputs.front() == Signal::Low) {
        return Signal::High;
    }
    if (inputs.front() == Signal::High) {
        return Signal::Low;
    }
    throw std::invalid_argument("NOT gate input must be Low or High");
}

} // namespace digital_logic
