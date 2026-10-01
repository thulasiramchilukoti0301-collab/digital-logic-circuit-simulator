#pragma once

#include "Component.h"
#include "Signal.h"

#include <string>

namespace digital_logic {

class Output final : public Component {
public:
    Output(ComponentId id, std::string name, Signal value = Signal::Undefined);

    const std::string& name() const noexcept;
    Signal value() const noexcept;
    void set_value(Signal value) noexcept;

private:
    std::string name_;
    Signal value_;
};

} // namespace digital_logic
