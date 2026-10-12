#include "inde/application/project_service.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char **argv) {
  if(argc!=2)return 2;
  try {
    const std::filesystem::path copy=argv[1];
    for(auto p=copy;p!=p.parent_path();p=p.parent_path())if(std::filesystem::is_symlink(p))throw std::runtime_error("Use uma cópia direta");
    const auto marker=copy/"EDITORIAL_REVIEW_COPY",dbpath=copy/"data/project.sqlite3";
    if(std::filesystem::is_symlink(marker) || std::filesystem::is_symlink(dbpath) || !std::filesystem::is_regular_file(dbpath))throw std::runtime_error("Cópia marcada e banco existente obrigatórios");
    std::ifstream input(marker);std::string line;std::getline(input,line);
    if(line!="INDE_EDITORIAL_REVIEW_COPY_2026_10_07")throw std::runtime_error("Não migra um Projeto sem marcador de cópia de avaliação");
    inde::application::ProjectService service;service.open(copy);service.close();
    inde::persistence::SqliteDatabase db(dbpath);
    if(inde::persistence::SchemaMigrator{}.current_version(db)!=inde::persistence::current_database_schema_version || db.query_text("PRAGMA integrity_check")!="ok" || db.prepare("PRAGMA foreign_key_check").step())throw std::runtime_error("Validação da cópia falhou");
    std::cout<<"PREPARED schema"<<inde::persistence::current_database_schema_version<<" copy="<<copy<<"\n";return 0;
  }catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
