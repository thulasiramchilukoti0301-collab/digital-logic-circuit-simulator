#include "CircuitRepository.h"

#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace digital_logic::app;

namespace {
int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

nlohmann::json unfinished_circuit() {
    return {
        {"version", 1},
        {"components", nlohmann::json::array({
            {{"id", "9"}, {"type", "and"}, {"name", "Gate"}, {"inputCount", 2},
             {"position", {{"x", 4.5}, {"y", 7}}}}
        })},
        {"wires", nlohmann::json::array()}
    };
}

void test_save_load_update(const std::filesystem::path& path) {
    const auto unfinished = unfinished_circuit();
    {
        CircuitRepository repository(path.string());
        const auto created = repository.create("Open gate", unfinished);
        expect(created.success, "unfinished circuit saves successfully");
        if (!created.success) return;

        const auto duplicate = repository.create("Open gate", unfinished);
        expect(!duplicate.success && duplicate.status == 409, "duplicate name is rejected");

        const auto loaded = repository.get(created.circuit.id);
        expect(loaded.success && loaded.circuit.document == unfinished, "get restores stored component data");

        const auto missing = repository.update(999, "Missing", unfinished);
        expect(!missing.success && missing.status == 404, "update reports missing circuit as not found");

        auto broken = unfinished;
        broken["wires"].push_back({{"sourceId", "absent"}, {"destinationId", "9"}, {"destinationPin", 0}});
        const auto rollback = repository.create("Should roll back", broken);
        const auto list = repository.list();
        expect(!rollback.success && list.success && list.circuits.size() == 1,
               "failed wire insert rolls back the entire save");
    }

    {
        CircuitRepository repository(path.string());
        const auto list = repository.list();
        expect(list.success && list.circuits.size() == 1, "saved circuit survives repository restart");
        if (list.success && !list.circuits.empty()) {
            auto updated = unfinished;
            updated["components"][0]["position"]["x"] = 8;
            const auto result = repository.update(list.circuits[0].id, "Renamed", updated);
            const auto loaded = repository.get(list.circuits[0].id);
            expect(result.success, "explicit circuit update succeeds");
            expect(loaded.success && loaded.circuit.name == "Renamed" && loaded.circuit.document == updated,
                   "updated name and document are restored");
        }
    }
}

void test_complete_circuit(const std::filesystem::path& path) {
    const nlohmann::json circuit = {
        {"version", 1},
        {"components", nlohmann::json::array({
            {{"id", "1"}, {"type", "input"}, {"name", "A"}, {"value", 1},
             {"position", {{"x", 1}, {"y", 2}}}},
            {{"id", "2"}, {"type", "output"}, {"name", "Y"},
             {"position", {{"x", 5}, {"y", 6}}}}
        })},
        {"wires", nlohmann::json::array({
            {{"sourceId", "1"}, {"destinationId", "2"}, {"destinationPin", 0}}
        })}
    };

    CircuitRepository repository(path.string());
    const auto saved = repository.create("Complete", circuit);
    const auto restored = saved.success ? repository.get(saved.circuit.id) : RepositoryResult{};
    expect(saved.success && restored.success && restored.circuit.document == circuit,
           "complete circuit and wires round-trip");
}

void test_database_failures(const std::filesystem::path& directory) {
    const auto corrupt_path = directory / "corrupt.sqlite3";
    {
        std::ofstream corrupt(corrupt_path, std::ios::binary);
        corrupt << "not a SQLite database";
    }
    {
        CircuitRepository repository(corrupt_path.string());
        const auto created = repository.create("Unavailable", unfinished_circuit());
        const auto loaded = repository.get(1);
        const auto listed = repository.list();
        expect(!repository.error().empty(), "schema initialization failure is retained");
        expect(!created.success && created.status == 500, "create reports unavailable database as a server error");
        expect(!loaded.success && loaded.status == 500, "get does not disguise database failure as not found");
        expect(!listed.success && listed.status == 500, "list does not disguise database failure as an empty success");
    }
    std::filesystem::remove(corrupt_path);

    const auto schema_path = directory / "incompatible.sqlite3";
    std::filesystem::remove(schema_path);
    sqlite3* database = nullptr;
    expect(sqlite3_open(schema_path.string().c_str(), &database) == SQLITE_OK,
           "test creates incompatible SQLite schema");
    if (database != nullptr) {
        const int schema_result = sqlite3_exec(database,
            "CREATE TABLE circuits(id INTEGER PRIMARY KEY,name TEXT,schema_version INTEGER);",
            nullptr, nullptr, nullptr);
        expect(schema_result == SQLITE_OK, "test schema setup succeeds");
        sqlite3_close_v2(database);
    }
    {
        CircuitRepository repository(schema_path.string());
        const auto loaded = repository.get(1);
        const auto listed = repository.list();
        expect(!loaded.success && loaded.status == 500, "prepare failure is reported as a database error");
        expect(!listed.success && listed.status == 500, "list prepare failure is reported, not returned as empty");
    }
    std::filesystem::remove(schema_path);
}
} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path();
    const auto path = directory / "dls_repository_test.sqlite3";
    std::filesystem::remove(path);

    test_save_load_update(path);
    std::filesystem::remove(path);
    test_complete_circuit(path);
    std::filesystem::remove(path);
    test_database_failures(directory);

    if (failures != 0) return 1;
    std::cout << "Repository persistence tests passed\n";
    return 0;
}
