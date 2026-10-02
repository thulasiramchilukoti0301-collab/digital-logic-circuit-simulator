#pragma once

#include "Component.h"
#include "Signal.h"

#include <string>

namespace digital_logic {

class Circuit;

class Output final : public Component {
public:
    Output(ComponentId id, std::string name);

    const std::string& name() const noexcept;
    Signal value() const noexcept;

private:
    friend class Circuit;

    void set_value(Signal value) noexcept;

    std::string name_;
    Signal value_ = Signal::Undefined;
};

} // namespace digital_logic
