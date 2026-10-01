#include "Input.h"

#include <utility>

namespace digital_logic {

Input::Input(ComponentId id, std::string name, Signal value)
    : Component(id), name_(std::move(name)), value_(value) {}

const std::string& Input::name() const noexcept {
    return name_;
}

Signal Input::value() const noexcept {
    return value_;
}

void Input::set_value(Signal value) noexcept {
    value_ = value;
}

} // namespace digital_logic
