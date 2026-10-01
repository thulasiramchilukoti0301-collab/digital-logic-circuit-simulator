#pragma once

#include <cstdint>

namespace digital_logic {

using ComponentId = std::uint64_t;

class Component {
public:
    explicit Component(ComponentId id) noexcept;
    virtual ~Component() = default;

    ComponentId id() const noexcept;

private:
    ComponentId id_;
};

} // namespace digital_logic
