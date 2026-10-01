#pragma once

#include "Component.h"
#include "Signal.h"

#include <cstddef>
#include <vector>

namespace digital_logic {

class Gate : public Component {
public:
    Gate(ComponentId id, std::size_t input_count) noexcept;
    ~Gate() override = default;

    std::size_t input_count() const noexcept;
    virtual Signal compute(const std::vector<Signal>& inputs) const = 0;

private:
    std::size_t input_count_;
};

} // namespace digital_logic
