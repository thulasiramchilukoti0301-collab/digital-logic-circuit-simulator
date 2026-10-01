#include "Component.h"

namespace digital_logic {

Component::Component(ComponentId id) noexcept : id_(id) {}

ComponentId Component::id() const noexcept {
    return id_;
}

} // namespace digital_logic
