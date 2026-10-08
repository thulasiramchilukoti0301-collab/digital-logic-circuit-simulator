#include "ApiRoutes.h"

#include "CircuitRequest.h"
#include "CircuitRepository.h"
#include "httplib.h"

#include <algorithm>
#include <cctype>
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

} // namespace

void register_api_routes(httplib::Server& server) {
    server.Get("/api/circuits", [](const httplib::Request&,httplib::Response& response){Json a=Json::array();for(const auto&c:repository().list())a.push_back(saved_json(c));send_json(response,200,{{"success",true},{"circuits",a}});});
    server.Get(R"(/api/circuits/([0-9]+))", [](const httplib::Request& req,httplib::Response& response){auto r=repository().get(std::stoll(req.matches[1]));if(!r.success){send_json(response,r.status,{{"success",false},{"errors",Json::array({{{"code",r.code},{"message",r.message}}})}});return;}auto c=saved_json(r.circuit);c["version"]=r.circuit.document["version"];c["components"]=r.circuit.document["components"];c["wires"]=r.circuit.document["wires"];send_json(response,200,{{"success",true},{"circuit",c}});});
    auto save_handler=[](bool update,const httplib::Request& req,httplib::Response& response){try{if(!has_json_content_type(req)){send_json(response,400,errors_json({{"invalid_content_type","Content-Type must be application/json"}}));return;}Json body=Json::parse(req.body);if(!body.is_object()||!body.contains("name")||!body["name"].is_string()||!body.contains("circuit")){send_json(response,400,errors_json({{"invalid_request","name and circuit are required"}}));return;}const auto name=body["name"].get<std::string>();if(name.empty()||name.size()>120||name.find_first_not_of(" \t\r\n")==std::string::npos){send_json(response,400,errors_json({{"invalid_name","name must contain 1 to 120 non-whitespace characters"}}));return;}std::vector<AdapterError> errors;if(!persistence_document(body["circuit"],errors)){send_json(response,400,errors_json(errors));return;}long long id=update?std::stoll(req.matches[1]):0;auto result=update?repository().update(id,name,body["circuit"]):repository().create(name,body["circuit"]);if(!result.success){send_json(response,result.status,{{"success",false},{"errors",Json::array({{{"code",result.code},{"message",result.message}}})}});return;}send_json(response,update?200:201,{{"success",true},{"circuit",saved_json(result.circuit)}});}catch(...){send_json(response,400,errors_json({{"invalid_request","request could not be processed"}}));}};
    server.Post("/api/circuits",[save_handler](const httplib::Request&r,httplib::Response&s){save_handler(false,r,s);});
    server.Put(R"(/api/circuits/([0-9]+))",[save_handler](const httplib::Request&r,httplib::Response&s){save_handler(true,r,s);});
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
