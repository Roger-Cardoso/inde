#include "inde/experimental/lab_source.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include <fstream>
#include <sstream>
namespace inde::experimental {
namespace {
std::string file_text(const std::filesystem::path &path) {
  std::ifstream input(path); if (!input) throw std::runtime_error("Arquivo da cópia não está acessível");
  std::ostringstream text; text << input.rdbuf(); return text.str();
}
void no_symlink(const std::filesystem::path &path) {
  auto current = path;
  while (!current.empty()) {
    if (std::filesystem::is_symlink(current)) throw std::runtime_error("Use uma cópia direta, sem links simbólicos");
    auto parent = current.parent_path(); if (parent == current) break; current = parent;
  }
}
}
LabSource::LabSource(std::filesystem::path copy) : copy_(std::move(copy)) {
  const auto db = copy_ / "data/project.sqlite3";
  no_symlink(db); no_symlink(copy_ / "manifest.json"); no_symlink(copy_ / "TEXT_BLOCKS_LAB_COPY");
  if (!std::filesystem::is_regular_file(db) || std::filesystem::is_symlink(copy_) || std::filesystem::is_symlink(db))
    throw std::runtime_error("Laboratório exige cópia válida, nunca cria banco ausente");
  std::ifstream marker(copy_ / "TEXT_BLOCKS_LAB_COPY"); std::string line;
  std::getline(marker, line);
  if (line != "INDE_TEXT_BLOCKS_SPIKE_COPY_V1") throw std::runtime_error("Use uma cópia preparada pelo lançador do laboratório");
  persistence::SqliteDatabase connection(db);
  if (persistence::SchemaMigrator{}.current_version(connection) != persistence::current_database_schema_version)
    throw std::runtime_error("Laboratório não migra projetos; prepare a cópia no schema "+std::to_string(persistence::current_database_schema_version));
  manifest_at_open_ = file_text(copy_ / "manifest.json");
}
std::vector<project::DocumentSummary> LabSource::list(std::size_t offset) const {
  persistence::DocumentQuery q; q.limit = 50; q.offset = offset;
  return reader_.document_summaries(copy_, q);
}
project::Document LabSource::load(const std::string &id) const {
  auto d = reader_.document(copy_, id);
  if (!d) throw std::runtime_error("Documento não encontrado na cópia");
  if (d->content.size() > 2 * 1024 * 1024) throw std::runtime_error("Laboratório gráfico limitado a 2 MiB; use o spike CLI para escala maior");
  return *d;
}
std::size_t LabSource::outgoing_references(const std::string &id) const {
  persistence::WritingStore::TextReferenceQuery q; q.document_id = id; q.limit = 1;
  return reader_.text_references(copy_, q).total;
}
project::Document LabSource::create_snapshot(const project::Document &d) const {
  no_symlink(copy_ / "TEXT_WORKSHOP_COPY"); no_symlink(copy_ / "data/project.sqlite3");
  if (file_text(copy_ / "TEXT_WORKSHOP_COPY") != "INDE_TEXT_WORKSHOP_COPY_V1\n" ||
      file_text(copy_ / "manifest.json") != manifest_at_open_)
    throw std::runtime_error("Criação exige a cópia nova preparada para esta reforma e identidade inalterada");
  project::validate(d);
  persistence::SqliteDatabase db(copy_ / "data/project.sqlite3");
  if (persistence::SchemaMigrator{}.current_version(db) != persistence::current_database_schema_version)
    throw std::runtime_error("Schema mudou; não gravar sem rever a cópia");
  if (reader_.document(copy_, d.id)) throw std::runtime_error("O Documento já existe: o laboratório não sobrescreve");
  reader_.save(copy_, d);
  auto restored = reader_.document(copy_, d.id);
  if (!restored) throw std::runtime_error("Gravação não pôde ser confirmada; não repita sem consultar a Biblioteca");
  return *restored;
}
}
