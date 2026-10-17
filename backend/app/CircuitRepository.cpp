#include "CircuitRepository.h"

#include <sqlite3.h>

#include <filesystem>
#include <utility>

namespace digital_logic::app {
namespace {

using Json = nlohmann::json;

class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db) {
        result_ = sqlite3_prepare_v2(db_, sql, -1, &statement_, nullptr);
    }

    ~Statement() {
        if (statement_ != nullptr) {
            sqlite3_finalize(statement_);
        }
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    bool ready() const { return result_ == SQLITE_OK; }
    sqlite3_stmt* get() const { return statement_; }
    int result() const { return result_; }

private:
    sqlite3* db_;
    sqlite3_stmt* statement_ = nullptr;
    int result_ = SQLITE_ERROR;
};

class Transaction {
public:
    explicit Transaction(sqlite3* db) : db_(db) {
        active_ = sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK;
    }

    ~Transaction() {
        if (active_) {
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        }
    }

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    bool active() const { return active_; }

    bool commit() {
        if (!active_ || sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK) {
            return false;
        }
        active_ = false;
        return true;
    }

private:
    sqlite3* db_;
    bool active_ = false;
};

bool bind_text(sqlite3_stmt* statement, int index, const std::string& value) {
    return sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

std::string column_text(sqlite3_stmt* statement, int index) {
    const auto* value = sqlite3_column_text(statement, index);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

RepositoryResult database_error(const char* message = "The circuit could not be stored.") {
    RepositoryResult result;
    result.status = 500;
    result.code = "database_error";
    result.message = message;
    return result;
}

RepositoryListResult list_database_error() {
    RepositoryListResult result;
    result.status = 500;
    result.code = "database_error";
    result.message = "Saved circuits could not be read.";
    return result;
}

bool is_unique_constraint(int code) {
    return code == SQLITE_CONSTRAINT_UNIQUE || code == SQLITE_CONSTRAINT_PRIMARYKEY;
}

} // namespace

struct CircuitRepository::Connection {
    explicit Connection(sqlite3* handle) : handle(handle) {}
    ~Connection() {
        if (handle != nullptr) {
            sqlite3_close_v2(handle);
        }
    }

    sqlite3* handle;
};

CircuitRepository::CircuitRepository(std::string path) : path_(std::move(path)) {
    try {
        const auto parent = std::filesystem::path(path_).parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
    } catch (...) {
        error_ = "Database directory is unavailable.";
        return;
    }

    sqlite3* handle = nullptr;
    if (sqlite3_open(path_.c_str(), &handle) != SQLITE_OK) {
        error_ = "Database could not be opened.";
        if (handle != nullptr) {
            sqlite3_close_v2(handle);
        }
        return;
    }

    db_ = std::make_unique<Connection>(handle);
    if (sqlite3_extended_result_codes(handle, 1) != SQLITE_OK ||
        sqlite3_busy_timeout(handle, 5000) != SQLITE_OK || !initialize()) {
        if (error_.empty()) {
            error_ = "Database could not be initialized.";
        }
        db_.reset();
    }
}

CircuitRepository::~CircuitRepository() = default;

bool CircuitRepository::initialize() {
    const char* sql =
        "PRAGMA foreign_keys=ON;"
        "CREATE TABLE IF NOT EXISTS circuits("
        "id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL UNIQUE CHECK(length(trim(name)) BETWEEN 1 AND 120),"
        "schema_version INTEGER NOT NULL CHECK(schema_version=1),"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);"
        "CREATE TABLE IF NOT EXISTS components("
        "circuit_id INTEGER NOT NULL, component_id TEXT NOT NULL,"
        "type TEXT NOT NULL CHECK(type IN ('input','output','and','or','not','xor','nand','nor'))"
        ",name TEXT NOT NULL, x REAL NOT NULL CHECK(x > -1.0e308 AND x < 1.0e308),"
        "y REAL NOT NULL CHECK(y > -1.0e308 AND y < 1.0e308),"
        "input_count INTEGER, input_value INTEGER,"
        "PRIMARY KEY(circuit_id,component_id),"
        "FOREIGN KEY(circuit_id) REFERENCES circuits(id) ON DELETE CASCADE,"
        "CHECK(input_value IS NULL OR input_value IN (0,1)),"
        "CHECK((type='input' AND input_count IS NULL AND input_value IS NOT NULL) OR "
        "(type='output' AND input_count IS NULL AND input_value IS NULL) OR "
        "(type IN ('and','or','nand','nor') AND input_count>=2 AND input_value IS NULL) OR "
        "(type='not' AND input_count=1 AND input_value IS NULL) OR "
        "(type='xor' AND input_count=2 AND input_value IS NULL)));"
        "CREATE TABLE IF NOT EXISTS wires("
        "circuit_id INTEGER NOT NULL, source_id TEXT NOT NULL, destination_id TEXT NOT NULL,"
        "destination_pin INTEGER NOT NULL CHECK(destination_pin>=0),"
        "PRIMARY KEY(circuit_id,destination_id,destination_pin),"
        "FOREIGN KEY(circuit_id,source_id) REFERENCES components(circuit_id,component_id),"
        "FOREIGN KEY(circuit_id,destination_id) REFERENCES components(circuit_id,component_id));";

    auto* db = db_->handle;
    char* sqlite_message = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &sqlite_message) != SQLITE_OK) {
        sqlite3_free(sqlite_message);
        error_ = "Database schema could not be initialized.";
        return false;
    }

    Statement foreign_keys(db, "PRAGMA foreign_keys");
    if (!foreign_keys.ready() || sqlite3_step(foreign_keys.get()) != SQLITE_ROW ||
        sqlite3_column_int(foreign_keys.get(), 0) != 1) {
        error_ = "Database foreign-key enforcement could not be enabled.";
        return false;
    }
    return true;
}

