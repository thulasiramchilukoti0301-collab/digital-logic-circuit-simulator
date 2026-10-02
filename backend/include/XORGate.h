#pragma once

#include "Gate.h"

namespace digital_logic {

class XORGate final : public Gate {
public:
    explicit XORGate(ComponentId id);

    Signal compute(const std::vector<Signal>& inputs) const override;
};

} // namespace digital_logic
