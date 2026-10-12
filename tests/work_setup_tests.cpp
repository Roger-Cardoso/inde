#include "inde/application/project_service.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include <cassert>
#include <glib.h>
#include <iostream>
using namespace inde;
template <class F> void refuses(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::exception &) {
    failed = true;
  }
  assert(failed);
}
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("inde-work-setup-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  application::ProjectService s;
  refuses([&] { static_cast<void>(s.start_work("Sem Projeto", "", "Mundo")); });
  s.create(root / "Vazio.inde", "Primeiro Projeto");
  auto dbpath = persistence::ProjectDatabaseRepository::database_path(
      s.current()->path());
  auto count = [&](const std::string &table) {
    persistence::SqliteDatabase db(dbpath);
    return db.query_integer("SELECT count(*) FROM " + table);
  };
  assert(s.catalog().works.empty() &&
         s.catalog().intellectual_properties.empty());
  const auto journal = count("project_operations");
  for (const auto &title : {"", " ", "\t\n"})
    refuses([&] { static_cast<void>(s.start_work(title, "", "Mundo")); });
  refuses([&] { static_cast<void>(s.start_work("Obra", "", " ")); });
  refuses([&] {
    static_cast<void>(s.start_work("Obra", project::new_uuid(), ""));
  });
  assert(count("works") == 0 && count("intellectual_properties") == 0 &&
         count("project_operations") == journal);
  {
    persistence::SqliteDatabase db(dbpath);
    db.execute("CREATE TRIGGER refuse_first_work BEFORE INSERT ON works BEGIN "
               "SELECT RAISE(ABORT,'QA rollback'); END");
    refuses([&] {
      static_cast<void>(
          s.start_work("Falha", "", "Universo que não pode sobrar"));
    });
    db.execute("DROP TRIGGER refuse_first_work");
  }
  assert(s.catalog().works.empty() && count("intellectual_properties") == 0 &&
         count("project_operations") == journal);
  const auto first = s.start_work("  Minha Obra  ", "", "  Meu universo  ");
  assert(first.title == "Minha Obra" && s.catalog().works.size() == 1 &&
         s.catalog().intellectual_properties.front().title == "Meu universo");
  assert(count("documents") == 0 && count("narrative_structures") == 1 &&
         count("editorial_structures") == 1 && count("narrative_units") == 0 &&
         count("project_operations") == journal + 1);
  const auto reversal = s.reversal_impact(s.project_operations().front().id);
  assert(reversal.allowed);
  s.confirm_project_operation(reversal);
  assert(count("works") == 0 && count("intellectual_properties") == 0 &&
         count("narrative_structures") == 0 &&
         count("editorial_structures") == 0);
  const auto restored = s.start_work("Uma Obra", "", "Mundo");
  const auto second = s.start_work("Segunda", restored.intellectual_property_id,
                                   "Ignorado: universo existente");
  assert(s.catalog().works.size() == 2 &&
         s.catalog().intellectual_properties.size() == 1 &&
         second.intellectual_property_id == restored.intellectual_property_id);
  s.save_as(root / "Cópia.inde");
  s.close();
  s.open(root / "Cópia.inde");
  dbpath = persistence::ProjectDatabaseRepository::database_path(
      s.current()->path());
  assert(s.catalog().works.size() == 2 &&
         s.catalog().intellectual_properties.size() == 1);
  {
    persistence::SqliteDatabase db(dbpath);
    assert(db.query_text("PRAGMA integrity_check") == "ok" &&
           !db.prepare("PRAGMA foreign_key_check").step());
  }
  s.close();
  std::filesystem::remove_all(root);
  std::cout << "PASS: pristine onboarding, validation, atomic rollback, one "
               "reversible operation, existing universe, Save As and restart\n";
}