RepositoryResult CircuitRepository::create(const std::string& name, const Json& document) {
    return save(false, 0, name, document);
}

RepositoryResult CircuitRepository::update(long long id, const std::string& name, const Json& document) {
    return save(true, id, name, document);
}

RepositoryResult CircuitRepository::save(bool update, long long id, const std::string& name, const Json& document) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) {
        return database_error("Database is unavailable.");
    }

    auto* db = db_->handle;
    Transaction transaction(db);
    if (!transaction.active()) {
        return database_error("Database is busy.");
    }

    Statement head(db, update
        ? "UPDATE circuits SET name=?,updated_at=CURRENT_TIMESTAMP WHERE id=?"
        : "INSERT INTO circuits(name,schema_version) VALUES(?,1)");
    if (!head.ready() || !bind_text(head.get(), 1, name) ||
        (update && sqlite3_bind_int64(head.get(), 2, id) != SQLITE_OK)) {
        return database_error();
    }

    const int head_step = sqlite3_step(head.get());
    if (head_step != SQLITE_DONE) {
        const int code = sqlite3_extended_errcode(db);
        if (is_unique_constraint(code)) {
            RepositoryResult result;
            result.status = 409;
            result.code = "duplicate_name";
            result.message = "A saved circuit with that name already exists.";
            return result;
        }
        return database_error();
    }

    const long long key = update ? id : sqlite3_last_insert_rowid(db);
    if (update && sqlite3_changes(db) == 0) {
        RepositoryResult result;
        result.status = 404;
        result.code = "not_found";
        result.message = "Saved circuit was not found.";
        return result;
    }

    if (update) {
        Statement delete_wires(db, "DELETE FROM wires WHERE circuit_id=?");
        if (!delete_wires.ready() || sqlite3_bind_int64(delete_wires.get(), 1, key) != SQLITE_OK ||
            sqlite3_step(delete_wires.get()) != SQLITE_DONE) {
            return database_error();
        }
        Statement delete_components(db, "DELETE FROM components WHERE circuit_id=?");
        if (!delete_components.ready() || sqlite3_bind_int64(delete_components.get(), 1, key) != SQLITE_OK ||
            sqlite3_step(delete_components.get()) != SQLITE_DONE) {
            return database_error();
        }
    }

    Statement component_insert(db,
        "INSERT INTO components(circuit_id,component_id,type,name,x,y,input_count,input_value) "
        "VALUES(?,?,?,?,?,?,?,?)");
    Statement wire_insert(db,
        "INSERT INTO wires(circuit_id,source_id,destination_id,destination_pin) VALUES(?,?,?,?)");
    if (!component_insert.ready() || !wire_insert.ready()) {
        return database_error();
    }

    for (const auto& component : document.at("components")) {
        auto* statement = component_insert.get();
        if (sqlite3_reset(statement) != SQLITE_OK || sqlite3_clear_bindings(statement) != SQLITE_OK ||
            sqlite3_bind_int64(statement, 1, key) != SQLITE_OK ||
            !bind_text(statement, 2, component.at("id").get<std::string>()) ||
            !bind_text(statement, 3, component.at("type").get<std::string>()) ||
            !bind_text(statement, 4, component.value("name", std::string{})) ||
            sqlite3_bind_double(statement, 5, component.at("position").at("x").get<double>()) != SQLITE_OK ||
            sqlite3_bind_double(statement, 6, component.at("position").at("y").get<double>()) != SQLITE_OK) {
            return database_error();
        }

        const bool has_input_count = component.contains("inputCount");
        const bool has_value = component.contains("value");
        if ((has_input_count && sqlite3_bind_int(statement, 7, component.at("inputCount").get<int>()) != SQLITE_OK) ||
            (!has_input_count && sqlite3_bind_null(statement, 7) != SQLITE_OK) ||
            (has_value && sqlite3_bind_int(statement, 8, component.at("value").get<int>()) != SQLITE_OK) ||
            (!has_value && sqlite3_bind_null(statement, 8) != SQLITE_OK) ||
            sqlite3_step(statement) != SQLITE_DONE) {
            return database_error();
        }
    }

    for (const auto& wire : document.at("wires")) {
        auto* statement = wire_insert.get();
        if (sqlite3_reset(statement) != SQLITE_OK || sqlite3_clear_bindings(statement) != SQLITE_OK ||
            sqlite3_bind_int64(statement, 1, key) != SQLITE_OK ||
            !bind_text(statement, 2, wire.at("sourceId").get<std::string>()) ||
            !bind_text(statement, 3, wire.at("destinationId").get<std::string>()) ||
            sqlite3_bind_int(statement, 4, wire.at("destinationPin").get<int>()) != SQLITE_OK) {
            return database_error();
        }
        if (sqlite3_step(statement) != SQLITE_DONE) {
            if ((sqlite3_extended_errcode(db) & 0xFF) == SQLITE_CONSTRAINT) {
                RepositoryResult result;
                result.status = 400;
                result.code = "invalid_reference";
                result.message = "A wire references an invalid component or pin.";
                return result;
            }
            return database_error();
        }
    }

    std::string updated_at;
    {
        Statement timestamp(db, "SELECT updated_at FROM circuits WHERE id=?");
        if (!timestamp.ready() || sqlite3_bind_int64(timestamp.get(), 1, key) != SQLITE_OK ||
            sqlite3_step(timestamp.get()) != SQLITE_ROW) {
            return database_error();
        }
        updated_at = column_text(timestamp.get(), 0);
    }

    if (!transaction.commit()) {
        return database_error();
    }

    RepositoryResult result;
    result.success = true;
    result.status = update ? 200 : 201;
    result.code.clear();
    result.message.clear();
    result.circuit.id = key;
    result.circuit.name = name;
    result.circuit.updated_at = std::move(updated_at);
    result.circuit.document = document;
    return result;
}

