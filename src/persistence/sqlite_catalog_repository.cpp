#include "inde/persistence/sqlite_catalog_repository.hpp"

#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"

#include <stdexcept>

namespace inde::persistence {
namespace {

SqliteDatabase open_database(const std::filesystem::path &project_path) {
  const auto path = ProjectDatabaseRepository::database_path(project_path);
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("O banco de dados do projeto não foi encontrado");
  return SqliteDatabase(path);
}

std::string project_id(SqliteDatabase &database) {
  auto statement = database.prepare("SELECT id FROM projects");
  if (!statement.step()) {
    throw std::runtime_error("O banco não contém a identidade do projeto");
  }
  auto result = statement.column_text(0);
  if (statement.step()) {
    throw std::runtime_error("O banco contém mais de uma identidade de projeto");
  }
  return result;
}

} // namespace

project::Catalog SqliteCatalogRepository::load(
    const std::filesystem::path &project_path) const {
  auto database = open_database(project_path);
  project::Catalog catalog;
  auto ips = database.prepare(
      "SELECT id, title, subtitle, description, cover_path, created_at, "
      "updated_at FROM intellectual_properties ORDER BY created_at, id");
  while (ips.step()) {
    project::IntellectualProperty value{
        ips.column_text(0), ips.column_text(1), ips.column_text(2),
        ips.column_text(3), ips.column_text(4), ips.column_text(5),
        ips.column_text(6)};
    project::validate(value);
    catalog.intellectual_properties.push_back(std::move(value));
  }

  auto works = database.prepare(
      "SELECT id, intellectual_property_id, title, subtitle, synopsis, "
      "language, status, created_at, updated_at FROM works "
      "ORDER BY created_at, id");
  while (works.step()) {
    project::Work value{
        works.column_text(0), works.column_text(1), works.column_text(2),
        works.column_text(3), works.column_text(4), works.column_text(5),
        works.column_text(6), works.column_text(7), works.column_text(8)};
    project::validate(value);
    catalog.works.push_back(std::move(value));
  }
  return catalog;
}

void SqliteCatalogRepository::save(
    const std::filesystem::path &project_path,
    const project::IntellectualProperty &value) const {
  project::validate(value);
  auto database = open_database(project_path);
  SqliteTransaction transaction(database);
  auto statement = database.prepare(
      "INSERT INTO intellectual_properties "
      "(id, project_id, title, subtitle, description, cover_path, created_at, "
      "updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(id) DO UPDATE SET title=excluded.title, "
      "subtitle=excluded.subtitle, description=excluded.description, "
      "cover_path=excluded.cover_path, updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, project_id(database));
  statement.bind(3, value.title);
  statement.bind(4, value.subtitle);
  statement.bind(5, value.description);
  statement.bind(6, value.cover_path);
  statement.bind(7, value.created_at);
  statement.bind(8, value.updated_at);
  statement.run();
  transaction.commit();
}

void SqliteCatalogRepository::save(const std::filesystem::path &project_path,
                                   const project::Work &value) const {
  project::validate(value);
  auto database = open_database(project_path);
  SqliteTransaction transaction(database);
  auto statement = database.prepare(
      "INSERT INTO works "
      "(id, intellectual_property_id, title, subtitle, synopsis, language, "
      "status, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(id) DO UPDATE SET "
      "intellectual_property_id=excluded.intellectual_property_id, "
      "title=excluded.title, subtitle=excluded.subtitle, "
      "synopsis=excluded.synopsis, language=excluded.language, "
      "status=excluded.status, updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, value.intellectual_property_id);
  statement.bind(3, value.title);
  statement.bind(4, value.subtitle);
  statement.bind(5, value.synopsis);
  statement.bind(6, value.language);
  statement.bind(7, value.status);
  statement.bind(8, value.created_at);
  statement.bind(9, value.updated_at);
  statement.run();
  transaction.commit();
}

void SqliteCatalogRepository::remove_intellectual_property(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  SqliteTransaction transaction(database);
  auto statement =
      database.prepare("DELETE FROM intellectual_properties WHERE id = ?");
  statement.bind(1, id);
  statement.run();
  if (database.changes() != 1)
    throw std::runtime_error("Propriedade intelectual não encontrada no banco");
  transaction.commit();
}

void SqliteCatalogRepository::remove_work(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  SqliteTransaction transaction(database);
  auto statement = database.prepare("DELETE FROM works WHERE id = ?");
  statement.bind(1, id);
  statement.run();
  if (database.changes() != 1)
    throw std::runtime_error("Obra não encontrada no banco");
  transaction.commit();
}

} // namespace inde::persistence
