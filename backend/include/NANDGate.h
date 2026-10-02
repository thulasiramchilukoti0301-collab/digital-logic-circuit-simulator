#pragma once

#include "Gate.h"

namespace digital_logic {

class NANDGate final : public Gate {
public:
    explicit NANDGate(ComponentId id, std::size_t input_count = 2);

    Signal compute(const std::vector<Signal>& inputs) const override;
};

} // namespace digital_logic
