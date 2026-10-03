#include "ApiRoutes.h"

#include "CircuitRequest.h"
#include "httplib.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

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

} // namespace

void register_api_routes(httplib::Server& server) {
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
}

} // namespace digital_logic::app
