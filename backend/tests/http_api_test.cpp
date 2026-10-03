#include "ApiRoutes.h"

#include "httplib.h"
#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {

using Json = nlohmann::json;
int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class ApiServer final {
public:
    ApiServer() {
        digital_logic::app::register_api_routes(server_);
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) {
            throw std::runtime_error("could not bind test HTTP server");
        }
        worker_ = std::thread([this] { server_.listen_after_bind(); });

        httplib::Client client("127.0.0.1", port_);
        for (int attempt = 0; attempt < 50; ++attempt) {
            const auto response = client.Get("/api/health");
            if (response && response->status == 200) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        server_.stop();
        worker_.join();
        throw std::runtime_error("test HTTP server did not become ready");
    }

    ~ApiServer() {
        server_.stop();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    httplib::Client client() const {
        return httplib::Client("127.0.0.1", port_);
    }

private:
    httplib::Server server_;
    int port_ = 0;
    std::thread worker_;
};

Json and_request() {
    return {
        {"version", 1},
        {"components", Json::array({
            {{"id", "1"}, {"type", "input"}, {"name", "A"}, {"value", 1}},
            {{"id", "2"}, {"type", "input"}, {"name", "B"}, {"value", 0}},
            {{"id", "3"}, {"type", "and"}, {"name", "AND"}, {"inputCount", 2}},
            {{"id", "4"}, {"type", "output"}, {"name", "Result"}}
        })},
        {"wires", Json::array({
            {{"sourceId", "1"}, {"destinationId", "3"}, {"destinationPin", 0}},
            {{"sourceId", "2"}, {"destinationId", "3"}, {"destinationPin", 1}},
            {{"sourceId", "3"}, {"destinationId", "4"}, {"destinationPin", 0}}
        })}
    };
}

void expect_failure_shape(const httplib::Result& response, int status,
                          const std::string& label) {
    expect(static_cast<bool>(response), label + " receives an HTTP response");
    if (!response) return;
    expect(response->status == status, label + " has expected HTTP status");
    expect(response->get_header_value("Content-Type").find("application/json") == 0,
           label + " uses JSON content type");
    try {
        const Json body = Json::parse(response->body);
        expect(body.is_object() && body.value("success", true) == false,
               label + " has success=false");
        expect(body.contains("errors") && body["errors"].is_array() &&
                   !body["errors"].empty() && body["errors"][0].contains("code") &&
                   body["errors"][0].contains("message"),
               label + " has structured errors");
        expect(!body.contains("outputs"), label + " has no partial outputs");
    } catch (const std::exception& error) {
        expect(false, label + " response is valid JSON: " + error.what());
    }
}

void test_health_unchanged(ApiServer& api) {
    auto client = api.client();
    const auto response = client.Get("/api/health");
    expect(response && response->status == 200, "health endpoint remains available");
    if (response) {
        expect(response->get_header_value("Content-Type").find("application/json") == 0,
               "health endpoint retains JSON content type");
        expect(response->body ==
                   R"({"status":"ok","service":"digital_logic_server"})",
               "health endpoint response body is unchanged");
    }
}

void test_successful_simulation(ApiServer& api) {
    auto client = api.client();
    const auto response = client.Post("/api/simulate", and_request().dump(), "application/json");
    expect(response && response->status == 200, "valid simulation returns HTTP 200");
    if (!response) return;
    expect(response->get_header_value("Content-Type").find("application/json") == 0,
           "simulation response uses JSON content type");
    try {
        const Json body = Json::parse(response->body);
        expect(body.value("success", false), "simulation success response has success=true");
        expect(body.contains("outputs") && body["outputs"].is_array() &&
                   body["outputs"].size() == 1,
               "simulation response contains one output");
        if (body.contains("outputs") && body["outputs"].is_array() &&
            body["outputs"].size() == 1) {
            expect(body["outputs"][0]["id"] == "4" &&
                       body["outputs"][0]["name"] == "Result" &&
                       body["outputs"][0]["value"] == 0,
                   "simulation output matches C++ engine result and schema");
        }
    } catch (const std::exception& error) {
        expect(false, std::string("simulation response parses as JSON: ") + error.what());
    }
}

void test_request_errors(ApiServer& api) {
    auto client = api.client();
    expect_failure_shape(client.Post("/api/simulate", "{", "application/json"), 400,
                         "malformed JSON");
    expect_failure_shape(client.Post("/api/simulate", "{}", "application/json"), 400,
                         "malformed request schema");
    expect_failure_shape(client.Post("/api/simulate", and_request().dump(), "text/plain"), 400,
                         "non-JSON content type");

    Json invalid_circuit = {
        {"version", 1},
        {"components", Json::array({
            {{"id", "1"}, {"type", "and"}, {"inputCount", 2}},
            {{"id", "2"}, {"type", "output"}, {"name", "Y"}}
        })},
        {"wires", Json::array()}
    };
    expect_failure_shape(client.Post("/api/simulate", invalid_circuit.dump(), "application/json"),
                         422, "structurally invalid circuit");
}

} // namespace

int main() {
    try {
        ApiServer api;
        test_health_unchanged(api);
        test_successful_simulation(api);
        test_request_errors(api);
    } catch (const std::exception& error) {
        std::cerr << "HTTP API test setup failed: " << error.what() << '\n';
        return 1;
    }

    if (failures != 0) {
        std::cerr << failures << " HTTP API test(s) failed\n";
        return 1;
    }
    std::cout << "HTTP API tests passed\n";
    return 0;
}
