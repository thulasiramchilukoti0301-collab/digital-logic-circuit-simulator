#include "Output.h"

#include <utility>

namespace digital_logic {

Output::Output(ComponentId id, std::string name, Signal value)
    : Component(id), name_(std::move(name)), value_(value) {}

const std::string& Output::name() const noexcept {
    return name_;
}

Signal Output::value() const noexcept {
    return value_;
}

void Output::set_value(Signal value) noexcept {
    value_ = value;
}

} // namespace digital_logic
