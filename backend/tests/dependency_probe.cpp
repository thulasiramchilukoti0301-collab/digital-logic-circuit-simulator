#include "httplib.h"
#include <nlohmann/json.hpp>

#include <string>

int main() {
    httplib::Server server;
    (void)server;

    const nlohmann::json original = {
        {"project", "digital-logic-circuit-simulator"},
        {"enabled", true},
    };
    const std::string serialized = original.dump();
    const nlohmann::json parsed = nlohmann::json::parse(serialized);

    return parsed == original ? 0 : 1;
}
