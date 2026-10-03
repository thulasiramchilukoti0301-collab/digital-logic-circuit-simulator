#include "CircuitRequest.h"

#include "ANDGate.h"
#include "Input.h"
#include "NANDGate.h"
#include "NORGate.h"
#include "NOTGate.h"
#include "ORGate.h"
#include "Output.h"
#include "Wire.h"
#include "XORGate.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace digital_logic::app {

namespace {

using Json = nlohmann::json;

CircuitRequestParseResult parse_failure(std::string code, std::string message) {
    CircuitRequestParseResult result;
    result.errors.push_back({std::move(code), std::move(message)});
    return result;
}

bool is_integer(const Json& value) {
    return value.is_number_integer() || value.is_number_unsigned();
}

bool parse_component_id(const Json& value, ComponentId& id) {
    if (!value.is_string()) {
        return false;
    }
    const std::string text = value.get<std::string>();
    if (text.empty()) {
        return false;
    }
    for (const char character : text) {
        if (character < '0' || character > '9') {
            return false;
        }
    }

    ComponentId parsed = 0;
    const auto conversion = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (conversion.ec != std::errc{} || conversion.ptr != text.data() + text.size()) {
        return false;
    }
    id = parsed;
    return true;
}

bool parse_size(const Json& value, std::size_t& parsed) {
    if (!is_integer(value)) {
        return false;
    }

    std::uint64_t unsigned_value = 0;
    if (value.is_number_unsigned()) {
        unsigned_value = value.get<std::uint64_t>();
    } else {
        const std::int64_t signed_value = value.get<std::int64_t>();
        if (signed_value < 0) {
            return false;
        }
        unsigned_value = static_cast<std::uint64_t>(signed_value);
    }
    if (unsigned_value > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    parsed = static_cast<std::size_t>(unsigned_value);
    return true;
}

bool parse_component_type(const std::string& text, RequestComponentType& type) {
    if (text == "input") type = RequestComponentType::Input;
    else if (text == "output") type = RequestComponentType::Output;
    else if (text == "and") type = RequestComponentType::AND;
    else if (text == "or") type = RequestComponentType::OR;
    else if (text == "not") type = RequestComponentType::NOT;
    else if (text == "xor") type = RequestComponentType::XOR;
    else if (text == "nand") type = RequestComponentType::NAND;
    else if (text == "nor") type = RequestComponentType::NOR;
    else return false;
    return true;
}

bool is_gate(RequestComponentType type) {
    return type != RequestComponentType::Input && type != RequestComponentType::Output;
}

std::size_t required_gate_inputs(RequestComponentType type, std::size_t configured) {
    if (type == RequestComponentType::NOT || type == RequestComponentType::XOR) {
        return type == RequestComponentType::NOT ? 1 : 2;
    }
    return configured;
}

Json error_response(const std::vector<AdapterError>& errors,
                    const std::string& fallback_code,
                    const std::string& fallback_message) {
    Json serialized_errors = Json::array();
    if (errors.empty()) {
        serialized_errors.push_back({{"code", fallback_code}, {"message", fallback_message}});
    } else {
        for (const AdapterError& error : errors) {
            serialized_errors.push_back({{"code", error.code}, {"message", error.message}});
        }
    }
    return {{"success", false}, {"errors", std::move(serialized_errors)}};
}

bool signal_to_json(Signal signal, Json& value) {
    if (signal == Signal::Low) {
        value = 0;
        return true;
    }
    if (signal == Signal::High) {
        value = 1;
        return true;
    }
    return false;
}

} // namespace

CircuitRequestParseResult parse_circuit_request(const std::string& json_text) {
    try {
        return parse_circuit_request_document(Json::parse(json_text));
    } catch (const Json::parse_error& error) {
        return parse_failure("malformed_json", std::string("malformed JSON: ") + error.what());
    }
}

CircuitRequestParseResult parse_circuit_request_document(const Json& document) {
    if (!document.is_object()) {
        return parse_failure("invalid_request", "request must be a JSON object");
    }
    // Unrecognized fields are intentionally ignored to allow additive client metadata.

    const auto version = document.find("version");
    if (version == document.end()) {
        return parse_failure("invalid_request", "version: required field is missing");
    }
    std::size_t parsed_version = 0;
    if (!parse_size(*version, parsed_version)) {
        return parse_failure("invalid_request", "version: expected a non-negative integer");
    }
    if (parsed_version != 1) {
        return parse_failure("unsupported_version", "version: only version 1 is supported");
    }

    const auto components = document.find("components");
    if (components == document.end()) {
        return parse_failure("invalid_request", "components: required field is missing");
    }
    if (!components->is_array()) {
        return parse_failure("invalid_request", "components: expected an array");
    }
    const auto wires = document.find("wires");
    if (wires == document.end()) {
        return parse_failure("invalid_request", "wires: required field is missing");
    }
    if (!wires->is_array()) {
        return parse_failure("invalid_request", "wires: expected an array");
    }

    CircuitRequestParseResult result;
    result.request.components.reserve(components->size());
    for (std::size_t index = 0; index < components->size(); ++index) {
        const Json& json_component = (*components)[index];
        const std::string path = "components[" + std::to_string(index) + "]";
        if (!json_component.is_object()) {
            return parse_failure("invalid_request", path + ": expected an object");
        }

        const auto id_field = json_component.find("id");
        if (id_field == json_component.end()) {
            return parse_failure("invalid_request", path + ".id: required field is missing");
        }
        ComponentId id = 0;
        if (!parse_component_id(*id_field, id)) {
            return parse_failure("invalid_id", path + ".id: expected a decimal uint64 string");
        }

        const auto type_field = json_component.find("type");
        if (type_field == json_component.end()) {
            return parse_failure("invalid_request", path + ".type: required field is missing");
        }
        if (!type_field->is_string()) {
            return parse_failure("invalid_request", path + ".type: expected a string");
        }
        RequestComponentType type{};
        if (!parse_component_type(type_field->get<std::string>(), type)) {
            return parse_failure("unknown_component_type",
                                 path + ".type: unsupported component type '" +
                                     type_field->get<std::string>() + "'");
        }

        ComponentRequest component;
        component.id = id;
        component.type = type;
        const auto name_field = json_component.find("name");
        if (name_field == json_component.end()) {
            if (type == RequestComponentType::Input || type == RequestComponentType::Output) {
                return parse_failure("invalid_request", path + ".name: required field is missing");
            }
        } else {
            if (!name_field->is_string()) {
                return parse_failure("invalid_request", path + ".name: expected a string");
            }
            component.name = name_field->get<std::string>();
        }

        const auto value_field = json_component.find("value");
        if (type == RequestComponentType::Input) {
            if (value_field == json_component.end()) {
                return parse_failure("invalid_request", path + ".value: required field is missing");
            }
            if (!is_integer(*value_field)) {
                return parse_failure("invalid_input_value", path + ".value: expected integer 0 or 1");
            }
            const std::int64_t value = value_field->get<std::int64_t>();
            if (value != 0 && value != 1) {
                return parse_failure("invalid_input_value", path + ".value: expected integer 0 or 1");
            }
            component.input_value = value == 0 ? Signal::Low : Signal::High;
        } else if (value_field != json_component.end()) {
            return parse_failure("invalid_request", path + ".value: value is only allowed for inputs");
        }

        const auto count_field = json_component.find("inputCount");
        if (is_gate(type)) {
            if (count_field == json_component.end()) {
                return parse_failure("invalid_request", path + ".inputCount: required field is missing");
            }
            std::size_t input_count = 0;
            if (!parse_size(*count_field, input_count)) {
                return parse_failure("invalid_gate_input_count",
                                     path + ".inputCount: expected a non-negative integer");
            }
            const std::size_t exact_count = required_gate_inputs(type, input_count);
            if (input_count != exact_count ||
                ((type == RequestComponentType::AND || type == RequestComponentType::OR ||
                  type == RequestComponentType::NAND || type == RequestComponentType::NOR) &&
                 input_count < 2)) {
                return parse_failure("invalid_gate_input_count",
                                     path + ".inputCount: invalid input count for gate type");
            }
            component.input_count = input_count;
        } else if (count_field != json_component.end()) {
            return parse_failure("invalid_request",
                                 path + ".inputCount: field is only allowed for gates");
        }

        const auto position = json_component.find("position");
        if (position != json_component.end()) {
            if (!position->is_object()) {
                return parse_failure("invalid_request", path + ".position: expected an object");
            }
            for (const char* coordinate : {"x", "y"}) {
                const auto coordinate_field = position->find(coordinate);
                if (coordinate_field == position->end() || !coordinate_field->is_number()) {
                    return parse_failure("invalid_request",
                                         path + ".position." + coordinate + ": expected a number");
                }
            }
        }

        result.request.components.push_back(std::move(component));
    }

    result.request.wires.reserve(wires->size());
    for (std::size_t index = 0; index < wires->size(); ++index) {
        const Json& json_wire = (*wires)[index];
        const std::string path = "wires[" + std::to_string(index) + "]";
        if (!json_wire.is_object()) {
            return parse_failure("invalid_request", path + ": expected an object");
        }
        WireRequest wire;
        const auto source = json_wire.find("sourceId");
        if (source == json_wire.end()) {
            return parse_failure("invalid_request", path + ".sourceId: required field is missing");
        }
        if (!parse_component_id(*source, wire.source_id)) {
            return parse_failure("invalid_id", path + ".sourceId: expected a decimal uint64 string");
        }
        const auto destination = json_wire.find("destinationId");
        if (destination == json_wire.end()) {
            return parse_failure("invalid_request",
                                 path + ".destinationId: required field is missing");
        }
        if (!parse_component_id(*destination, wire.destination_id)) {
            return parse_failure("invalid_id",
                                 path + ".destinationId: expected a decimal uint64 string");
        }
        const auto pin = json_wire.find("destinationPin");
        if (pin == json_wire.end()) {
            return parse_failure("invalid_request",
                                 path + ".destinationPin: required field is missing");
        }
        if (!parse_size(*pin, wire.destination_pin)) {
            return parse_failure("invalid_destination_pin",
                                 path + ".destinationPin: expected a non-negative integer");
        }
        result.request.wires.push_back(wire);
    }

    result.success = true;
    return result;
}

CircuitBuildResult build_circuit(const CircuitRequest& request) {
    CircuitBuildResult result;
    auto circuit = std::make_unique<Circuit>();

    for (std::size_t index = 0; index < request.components.size(); ++index) {
        const ComponentRequest& component = request.components[index];
        const std::string path = "components[" + std::to_string(index) + "] id " +
                                 std::to_string(component.id) + ": ";
        try {
            switch (component.type) {
            case RequestComponentType::Input:
                if (!component.input_value) {
                    result.errors.push_back({"invalid_request", path + "input value is missing"});
                    return result;
                }
                circuit->add_component(std::make_unique<Input>(
                    component.id, component.name, *component.input_value));
                break;
            case RequestComponentType::Output:
                circuit->add_component(std::make_unique<Output>(component.id, component.name));
                break;
            case RequestComponentType::AND:
                if (!component.input_count) throw std::invalid_argument("input count is missing");
                circuit->add_component(std::make_unique<ANDGate>(component.id, *component.input_count));
                break;
            case RequestComponentType::OR:
                if (!component.input_count) throw std::invalid_argument("input count is missing");
                circuit->add_component(std::make_unique<ORGate>(component.id, *component.input_count));
                break;
            case RequestComponentType::NOT:
                if (component.input_count != 1) throw std::invalid_argument("NOT gate requires one input");
                circuit->add_component(std::make_unique<NOTGate>(component.id));
                break;
            case RequestComponentType::XOR:
                if (component.input_count != 2) throw std::invalid_argument("XOR gate requires two inputs");
                circuit->add_component(std::make_unique<XORGate>(component.id));
                break;
            case RequestComponentType::NAND:
                if (!component.input_count) throw std::invalid_argument("input count is missing");
                circuit->add_component(std::make_unique<NANDGate>(component.id, *component.input_count));
                break;
            case RequestComponentType::NOR:
                if (!component.input_count) throw std::invalid_argument("input count is missing");
                circuit->add_component(std::make_unique<NORGate>(component.id, *component.input_count));
                break;
            default:
                throw std::invalid_argument("unsupported adapter component type");
            }
        } catch (const std::exception& error) {
            const std::string message = error.what();
            result.errors.push_back({
                message.find("duplicate component ID") != std::string::npos
                    ? "duplicate_component_id"
                    : "invalid_request",
                path + message
            });
            return result;
        }
    }

    for (std::size_t index = 0; index < request.wires.size(); ++index) {
        const WireRequest& wire = request.wires[index];
        try {
            circuit->add_wire(Wire(wire.source_id, wire.destination_id,
                                   wire.destination_pin));
        } catch (const std::exception& error) {
            result.errors.push_back({
                "invalid_connection",
                "wires[" + std::to_string(index) + "] (sourceId " +
                    std::to_string(wire.source_id) + ", destinationId " +
                    std::to_string(wire.destination_id) + "): " + error.what()
            });
            return result;
        }
    }

    const ValidationResult validation = circuit->validate();
    if (!validation.is_valid()) {
        for (const std::string& error : validation.errors) {
            result.errors.push_back({"invalid_circuit", error});
        }
        return result;
    }

    result.success = true;
    result.circuit = std::move(circuit);
    return result;
}

Json evaluation_result_to_json(const EvaluationResult& result, const Circuit& circuit) {
    if (!result.success) {
        std::vector<AdapterError> errors;
        for (const std::string& message : result.errors) {
            errors.push_back({"evaluation_failed", message});
        }
        return error_response(errors, "evaluation_failed", "circuit evaluation failed");
    }

    struct SerializedOutput {
        ComponentId id;
        std::string name;
        Signal value;
    };
    std::vector<SerializedOutput> outputs;
    outputs.reserve(result.outputs.size());
    for (const auto& entry : result.outputs) {
        const auto* output = dynamic_cast<const Output*>(circuit.find_component(entry.first));
        if (output == nullptr || (entry.second != Signal::Low && entry.second != Signal::High)) {
            return error_response({{"invalid_engine_result", "evaluation returned an invalid output"}},
                                  "invalid_engine_result", "evaluation returned an invalid output");
        }
        outputs.push_back({entry.first, output->name(), entry.second});
    }
    for (const auto& component : circuit.components()) {
        if (dynamic_cast<const Output*>(component.get()) != nullptr &&
            result.outputs.find(component->id()) == result.outputs.end()) {
            return error_response({{"invalid_engine_result", "evaluation omitted output " +
                                     std::to_string(component->id())}},
                                  "invalid_engine_result", "evaluation omitted an output");
        }
    }
    std::sort(outputs.begin(), outputs.end(), [](const auto& left, const auto& right) {
        return left.id < right.id;
    });

    Json serialized_outputs = Json::array();
    for (const SerializedOutput& output : outputs) {
        serialized_outputs.push_back({
            {"id", std::to_string(output.id)},
            {"name", output.name},
            {"value", output.value == Signal::High ? 1 : 0}
        });
    }
    return {{"success", true}, {"outputs", std::move(serialized_outputs)}};
}

Json truth_table_result_to_json(const TruthTableResult& result) {
    if (!result.success) {
        std::vector<AdapterError> errors;
        for (const std::string& message : result.errors) {
            errors.push_back({"truth_table_failed", message});
        }
        return error_response(errors, "truth_table_failed", "truth-table generation failed");
    }

    Json input_columns = Json::array();
    Json output_columns = Json::array();
    Json rows = Json::array();
    for (const TruthTableInput& input : result.table.input_columns) {
        input_columns.push_back({{"id", std::to_string(input.id)}, {"name", input.name}});
    }
    for (const TruthTableOutput& output : result.table.output_columns) {
        output_columns.push_back({{"id", std::to_string(output.id)}, {"name", output.name}});
    }
    for (const TruthTableRow& row : result.table.rows) {
        if (row.inputs.size() != result.table.input_columns.size() ||
            row.outputs.size() != result.table.output_columns.size()) {
            return error_response({{"invalid_engine_result", "truth-table row width does not match its columns"}},
                                  "invalid_engine_result", "invalid truth-table row");
        }
        Json input_values = Json::array();
        Json output_values = Json::array();
        for (const Signal signal : row.inputs) {
            Json value;
            if (!signal_to_json(signal, value)) {
                return error_response({{"invalid_engine_result", "truth table contains an Undefined input"}},
                                      "invalid_engine_result", "invalid truth-table signal");
            }
            input_values.push_back(std::move(value));
        }
        for (const Signal signal : row.outputs) {
            Json value;
            if (!signal_to_json(signal, value)) {
                return error_response({{"invalid_engine_result", "truth table contains an Undefined output"}},
                                      "invalid_engine_result", "invalid truth-table signal");
            }
            output_values.push_back(std::move(value));
        }
        rows.push_back({{"inputs", std::move(input_values)}, {"outputs", std::move(output_values)}});
    }
    return {{"success", true},
            {"inputs", std::move(input_columns)},
            {"outputs", std::move(output_columns)},
            {"rows", std::move(rows)}};
}

} // namespace digital_logic::app
