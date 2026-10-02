#include "Input.h"

#include <stdexcept>
#include <utility>

namespace digital_logic {

Input::Input(ComponentId id, std::string name, Signal value)
    : Component(id), name_(std::move(name)), value_(value) {
    if (value != Signal::Low && value != Signal::High) {
        throw std::invalid_argument("input value must be Low or High");
    }
}

const std::string& Input::name() const noexcept {
    return name_;
}

Signal Input::value() const noexcept {
    return value_;
}

void Input::set_value(Signal value) {
    if (value != Signal::Low && value != Signal::High) {
        throw std::invalid_argument("input value must be Low or High");
    }
    value_ = value;
}

} // namespace digital_logic
