#pragma once
#include <nlohmann/json.hpp>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace digital_logic::app {
struct SavedCircuit { long long id; std::string name; std::string updated_at; nlohmann::json document; };
struct RepositoryResult { bool success=false; int status=500; std::string code="database_error"; std::string message="The circuit could not be stored."; SavedCircuit circuit{}; };
class CircuitRepository {
public:
 explicit CircuitRepository(std::string path);
 ~CircuitRepository();
 RepositoryResult create(const std::string&, const nlohmann::json&);
 RepositoryResult update(long long, const std::string&, const nlohmann::json&);
 RepositoryResult get(long long);
 std::vector<SavedCircuit> list();
 const std::string& error() const { return error_; }
private:
 bool initialize();
 RepositoryResult save(bool, long long, const std::string&, const nlohmann::json&);
 std::string path_, error_; void* db_=nullptr; mutable std::mutex mutex_;
};
}
