#pragma once
#include <nlohmann/json.hpp>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

namespace digital_logic::app {
struct SavedCircuit {
    long long id = 0;
    std::string name;
    std::string updated_at;
    nlohmann::json document;
};

struct RepositoryResult {
    bool success = false;
    int status = 500;
    std::string code = "database_error";
    std::string message = "The circuit could not be stored.";
    SavedCircuit circuit{};
};

struct RepositoryListResult {
    bool success = false;
    int status = 500;
    std::string code = "database_error";
    std::string message = "Saved circuits could not be read.";
    std::vector<SavedCircuit> circuits;
};

class CircuitRepository {
public:
    explicit CircuitRepository(std::string path);
 ~CircuitRepository();
    RepositoryResult create(const std::string&, const nlohmann::json&);
    RepositoryResult update(long long, const std::string&, const nlohmann::json&);
    RepositoryResult get(long long);
    RepositoryListResult list();
    const std::string& error() const { return error_; }
private:
    struct Connection;
    bool initialize();
    RepositoryResult save(bool, long long, const std::string&, const nlohmann::json&);
    std::string path_;
    std::string error_;
    std::unique_ptr<Connection> db_;
    mutable std::mutex mutex_;
};
}
