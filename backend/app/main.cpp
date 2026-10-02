#include "httplib.h"

#include <iostream>

int main() {
    httplib::Server server;

    server.Get("/api/health", [](const httplib::Request&, httplib::Response& response) {
        response.set_content(
            R"({"status":"ok","service":"digital_logic_server"})",
            "application/json");
    });

    std::cout << "Starting digital_logic_server at http://127.0.0.1:8080\n";
    if (!server.listen("127.0.0.1", 8080)) {
        std::cerr << "Failed to start digital_logic_server on 127.0.0.1:8080\n";
        return 1;
    }

    return 0;
}
