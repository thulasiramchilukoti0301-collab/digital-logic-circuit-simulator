#include "ApiRoutes.h"

#include "CircuitRequest.h"
#include "CircuitRepository.h"
#include "httplib.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <system_error>
#include <string>
#include <vector>
#include <cstdlib>
#include <unordered_map>
#include <cmath>
#include <set>

namespace digital_logic::app {

namespace {

using Json = nlohmann::json;

bool has_json_content_type(const httplib::Request& request) {
    std::string content_type = request.get_header_value("Content-Type");
    const std::size_t parameters = content_type.find(';');
    if (parameters != std::string::npos) {
        content_type.resize(parameters);
    }
    const auto not_space = [](unsigned char character) {
        return std::isspace(character) == 0;
    };
    content_type.erase(content_type.begin(),
                       std::find_if(content_type.begin(), content_type.end(), not_space));
    content_type.erase(std::find_if(content_type.rbegin(), content_type.rend(), not_space).base(),
                       content_type.end());
    std::transform(content_type.begin(), content_type.end(), content_type.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return content_type == "application/json";
}

Json errors_json(const std::vector<AdapterError>& errors) {
    Json serialized_errors = Json::array();
    for (const AdapterError& error : errors) {
        serialized_errors.push_back({{"code", error.code}, {"message", error.message}});
    }
    if (serialized_errors.empty()) {
        serialized_errors.push_back({{"code", "invalid_request"},
                                     {"message", "request could not be processed"}});
    }
    return {{"success", false}, {"errors", std::move(serialized_errors)}};
}

void send_json(httplib::Response& response, int status, const Json& body) {
    response.status = status;
    response.set_content(body.dump(), "application/json");
}

CircuitRepository& repository() {
    static CircuitRepository instance([] { const char* p=std::getenv("DIGITAL_LOGIC_DB_PATH"); return p&&*p?std::string(p):std::string("data/circuits.sqlite3"); }());
    return instance;
}
bool persistence_document(const Json& doc, std::vector<AdapterError>& errors) {
    auto parsed=parse_circuit_request_document(doc);
    if(!parsed.success){errors=parsed.errors;return false;}
    std::unordered_map<std::string, Json> components;
    for(const auto& c:doc["components"]){
        const auto id=c["id"].get<std::string>();
        if(!components.emplace(id,c).second){errors.push_back({"duplicate_component_id","component IDs must be unique"});return false;}
        if(!c.contains("name")||!c["name"].is_string()){errors.push_back({"invalid_component","every saved component needs a name"});return false;}
        if(!c.contains("position")||!c["position"].is_object()||!c["position"].contains("x")||!c["position"].contains("y")||!c["position"]["x"].is_number()||!c["position"]["y"].is_number()||!std::isfinite(c["position"]["x"].get<double>())||!std::isfinite(c["position"]["y"].get<double>())){errors.push_back({"invalid_position","component positions must contain finite x and y coordinates"});return false;}
    }
    std::set<std::pair<std::string,std::size_t>> pins;
    for(const auto&w:doc["wires"]){auto s=w["sourceId"].get<std::string>(),d=w["destinationId"].get<std::string>();auto si=components.find(s),di=components.find(d);if(si==components.end()||di==components.end()){errors.push_back({"invalid_reference","wire references a component that does not exist"});return false;}if(s==d){errors.push_back({"invalid_reference","wire cannot connect a component to itself"});return false;}auto st=si->second["type"].get<std::string>(),dt=di->second["type"].get<std::string>();if(st=="output"||dt=="input"){errors.push_back({"invalid_reference","wire source or destination type is invalid"});return false;}std::size_t pin=w["destinationPin"].get<std::size_t>(),count=dt=="output"?1:di->second["inputCount"].get<std::size_t>();if(pin>=count||!pins.emplace(d,pin).second){errors.push_back({"invalid_reference","wire pin is out of range or already connected"});return false;}}
    return true;
}
Json saved_json(const SavedCircuit& c){return {{"id",c.id},{"name",c.name},{"updatedAt",c.updated_at}};}

bool parse_saved_circuit_id(const std::string& text, long long& id) {
    if (text.empty() || !std::all_of(text.begin(), text.end(), [](unsigned char character) {
            return character >= '0' && character <= '9';
        })) return false;
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, id, 10);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

void send_repository_error(httplib::Response& response, int status,
                           const std::string& code, const std::string& message) {
    send_json(response, status, {{"success", false},
        {"errors", Json::array({{{"code", code}, {"message", message}}})}});
}

} // namespace

void register_api_routes(httplib::Server& server) {
    server.Get("/api/circuits", [](const httplib::Request&, httplib::Response& response) {
        const auto result = repository().list();
        if (!result.success) {
            send_repository_error(response, result.status, result.code, result.message);
            return;
        }
        Json circuits = Json::array();
        for (const auto& circuit : result.circuits) circuits.push_back(saved_json(circuit));
        send_json(response, 200, {{"success", true}, {"circuits", circuits}});
    });

    server.Get(R"(/api/circuits/([^/]+))", [](const httplib::Request& request, httplib::Response& response) {
        long long id = 0;
        if (!parse_saved_circuit_id(request.matches[1], id)) {
            send_repository_error(response, 400, "invalid_id", "Circuit ID must be a decimal integer in range.");
            return;
        }
        const auto result = repository().get(id);
        if (!result.success) {
            send_repository_error(response, result.status, result.code, result.message);
            return;
        }
        auto circuit = saved_json(result.circuit);
        circuit["version"] = result.circuit.document["version"];
        circuit["components"] = result.circuit.document["components"];
        circuit["wires"] = result.circuit.document["wires"];
        send_json(response, 200, {{"success", true}, {"circuit", circuit}});
    });

    auto save_handler = [](bool update, const httplib::Request& request, httplib::Response& response) {
        try {
            long long id = 0;
            if (update && !parse_saved_circuit_id(request.matches[1], id)) {
                send_repository_error(response, 400, "invalid_id", "Circuit ID must be a decimal integer in range.");
                return;
            }
            if (!has_json_content_type(request)) {
                send_json(response, 400, errors_json({{"invalid_content_type", "Content-Type must be application/json"}}));
                return;
            }

            const Json body = Json::parse(request.body);
            if (!body.is_object() || !body.contains("name") || !body["name"].is_string() || !body.contains("circuit")) {
                send_json(response, 400, errors_json({{"invalid_request", "name and circuit are required"}}));
                return;
            }
            const auto name = body["name"].get<std::string>();
            if (name.empty() || name.size() > 120 || name.find_first_not_of(" \t\r\n") == std::string::npos) {
                send_json(response, 400, errors_json({{"invalid_name", "name must contain 1 to 120 non-whitespace characters"}}));
                return;
            }

            std::vector<AdapterError> errors;
            if (!persistence_document(body["circuit"], errors)) {
                send_json(response, 400, errors_json(errors));
                return;
            }

            const auto result = update
                ? repository().update(id, name, body["circuit"])
                : repository().create(name, body["circuit"]);
            if (!result.success) {
                send_repository_error(response, result.status, result.code, result.message);
                return;
            }
            send_json(response, update ? 200 : 201,
                      {{"success", true}, {"circuit", saved_json(result.circuit)}});
        } catch (const Json::exception&) {
            send_json(response, 400, errors_json({{"invalid_request", "request could not be processed"}}));
        } catch (...) {
            send_json(response, 500, errors_json({{"internal_error", "An internal server error occurred."}}));
        }
    };
    server.Post("/api/circuits", [save_handler](const httplib::Request& request, httplib::Response& response) {
        save_handler(false, request, response);
    });
    server.Put(R"(/api/circuits/([^/]+))", [save_handler](const httplib::Request& request, httplib::Response& response) {
        save_handler(true, request, response);
    });
    server.Get("/api/health", [](const httplib::Request&, httplib::Response& response) {
        response.set_content(
            R"({"status":"ok","service":"digital_logic_server"})",
            "application/json");
    });

    server.Post("/api/simulate", [](const httplib::Request& request,
                                    httplib::Response& response) {
        try {
            if (!has_json_content_type(request)) {
                send_json(response, 400, errors_json({
                    {"invalid_content_type", "Content-Type must be application/json"}
                }));
                return;
            }

            const CircuitRequestParseResult parsed = parse_circuit_request(request.body);
            if (!parsed.success) {
                send_json(response, 400, errors_json(parsed.errors));
                return;
            }

            CircuitBuildResult built = build_circuit(parsed.request);
            if (!built.success || !built.circuit) {
                send_json(response, 422, errors_json(built.errors));
                return;
            }

            const EvaluationResult evaluation = built.circuit->evaluate();
            const Json body = evaluation_result_to_json(evaluation, *built.circuit);
            send_json(response, evaluation.success && body.value("success", false) ? 200 : 422,
                      body);
        } catch (...) {
            send_json(response, 500, Json{
                {"success", false},
                {"errors", Json::array({{{"code", "internal_error"},
                                         {"message", "An unexpected server error occurred"}}})}
            });
        }
    });

    server.Post("/api/truth-table", [](const httplib::Request& request,
                                       httplib::Response& response) {
        try {
            if (!has_json_content_type(request)) {
                send_json(response, 400, errors_json({
                    {"invalid_content_type", "Content-Type must be application/json"}
                }));
                return;
            }

            const CircuitRequestParseResult parsed = parse_circuit_request(request.body);
            if (!parsed.success) {
                send_json(response, 400, errors_json(parsed.errors));
                return;
            }

            CircuitBuildResult built = build_circuit(parsed.request);
            if (!built.success || !built.circuit) {
                send_json(response, 422, errors_json(built.errors));
                return;
            }

            const TruthTableResult truth_table = built.circuit->generate_truth_table();
            const Json body = truth_table_result_to_json(truth_table);
            send_json(response, truth_table.success && body.value("success", false) ? 200 : 422,
                      body);
        } catch (...) {
            send_json(response, 500, Json{
                {"success", false},
                {"errors", Json::array({{{"code", "internal_error"},
                                         {"message", "An unexpected server error occurred"}}})}
            });
        }
    });

    server.Post("/api/validate", [](const httplib::Request& request,
                                    httplib::Response& response) {
        try {
            if (!has_json_content_type(request)) {
                send_json(response, 400, errors_json({
                    {"invalid_content_type", "Content-Type must be application/json"}
                }));
                return;
            }

            const CircuitRequestParseResult parsed = parse_circuit_request(request.body);
            if (!parsed.success) {
                send_json(response, 400, errors_json(parsed.errors));
                return;
            }

            CircuitBuildResult built = build_circuit(parsed.request);
            if (!built.success || !built.circuit) {
                send_json(response, 422, errors_json(built.errors));
                return;
            }

            const ValidationResult validation = built.circuit->validate();
            if (!validation.is_valid()) {
                std::vector<AdapterError> errors;
                errors.reserve(validation.errors.size());
                for (const std::string& message : validation.errors) {
                    errors.push_back({"invalid_circuit", message});
                }
                send_json(response, 422, errors_json(errors));
                return;
            }

            send_json(response, 200, Json{{"success", true}});
        } catch (...) {
            send_json(response, 500, Json{
                {"success", false},
                {"errors", Json::array({{{"code", "internal_error"},
                                         {"message", "An unexpected server error occurred"}}})}
            });
        }
    });
}

} // namespace digital_logic::app
