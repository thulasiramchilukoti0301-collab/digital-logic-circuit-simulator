#pragma once

#include "Component.h"
#include "Signal.h"

#include <string>

namespace digital_logic {

class Input final : public Component {
public:
    Input(ComponentId id, std::string name, Signal value = Signal::Low);

    const std::string& name() const noexcept;
    Signal value() const noexcept;
    void set_value(Signal value);

private:
    std::string name_;
    Signal value_;
};

} // namespace digital_logic
