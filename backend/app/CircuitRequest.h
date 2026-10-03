#pragma once

#include "Circuit.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace digital_logic::app {

enum class RequestComponentType {
    Input,
    Output,
    AND,
    OR,
    NOT,
    XOR,
    NAND,
    NOR
};

struct ComponentRequest {
    ComponentId id = 0;
    RequestComponentType type = RequestComponentType::Input;
    std::string name;
    std::optional<Signal> input_value;
    std::optional<std::size_t> input_count;
};

struct WireRequest {
    ComponentId source_id = 0;
    ComponentId destination_id = 0;
    std::size_t destination_pin = 0;
};

struct CircuitRequest {
    std::vector<ComponentRequest> components;
    std::vector<WireRequest> wires;
};

struct AdapterError {
    std::string code;
    std::string message;
};

struct CircuitRequestParseResult {
    bool success = false;
    std::vector<AdapterError> errors;
    CircuitRequest request;
};

struct CircuitBuildResult {
    bool success = false;
    std::vector<AdapterError> errors;
    std::unique_ptr<Circuit> circuit;
};

CircuitRequestParseResult parse_circuit_request(const std::string& json_text);
CircuitRequestParseResult parse_circuit_request_document(const nlohmann::json& document);
CircuitBuildResult build_circuit(const CircuitRequest& request);

nlohmann::json evaluation_result_to_json(const EvaluationResult& result,
                                         const Circuit& circuit);
nlohmann::json truth_table_result_to_json(const TruthTableResult& result);

} // namespace digital_logic::app