RepositoryResult CircuitRepository::get(long long id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) {
        return database_error("Database is unavailable.");
    }

    auto* db = db_->handle;
    RepositoryResult result;
    Statement head(db, "SELECT name,updated_at FROM circuits WHERE id=?");
    if (!head.ready() || sqlite3_bind_int64(head.get(), 1, id) != SQLITE_OK) {
        return database_error("Saved circuits could not be read.");
    }

    const int head_step = sqlite3_step(head.get());
    if (head_step == SQLITE_DONE) {
        result.status = 404;
        result.code = "not_found";
        result.message = "Saved circuit was not found.";
        return result;
    }
    if (head_step != SQLITE_ROW) {
        return database_error("Saved circuits could not be read.");
    }

    result.circuit.id = id;
    result.circuit.name = column_text(head.get(), 0);
    result.circuit.updated_at = column_text(head.get(), 1);
    result.circuit.document = {{"version", 1}, {"components", Json::array()}, {"wires", Json::array()}};

    Statement components(db,
        "SELECT component_id,type,name,x,y,input_count,input_value FROM components "
        "WHERE circuit_id=? ORDER BY rowid");
    if (!components.ready() || sqlite3_bind_int64(components.get(), 1, id) != SQLITE_OK) {
        return database_error("Saved circuits could not be read.");
    }
    for (;;) {
        const int step = sqlite3_step(components.get());
        if (step == SQLITE_DONE) break;
        if (step != SQLITE_ROW) return database_error("Saved circuits could not be read.");

        Json component = {
            {"id", column_text(components.get(), 0)},
            {"type", column_text(components.get(), 1)},
            {"name", column_text(components.get(), 2)},
            {"position", {{"x", sqlite3_column_double(components.get(), 3)},
                          {"y", sqlite3_column_double(components.get(), 4)}}}
        };
        if (sqlite3_column_type(components.get(), 5) != SQLITE_NULL) {
            component["inputCount"] = sqlite3_column_int(components.get(), 5);
        }
        if (sqlite3_column_type(components.get(), 6) != SQLITE_NULL) {
            component["value"] = sqlite3_column_int(components.get(), 6);
        }
        result.circuit.document["components"].push_back(std::move(component));
    }

    Statement wires(db,
        "SELECT source_id,destination_id,destination_pin FROM wires WHERE circuit_id=? ORDER BY rowid");
    if (!wires.ready() || sqlite3_bind_int64(wires.get(), 1, id) != SQLITE_OK) {
        return database_error("Saved circuits could not be read.");
    }
    for (;;) {
        const int step = sqlite3_step(wires.get());
        if (step == SQLITE_DONE) break;
        if (step != SQLITE_ROW) return database_error("Saved circuits could not be read.");
        result.circuit.document["wires"].push_back({
            {"sourceId", column_text(wires.get(), 0)},
            {"destinationId", column_text(wires.get(), 1)},
            {"destinationPin", sqlite3_column_int(wires.get(), 2)}
        });
    }

    result.success = true;
    result.status = 200;
    result.code.clear();
    result.message.clear();
    return result;
}

RepositoryListResult CircuitRepository::list() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) {
        return list_database_error();
    }

    RepositoryListResult result;
    Statement statement(db_->handle, "SELECT id,name,updated_at FROM circuits ORDER BY name COLLATE NOCASE");
    if (!statement.ready()) {
        return list_database_error();
    }

    for (;;) {
        const int step = sqlite3_step(statement.get());
        if (step == SQLITE_DONE) break;
        if (step != SQLITE_ROW) return list_database_error();

        SavedCircuit circuit;
        circuit.id = sqlite3_column_int64(statement.get(), 0);
        circuit.name = column_text(statement.get(), 1);
        circuit.updated_at = column_text(statement.get(), 2);
        result.circuits.push_back(std::move(circuit));
    }

    result.success = true;
    result.status = 200;
    result.code.clear();
    result.message.clear();
    return result;
}

} // namespace digital_logic::app
