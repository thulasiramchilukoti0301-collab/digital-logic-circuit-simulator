#include "CircuitRequest.h"

#include "Gate.h"
#include "Input.h"
#include "Output.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

namespace {

using digital_logic::Signal;
using digital_logic::app::CircuitRequest;
using digital_logic::app::RequestComponentType;
using Json = nlohmann::json;

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

Json basic_request() {
    return {
        {"version", 1},
        {"components", Json::array({
            {{"id", "1"}, {"type", "input"}, {"name", "A"}, {"value", 0}},
            {{"id", "2"}, {"type", "output"}, {"name", "Result"}}
        })},
        {"wires", Json::array({
            {{"sourceId", "1"}, {"destinationId", "2"}, {"destinationPin", 0}}
        })}
    };
}

digital_logic::app::CircuitRequestParseResult parse(const Json& json) {
    return digital_logic::app::parse_circuit_request_document(json);
}

void test_all_component_types_and_gate_arity() {
    Json request = {
        {"version", 1},
        {"components", Json::array({
            {{"id", "1"}, {"type", "input"}, {"name", "A"}, {"value", 0},
             {"position", {{"x", 3.5}, {"y", 8}}}},
            {{"id", "2"}, {"type", "input"}, {"name", "B"}, {"value", 1}},
            {{"id", "3"}, {"type", "input"}, {"name", "C"}, {"value", 0}},
            {{"id", "4"}, {"type", "output"}, {"name", "Result"}},
            {{"id", "10"}, {"type", "and"}, {"inputCount", 2}, {"name", "AND"}},
            {{"id", "11"}, {"type", "or"}, {"inputCount", 3}},
            {{"id", "12"}, {"type", "not"}, {"inputCount", 1}},
            {{"id", "13"}, {"type", "xor"}, {"inputCount", 2}},
            {{"id", "14"}, {"type", "nand"}, {"inputCount", 3}},
            {{"id", "15"}, {"type", "nor"}, {"inputCount", 2}}
        })},
        {"wires", Json::array()}
    };
    auto& wires = request["wires"];
    const auto connect_inputs = [&wires](int gate_id, int count) {
        for (int pin = 0; pin < count; ++pin) {
            wires.push_back({{"sourceId", std::to_string((pin % 3) + 1)},
                             {"destinationId", std::to_string(gate_id)},
                             {"destinationPin", pin}});
        }
    };
    connect_inputs(10, 2);
    connect_inputs(11, 3);
    connect_inputs(12, 1);
    connect_inputs(13, 2);
    connect_inputs(14, 3);
    connect_inputs(15, 2);
    wires.push_back({{"sourceId", "10"}, {"destinationId", "4"}, {"destinationPin", 0}});

    const auto parsed = parse(request);
    expect(parsed.success, "all supported types and configured gate arities parse");
    if (!parsed.success) return;
    expect(parsed.request.components.size() == 10, "component metadata count is preserved");
    expect(parsed.request.components[0].input_value == Signal::Low, "input 0 maps to Low");
    expect(parsed.request.components[1].input_value == Signal::High, "input 1 maps to High");
    expect(parsed.request.components[4].name == "AND", "provided gate name is preserved");
    expect(parsed.request.components[5].name.empty(), "omitted gate name defaults to empty");
    expect(!parsed.request.components[0].input_count, "frontend position is not retained");
    expect(parsed.request.components[4].type == RequestComponentType::AND &&
           parsed.request.components[5].type == RequestComponentType::OR &&
           parsed.request.components[6].type == RequestComponentType::NOT &&
           parsed.request.components[7].type == RequestComponentType::XOR &&
           parsed.request.components[8].type == RequestComponentType::NAND &&
           parsed.request.components[9].type == RequestComponentType::NOR,
           "each gate type maps to its adapter type");

    auto built = digital_logic::app::build_circuit(parsed.request);
    expect(built.success && built.circuit != nullptr, "all supported component types build into Circuit");
    if (built.circuit) {
        expect(dynamic_cast<const digital_logic::Input*>(built.circuit->find_component(1)) != nullptr,
               "input becomes engine Input");
        expect(dynamic_cast<const digital_logic::Output*>(built.circuit->find_component(4)) != nullptr,
               "output becomes engine Output");
        for (digital_logic::ComponentId id = 10; id <= 15; ++id) {
            expect(dynamic_cast<const digital_logic::Gate*>(built.circuit->find_component(id)) != nullptr,
                   "gate type becomes a polymorphic engine Gate");
        }
        const auto* output = dynamic_cast<const digital_logic::Output*>(built.circuit->find_component(4));
        expect(output != nullptr && output->name() == "Result", "component name is preserved in engine");
    }
}

void test_component_ids() {
    Json request = basic_request();
    request["components"][0]["id"] = "18446744073709551615";
    request["wires"][0]["sourceId"] = "18446744073709551615";
    const auto max_id = parse(request);
    expect(max_id.success && max_id.request.components[0].id ==
               std::numeric_limits<std::uint64_t>::max(),
           "maximum uint64 decimal ID parses without numeric JSON conversion");

    for (const std::string& invalid : {"", "-1", "+1", " 1", "1x",
                                        "18446744073709551616"}) {
        request = basic_request();
        request["components"][0]["id"] = invalid;
        const auto parsed = parse(request);
        expect(!parsed.success && !parsed.errors.empty() &&
                   parsed.errors.front().code == "invalid_id",
               "invalid decimal ID is rejected: '" + invalid + "'");
    }
    request = basic_request();
    request["components"][0]["id"] = 1;
    expect(!parse(request).success, "numeric JSON component ID is rejected");
}

void test_schema_errors_and_gate_counts() {
    Json request = basic_request();
    request["components"][0]["value"] = 1.0;
    expect(!parse(request).success, "floating input value is rejected");
    request["components"][0]["value"] = true;
    expect(!parse(request).success, "boolean input value is rejected");
    request["components"][0]["value"] = 2;
    expect(!parse(request).success, "input value outside 0/1 is rejected");
    request["components"][0]["value"] = -1;
    expect(!parse(request).success, "negative input value is rejected");

    const auto expect_gate_count = [](const std::string& type, int count, bool expected) {
        Json doc = {{"version", 1},
                    {"components", Json::array({{{"id", "1"}, {"type", type},
                                                  {"inputCount", count}}})},
                    {"wires", Json::array()}};
        const auto result = parse(doc);
        expect(result.success == expected, type + " inputCount=" + std::to_string(count) +
                                               (expected ? " accepted" : " rejected"));
    };
    expect_gate_count("and", 2, true);
    expect_gate_count("and", 3, true);
    expect_gate_count("or", 2, true);
    expect_gate_count("nand", 4, true);
    expect_gate_count("nor", 2, true);
    expect_gate_count("and", 1, false);
    expect_gate_count("or", 0, false);
    expect_gate_count("nand", 1, false);
    expect_gate_count("nor", 1, false);
    expect_gate_count("xor", 2, true);
    expect_gate_count("xor", 3, false);
    expect_gate_count("not", 1, true);
    expect_gate_count("not", 2, false);
    expect_gate_count("not", -1, false);

    request = basic_request();
    request["components"][0]["type"] = "mux";
    expect(!parse(request).success, "unknown component type is rejected");
    request = basic_request();
    request["components"][0].erase("name");
    expect(!parse(request).success, "missing required input name is rejected");
    request = basic_request();
    request["components"][0]["name"] = 9;
    expect(!parse(request).success, "wrong name type is rejected");
    request = basic_request();
    request["components"][0]["inputCount"] = 2;
    expect(!parse(request).success, "inputCount on an Input is rejected");
    request = basic_request();
    request["components"][1]["value"] = 0;
    expect(!parse(request).success, "simulation value on Output is rejected");
}

void test_top_level_and_wire_errors() {
    expect(!digital_logic::app::parse_circuit_request("{").success,
           "malformed JSON text is rejected");
    expect(!parse(Json::array()).success, "non-object request is rejected");
    Json request = basic_request();
    request.erase("version");
    expect(!parse(request).success, "missing version is rejected");
    request = basic_request();
    request["version"] = 2;
    expect(!parse(request).success, "unsupported version is rejected");
    request = basic_request();
    request.erase("components");
    expect(!parse(request).success, "missing components is rejected");
    request = basic_request();
    request["wires"] = "bad";
    expect(!parse(request).success, "wrong wires type is rejected");

    request = basic_request();
    request["wires"][0]["sourceId"] = 4;
    expect(!parse(request).success, "wire numeric ID instead of string is rejected");
    request = basic_request();
    request["wires"][0].erase("destinationPin");
    expect(!parse(request).success, "wire missing destinationPin is rejected");
    request = basic_request();
    request["wires"][0]["destinationPin"] = -1;
    expect(!parse(request).success, "negative destinationPin is rejected");
    request = basic_request();
    request["components"][0]["futureClientField"] = "ignored";
    expect(parse(request).success, "unrecognized additive fields are ignored");
    request = basic_request();
    request["components"][0]["position"] = {{"x", true}, {"y", 2}};
    expect(!parse(request).success, "frontend position coordinates must be numeric");

    request = basic_request();
    auto parsed = parse(request);
    expect(parsed.success && parsed.request.wires.size() == 1 &&
               parsed.request.wires[0].source_id == 1 &&
               parsed.request.wires[0].destination_id == 2 &&
               parsed.request.wires[0].destination_pin == 0,
           "wire IDs and pin convert to engine-sized values");
    auto built = digital_logic::app::build_circuit(parsed.request);
    expect(built.success && built.circuit && built.circuit->wires().size() == 1,
           "valid wire is added to Circuit");
}

void test_duplicate_ids_and_graph_errors() {
    Json request = basic_request();
    request["components"][1]["id"] = "1";
    auto parsed = parse(request);
    expect(parsed.success, "duplicate IDs remain a Circuit construction concern");
    if (parsed.success) {
        const auto built = digital_logic::app::build_circuit(parsed.request);
        expect(!built.success && !built.circuit && !built.errors.empty() &&
                   built.errors.front().code == "duplicate_component_id" &&
                   built.errors.front().message.find("components[1]") != std::string::npos,
               "Circuit duplicate-ID exception maps to contextual adapter error");
    }

    request = basic_request();
    request["wires"][0]["destinationPin"] = 1;
    parsed = parse(request);
    const auto invalid_pin = digital_logic::app::build_circuit(parsed.request);
    expect(!invalid_pin.success && invalid_pin.errors.front().code == "invalid_connection" &&
               invalid_pin.errors.front().message.find("wires[0]") != std::string::npos,
           "Circuit connection rules reject an invalid output pin with wire context");

    request = basic_request();
    request["wires"][0]["sourceId"] = "99";
    parsed = parse(request);
    const auto missing_source = digital_logic::app::build_circuit(parsed.request);
    expect(!missing_source.success && !missing_source.errors.empty() &&
               missing_source.errors.front().code == "invalid_connection" &&
               missing_source.errors.front().message.find("wires[0]") != std::string::npos,
           "Circuit connection rules report missing wire endpoint with context");

    request = basic_request();
    request["wires"] = Json::array();
    parsed = parse(request);
    const auto disconnected = digital_logic::app::build_circuit(parsed.request);
    expect(!disconnected.success && !disconnected.errors.empty() &&
               disconnected.errors.front().code == "invalid_circuit",
           "Circuit validation remains authoritative for disconnected output");
}

void test_result_serialization() {
    digital_logic::Circuit circuit;
    circuit.add_component(std::make_unique<digital_logic::Output>(3, "Result"));
    digital_logic::EvaluationResult evaluation;
    evaluation.success = true;
    evaluation.outputs.emplace(3, Signal::High);
    Json response = digital_logic::app::evaluation_result_to_json(evaluation, circuit);
    expect(response.value("success", false) && response.contains("outputs") &&
               response["outputs"].size() == 1 &&
               response["outputs"][0]["id"] == "3" &&
               response["outputs"][0]["name"] == "Result" &&
               response["outputs"][0]["value"] == 1,
           "EvaluationResult serializes output ID, name and binary value");

    evaluation.success = false;
    evaluation.errors = {"preserved engine failure"};
    evaluation.outputs.emplace(999, Signal::Low);
    response = digital_logic::app::evaluation_result_to_json(evaluation, circuit);
    expect(!response.value("success", true) && !response.contains("outputs") &&
               response["errors"][0]["message"] == "preserved engine failure",
           "failed evaluation response preserves errors without partial outputs");

    digital_logic::TruthTableResult truth_table;
    truth_table.success = true;
    truth_table.table.input_columns.push_back({1, "A"});
    truth_table.table.output_columns.push_back({3, "Result"});
    truth_table.table.rows.push_back({{Signal::Low}, {Signal::High}});
    response = digital_logic::app::truth_table_result_to_json(truth_table);
    expect(response.value("success", false) && response["inputs"][0]["id"] == "1" &&
               response["outputs"][0]["name"] == "Result" &&
               response["rows"][0]["inputs"][0] == 0 &&
               response["rows"][0]["outputs"][0] == 1,
           "TruthTableResult serializes metadata and rows");

    truth_table.success = false;
    truth_table.errors = {"truth table engine failure"};
    response = digital_logic::app::truth_table_result_to_json(truth_table);
    expect(!response.value("success", true) && !response.contains("inputs") &&
               !response.contains("outputs") && !response.contains("rows") &&
               response["errors"][0]["message"] == "truth table engine failure",
           "failed truth table response has no partial data and preserves error");
}

} // namespace

int main() {
    test_all_component_types_and_gate_arity();
    test_component_ids();
    test_schema_errors_and_gate_counts();
    test_top_level_and_wire_errors();
    test_duplicate_ids_and_graph_errors();
    test_result_serialization();

    if (failures != 0) {
        std::cerr << failures << " circuit request test(s) failed\n";
        return 1;
    }
    std::cout << "Circuit request adapter tests passed\n";
    return 0;
}
