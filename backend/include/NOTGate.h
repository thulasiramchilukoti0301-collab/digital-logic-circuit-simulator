#pragma once

#include "Gate.h"

namespace digital_logic {

class NOTGate final : public Gate {
public:
    explicit NOTGate(ComponentId id);

    Signal compute(const std::vector<Signal>& inputs) const override;
};

} // namespace digital_logic
