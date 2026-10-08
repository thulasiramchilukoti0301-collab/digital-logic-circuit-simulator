#include "CircuitRepository.h"
#include <sqlite3.h>
#include <filesystem>
#include <cmath>
#include <cstdlib>
#include <unordered_map>

namespace digital_logic::app {
using Json=nlohmann::json;
namespace {
struct Stmt { sqlite3_stmt* p=nullptr; ~Stmt(){ if(p) sqlite3_finalize(p); } };
void text(sqlite3_stmt* s,int i,const std::string& v){sqlite3_bind_text(s,i,v.c_str(),-1,SQLITE_TRANSIENT);}
std::string col(sqlite3_stmt*s,int i){const auto*p=sqlite3_column_text(s,i);return p?reinterpret_cast<const char*>(p):"";}
}
CircuitRepository::CircuitRepository(std::string path):path_(std::move(path)) {
 try { auto parent=std::filesystem::path(path_).parent_path(); if(!parent.empty()) std::filesystem::create_directories(parent); }
 catch (...) { error_="Database directory is unavailable."; return; }
 sqlite3* db=nullptr; if(sqlite3_open(path_.c_str(),&db)!=SQLITE_OK){error_="Database could not be opened.";if(db)sqlite3_close(db);return;} db_=db;sqlite3_busy_timeout(db,5000); initialize();
}
CircuitRepository::~CircuitRepository(){if(db_)sqlite3_close(static_cast<sqlite3*>(db_));}
bool CircuitRepository::initialize(){
 auto* db=static_cast<sqlite3*>(db_); char* e=nullptr;
 const char* sql="PRAGMA foreign_keys=ON; CREATE TABLE IF NOT EXISTS circuits(id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE CHECK(length(trim(name)) BETWEEN 1 AND 120), schema_version INTEGER NOT NULL CHECK(schema_version=1), created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP); CREATE TABLE IF NOT EXISTS components(circuit_id INTEGER NOT NULL, component_id TEXT NOT NULL, type TEXT NOT NULL CHECK(type IN ('input','output','and','or','not','xor','nand','nor')), name TEXT NOT NULL, x REAL NOT NULL CHECK(x > -1.0e308 AND x < 1.0e308), y REAL NOT NULL CHECK(y > -1.0e308 AND y < 1.0e308), input_count INTEGER, input_value INTEGER, PRIMARY KEY(circuit_id,component_id), FOREIGN KEY(circuit_id) REFERENCES circuits(id) ON DELETE CASCADE, CHECK(input_value IS NULL OR input_value IN (0,1)), CHECK((type='input' AND input_count IS NULL AND input_value IS NOT NULL) OR (type='output' AND input_count IS NULL AND input_value IS NULL) OR (type IN ('and','or','nand','nor') AND input_count>=2 AND input_value IS NULL) OR (type='not' AND input_count=1 AND input_value IS NULL) OR (type='xor' AND input_count=2 AND input_value IS NULL))); CREATE TABLE IF NOT EXISTS wires(circuit_id INTEGER NOT NULL, source_id TEXT NOT NULL, destination_id TEXT NOT NULL, destination_pin INTEGER NOT NULL CHECK(destination_pin>=0), PRIMARY KEY(circuit_id,destination_id,destination_pin), FOREIGN KEY(circuit_id,source_id) REFERENCES components(circuit_id,component_id), FOREIGN KEY(circuit_id,destination_id) REFERENCES components(circuit_id,component_id));";
 if(sqlite3_exec(db,sql,nullptr,nullptr,&e)!=SQLITE_OK){error_="Database schema could not be initialized.";sqlite3_free(e);return false;}return true;
}
RepositoryResult CircuitRepository::create(const std::string& n,const Json& d){return save(false,0,n,d);}
RepositoryResult CircuitRepository::update(long long id,const std::string& n,const Json& d){return save(true,id,n,d);}
RepositoryResult CircuitRepository::save(bool update,long long id,const std::string& name,const Json& doc){
 std::lock_guard<std::mutex> lock(mutex_); RepositoryResult r; auto* db=static_cast<sqlite3*>(db_); if(!db){r.message="Database is unavailable.";return r;}
 if(sqlite3_exec(db,"BEGIN IMMEDIATE",nullptr,nullptr,nullptr)!=SQLITE_OK){r.message="Database is busy.";return r;}
 auto rollback=[&]{sqlite3_exec(db,"ROLLBACK",nullptr,nullptr,nullptr);};
 Stmt head; const char* q=update?"UPDATE circuits SET name=?,updated_at=CURRENT_TIMESTAMP WHERE id=?":"INSERT INTO circuits(name,schema_version) VALUES(?,1)";
 if(sqlite3_prepare_v2(db,q,-1,&head.p,nullptr)!=SQLITE_OK){rollback();return r;} text(head.p,1,name); if(update)sqlite3_bind_int64(head.p,2,id);
 if(sqlite3_step(head.p)!=SQLITE_DONE){int ec=sqlite3_errcode(db);rollback();if(ec==SQLITE_CONSTRAINT){r.status=update?409:409;r.code="duplicate_name";r.message="A saved circuit with that name already exists.";}return r;}
 long long key=update?id:sqlite3_last_insert_rowid(db); if(update&&sqlite3_changes(db)==0){rollback();r.status=404;r.code="not_found";r.message="Saved circuit was not found.";return r;}
 if(update){Stmt del;sqlite3_prepare_v2(db,"DELETE FROM wires WHERE circuit_id=?",-1,&del.p,nullptr);sqlite3_bind_int64(del.p,1,key);if(sqlite3_step(del.p)!=SQLITE_DONE){rollback();return r;}Stmt dc;sqlite3_prepare_v2(db,"DELETE FROM components WHERE circuit_id=?",-1,&dc.p,nullptr);sqlite3_bind_int64(dc.p,1,key);if(sqlite3_step(dc.p)!=SQLITE_DONE){rollback();return r;}}
 Stmt cp,wp;sqlite3_prepare_v2(db,"INSERT INTO components(circuit_id,component_id,type,name,x,y,input_count,input_value) VALUES(?,?,?,?,?,?,?,?)",-1,&cp.p,nullptr);sqlite3_prepare_v2(db,"INSERT INTO wires(circuit_id,source_id,destination_id,destination_pin) VALUES(?,?,?,?)",-1,&wp.p,nullptr);
 for(const auto& c:doc["components"]){sqlite3_reset(cp.p);sqlite3_clear_bindings(cp.p);sqlite3_bind_int64(cp.p,1,key);text(cp.p,2,c["id"].get<std::string>());text(cp.p,3,c["type"].get<std::string>());text(cp.p,4,c.value("name",std::string{}));double x=c["position"]["x"].get<double>(),y=c["position"]["y"].get<double>();sqlite3_bind_double(cp.p,5,x);sqlite3_bind_double(cp.p,6,y);if(c.contains("inputCount"))sqlite3_bind_int(cp.p,7,c["inputCount"].get<int>());else sqlite3_bind_null(cp.p,7);if(c.contains("value"))sqlite3_bind_int(cp.p,8,c["value"].get<int>());else sqlite3_bind_null(cp.p,8);if(sqlite3_step(cp.p)!=SQLITE_DONE){rollback();return r;}}
 for(const auto&w:doc["wires"]){sqlite3_reset(wp.p);sqlite3_clear_bindings(wp.p);sqlite3_bind_int64(wp.p,1,key);text(wp.p,2,w["sourceId"].get<std::string>());text(wp.p,3,w["destinationId"].get<std::string>());sqlite3_bind_int(wp.p,4,w["destinationPin"].get<int>());if(sqlite3_step(wp.p)!=SQLITE_DONE){rollback();r.status=400;r.code="invalid_reference";r.message="A wire references an invalid component or pin.";return r;}}
 if(sqlite3_exec(db,"COMMIT",nullptr,nullptr,nullptr)!=SQLITE_OK){rollback();return r;}r.success=true;r.status=200;r.circuit.id=key;r.circuit.name=name;r.circuit.document=doc;Stmt stamp;sqlite3_prepare_v2(db,"SELECT updated_at FROM circuits WHERE id=?",-1,&stamp.p,nullptr);sqlite3_bind_int64(stamp.p,1,key);if(sqlite3_step(stamp.p)==SQLITE_ROW)r.circuit.updated_at=col(stamp.p,0);return r;
}
RepositoryResult CircuitRepository::get(long long id){
 std::lock_guard<std::mutex> lock(mutex_);RepositoryResult r;auto*db=static_cast<sqlite3*>(db_);if(!db){r.message="Database is unavailable.";return r;}Stmt h;sqlite3_prepare_v2(db,"SELECT name,updated_at FROM circuits WHERE id=?",-1,&h.p,nullptr);sqlite3_bind_int64(h.p,1,id);if(sqlite3_step(h.p)!=SQLITE_ROW){r.status=404;r.code="not_found";r.message="Saved circuit was not found.";return r;}r.circuit.id=id;r.circuit.name=col(h.p,0);r.circuit.updated_at=col(h.p,1);r.circuit.document={{"version",1},{"components",Json::array()},{"wires",Json::array()}};
 Stmt c;sqlite3_prepare_v2(db,"SELECT component_id,type,name,x,y,input_count,input_value FROM components WHERE circuit_id=? ORDER BY rowid",-1,&c.p,nullptr);sqlite3_bind_int64(c.p,1,id);while(sqlite3_step(c.p)==SQLITE_ROW){Json v={{"id",col(c.p,0)},{"type",col(c.p,1)},{"name",col(c.p,2)},{"position",{{"x",sqlite3_column_double(c.p,3)},{"y",sqlite3_column_double(c.p,4)}}}};if(sqlite3_column_type(c.p,5)!=SQLITE_NULL)v["inputCount"]=sqlite3_column_int(c.p,5);if(sqlite3_column_type(c.p,6)!=SQLITE_NULL)v["value"]=sqlite3_column_int(c.p,6);r.circuit.document["components"].push_back(v);}
 Stmt w;sqlite3_prepare_v2(db,"SELECT source_id,destination_id,destination_pin FROM wires WHERE circuit_id=? ORDER BY rowid",-1,&w.p,nullptr);sqlite3_bind_int64(w.p,1,id);while(sqlite3_step(w.p)==SQLITE_ROW)r.circuit.document["wires"].push_back({{"sourceId",col(w.p,0)},{"destinationId",col(w.p,1)},{"destinationPin",sqlite3_column_int(w.p,2)}});r.success=true;r.status=200;return r;
}
std::vector<SavedCircuit> CircuitRepository::list(){std::lock_guard<std::mutex>l(mutex_);std::vector<SavedCircuit>v;auto*db=static_cast<sqlite3*>(db_);if(!db)return v;Stmt s;sqlite3_prepare_v2(db,"SELECT id,name,updated_at FROM circuits ORDER BY name COLLATE NOCASE",-1,&s.p,nullptr);while(sqlite3_step(s.p)==SQLITE_ROW){SavedCircuit c;c.id=sqlite3_column_int64(s.p,0);c.name=col(s.p,1);c.updated_at=col(s.p,2);v.push_back(c);}return v;}
}
