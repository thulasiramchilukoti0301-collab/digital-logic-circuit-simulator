#include "CircuitRepository.h"
#include <filesystem>
#include <iostream>
#include <string>
using namespace digital_logic::app;
int main(){
 const auto path=(std::filesystem::temp_directory_path()/"dls_repository_test.sqlite3").string();std::filesystem::remove(path);
 nlohmann::json unfinished={{"version",1},{"components",nlohmann::json::array({{{"id","9"},{"type","and"},{"name","Gate"},{"inputCount",2},{"position",{{"x",4.5},{"y",7}}}}})},{"wires",nlohmann::json::array()}};
 {CircuitRepository repo(path);auto a=repo.create("Open gate",unfinished);if(!a.success)return 1;auto duplicate=repo.create("Open gate",unfinished);if(duplicate.success||duplicate.status!=409)return 2;auto got=repo.get(a.circuit.id);if(!got.success||got.circuit.document!=unfinished)return 3;auto missing=repo.update(999,"Missing",unfinished);if(missing.success||missing.status!=404)return 4;auto broken=unfinished;broken["wires"].push_back({{"sourceId","absent"},{"destinationId","9"},{"destinationPin",0}});auto rollback=repo.create("Should roll back",broken);if(rollback.success||repo.list().size()!=1)return 8;}
 {CircuitRepository reopened(path);auto all=reopened.list();if(all.size()!=1)return 5;auto updated=unfinished;updated["components"][0]["position"]["x"]=8;auto result=reopened.update(all[0].id,"Renamed",updated);if(!result.success)return 6;auto got=reopened.get(all[0].id);if(!got.success||got.circuit.name!="Renamed"||got.circuit.document!=updated)return 7;}
 {CircuitRepository repo(path);nlohmann::json complete={{"version",1},{"components",nlohmann::json::array({{{"id","1"},{"type","input"},{"name","A"},{"value",1},{"position",{{"x",1},{"y",2}}}},{{"id","2"},{"type","output"},{"name","Y"},{"position",{{"x",5},{"y",6}}}}})},{"wires",nlohmann::json::array({{{"sourceId","1"},{"destinationId","2"},{"destinationPin",0}}})}};auto saved=repo.create("Complete",complete);if(!saved.success)return 9;auto restored=repo.get(saved.circuit.id);if(!restored.success||restored.circuit.document!=complete)return 10;}
 std::filesystem::remove(path);std::cout<<"Repository persistence tests passed\n";return 0;
}
