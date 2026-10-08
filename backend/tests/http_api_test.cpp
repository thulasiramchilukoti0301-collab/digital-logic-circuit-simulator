#include "ApiRoutes.h"

#include "httplib.h"
#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <filesystem>
#include <cstdlib>

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
        expect(!body.contains("outputs") && !body.contains("inputs") && !body.contains("rows"),
               label + " has no partial simulation or truth-table data");
    } catch (const std::exception& error) {
        expect(false, label + " response is valid JSON: " + error.what());
    }
}

void test_persistence_api(ApiServer& api) {
    auto client=api.client();
    Json unfinished={{"version",1},{"components",Json::array({{{"id","31"},{"type","and"},{"name","Open AND"},{"inputCount",2},{"position",{{"x",3.5},{"y",8}}}}})},{"wires",Json::array()}};
    auto created=client.Post("/api/circuits",Json{{"name","Unfinished"},{"circuit",unfinished}}.dump(),"application/json");
    expect(created&&created->status==201,"unfinished circuit can be saved"); if(!created)return;
    auto body=Json::parse(created->body);auto id=body["circuit"]["id"].get<long long>();
    auto opened=client.Get("/api/circuits/"+std::to_string(id));expect(opened&&opened->status==200,"saved circuit can be opened");if(opened)expect(Json::parse(opened->body)["circuit"]["components"]==unfinished["components"],"open restores component data");
    auto duplicate=client.Post("/api/circuits",Json{{"name","Unfinished"},{"circuit",unfinished}}.dump(),"application/json");expect(duplicate&&duplicate->status==409,"duplicate name returns conflict");
    auto bad=unfinished;bad["wires"].push_back({{"sourceId","missing"},{"destinationId","31"},{"destinationPin",0}});
    auto invalid=client.Post("/api/circuits",Json{{"name","Bad"},{"circuit",bad}}.dump(),"application/json");expect(invalid&&invalid->status==400,"invalid wire reference is rejected");
    auto update=client.Put("/api/circuits/"+std::to_string(id),Json{{"name","Updated"},{"circuit",unfinished}}.dump(),"application/json");expect(update&&update->status==200,"explicit update succeeds");
    auto missing=client.Put("/api/circuits/999999",Json{{"name","Missing"},{"circuit",unfinished}}.dump(),"application/json");expect(missing&&missing->status==404,"update missing ID returns not found");
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

void test_successful_truth_table(ApiServer& api) {
    auto client = api.client();
    const auto response = client.Post("/api/truth-table", and_request().dump(),
                                      "application/json");
    expect(response && response->status == 200, "valid truth-table request returns HTTP 200");
    if (!response) return;
    expect(response->get_header_value("Content-Type").find("application/json") == 0,
           "truth-table response uses JSON content type");
    try {
        const Json body = Json::parse(response->body);
        expect(body.value("success", false), "truth-table success response has success=true");
        expect(body.contains("inputs") && body["inputs"].is_array() &&
                   body["inputs"].size() == 2,
               "truth-table response has two input columns");
        expect(body.contains("outputs") && body["outputs"].is_array() &&
                   body["outputs"].size() == 1,
               "truth-table response has one output column");
        if (body.contains("inputs") && body["inputs"].is_array() &&
            body["inputs"].size() == 2 && body.contains("outputs") &&
            body["outputs"].is_array() && body["outputs"].size() == 1) {
            expect(body["inputs"][0]["id"] == "1" && body["inputs"][0]["name"] == "A" &&
                       body["outputs"][0]["id"] == "4" &&
                       body["outputs"][0]["name"] == "Result",
                   "truth-table columns preserve component IDs and names");
        }
        expect(body.contains("rows") && body["rows"].is_array() && body["rows"].size() == 4,
               "two-input AND truth table contains four rows");
        if (body.contains("rows") && body["rows"].is_array() && body["rows"].size() == 4) {
            const int expected_inputs[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
            const int expected_outputs[4] = {0, 0, 0, 1};
            for (std::size_t row = 0; row < 4; ++row) {
                expect(body["rows"][row]["inputs"].size() == 2 &&
                           body["rows"][row]["outputs"].size() == 1,
                       "each truth-table row has the expected vector widths");
                if (body["rows"][row]["inputs"].size() == 2 &&
                    body["rows"][row]["outputs"].size() == 1) {
                    expect(body["rows"][row]["inputs"][0] == expected_inputs[row][0] &&
                               body["rows"][row]["inputs"][1] == expected_inputs[row][1] &&
                               body["rows"][row]["outputs"][0] == expected_outputs[row],
                           "AND row values and binary ordering match C++ truth table");
                }
            }
        }
    } catch (const std::exception& error) {
        expect(false, std::string("truth-table response parses as JSON: ") + error.what());
    }
}

void test_truth_table_errors(ApiServer& api) {
    auto client = api.client();
    expect_failure_shape(client.Post("/api/truth-table", "{", "application/json"), 400,
                         "truth-table malformed JSON");
    expect_failure_shape(client.Post("/api/truth-table", "{}", "application/json"), 400,
                         "truth-table malformed request schema");
    expect_failure_shape(client.Post("/api/truth-table", and_request().dump(), "text/plain"),
                         400, "truth-table non-JSON content type");

    Json invalid_circuit = {
        {"version", 1},
        {"components", Json::array({
            {{"id", "1"}, {"type", "and"}, {"inputCount", 2}},
            {{"id", "2"}, {"type", "output"}, {"name", "Y"}}
        })},
        {"wires", Json::array()}
    };
    expect_failure_shape(client.Post("/api/truth-table", invalid_circuit.dump(),
                                     "application/json"),
                         422, "truth-table structurally invalid circuit");

    Json excessive_table = {{"version", 1}, {"components", Json::array()},
                            {"wires", Json::array({
                                {{"sourceId", "1"}, {"destinationId", "100"},
                                 {"destinationPin", 0}}
                            })}};
    for (int id = 1; id <= 13; ++id) {
        excessive_table["components"].push_back({
            {"id", std::to_string(id)}, {"type", "input"},
            {"name", "I" + std::to_string(id)}, {"value", 0}
        });
    }
    excessive_table["components"].push_back(
        {{"id", "100"}, {"type", "output"}, {"name", "Y"}});
    expect_failure_shape(client.Post("/api/truth-table", excessive_table.dump(),
                                     "application/json"),
                         422, "truth-table exceeds engine default row limit");
}

void test_validation_endpoint(ApiServer& api) {
    auto client = api.client();
    auto response = client.Post("/api/validate", and_request().dump(), "application/json");
    expect(response && response->status == 200, "valid circuit returns HTTP 200 from validate");
    if (response) {
        try {
            const Json body = Json::parse(response->body);
            expect(body.is_object() && body.size() == 1 && body.value("success", false),
                   "valid circuit response is exactly success=true without simulation data");
        } catch (const std::exception& error) {
            expect(false, std::string("valid circuit response parses as JSON: ") + error.what());
        }
    }

    expect_failure_shape(client.Post("/api/validate", "{", "application/json"), 400,
                         "validate malformed JSON");
    expect_failure_shape(client.Post("/api/validate", "{}", "application/json"), 400,
                         "validate malformed schema");
    expect_failure_shape(client.Post("/api/validate"), 400,
                         "validate missing content type");
    expect_failure_shape(client.Post("/api/validate", and_request().dump(), "text/plain"), 400,
                         "validate non-JSON content type");

    Json invalid_circuit = {
        {"version", 1},
        {"components", Json::array({
            {{"id", "1"}, {"type", "and"}, {"inputCount", 2}},
            {{"id", "2"}, {"type", "output"}, {"name", "Y"}}
        })},
        {"wires", Json::array()}
    };
    response = client.Post("/api/validate", invalid_circuit.dump(), "application/json");
    expect_failure_shape(response, 422, "validate structurally invalid circuit");
    if (response) {
        try {
            const Json body = Json::parse(response->body);
            expect(body["errors"][0]["code"] == "invalid_circuit",
                   "invalid circuit uses the structured invalid_circuit code");
        } catch (const std::exception& error) {
            expect(false, std::string("validation error response parses as JSON: ") + error.what());
        }
    }
}

} // namespace

int main() {
    try {
        const auto db=(std::filesystem::temp_directory_path()/"dls_http_test.sqlite3").string();
        std::filesystem::remove(db);
#ifdef _WIN32
        _putenv_s("DIGITAL_LOGIC_DB_PATH",db.c_str());
#else
        setenv("DIGITAL_LOGIC_DB_PATH",db.c_str(),1);
#endif
        ApiServer api;
        test_persistence_api(api);
        test_health_unchanged(api);
        test_successful_simulation(api);
        test_request_errors(api);
        test_successful_truth_table(api);
        test_truth_table_errors(api);
        test_validation_endpoint(api);
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
