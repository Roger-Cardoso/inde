#include "inde/persistence/sqlite_linguistic_catalog.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace inde::persistence {
namespace {

struct DatabaseCloser {
  void operator()(sqlite3 *database) const noexcept {
    if (database)
      sqlite3_close_v2(database);
  }
};

struct StatementCloser {
  void operator()(sqlite3_stmt *statement) const noexcept {
    if (statement)
      sqlite3_finalize(statement);
  }
};

using Database = std::unique_ptr<sqlite3, DatabaseCloser>;
using Statement = std::unique_ptr<sqlite3_stmt, StatementCloser>;

std::string sqlite_message(sqlite3 *database, std::string_view action) {
  std::string message{"Não foi possível "};
  message += action;
  message += ": ";
  message += database ? sqlite3_errmsg(database) : "erro SQLite desconhecido";
  return message;
}

Database open_read_only(const std::filesystem::path &path) {
  if (path.empty())
    throw std::runtime_error("O caminho do catálogo não foi configurado");
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("Catálogo linguístico não encontrado em " +
                             path.string());
  sqlite3 *raw{};
  const auto filename = path.string();
  const int result =
      sqlite3_open_v2(filename.c_str(), &raw,
                      SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
  Database database{raw};
  if (result != SQLITE_OK)
    throw std::runtime_error(sqlite_message(raw, "abrir o catálogo"));
  if (sqlite3_db_readonly(raw, "main") != 1)
    throw std::runtime_error(
        "O catálogo linguístico não abriu em modo somente leitura");
  char *error{};
  if (sqlite3_exec(raw, "PRAGMA query_only = ON", nullptr, nullptr, &error) !=
      SQLITE_OK) {
    const std::string detail = error ? error : "erro desconhecido";
    sqlite3_free(error);
    throw std::runtime_error(
        "Não foi possível tornar o catálogo somente leitura: " + detail);
  }
  return database;
}

Statement prepare(sqlite3 *database, std::string_view sql) {
  sqlite3_stmt *raw{};
  if (sqlite3_prepare_v2(database, sql.data(), static_cast<int>(sql.size()),
                         &raw, nullptr) != SQLITE_OK)
    throw std::runtime_error(sqlite_message(database, "preparar a consulta"));
  return Statement{raw};
}

void bind_text(sqlite3 *database, sqlite3_stmt *statement, int index,
               const std::string &value) {
  if (sqlite3_bind_text(statement, index, value.data(),
                        static_cast<int>(value.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK)
    throw std::runtime_error(
        sqlite_message(database, "vincular o texto da consulta"));
}

void bind_integer(sqlite3 *database, sqlite3_stmt *statement, int index,
                  std::size_t value) {
  if (sqlite3_bind_int64(statement, index, static_cast<sqlite3_int64>(value)) !=
      SQLITE_OK)
    throw std::runtime_error(
        sqlite_message(database, "vincular o limite da consulta"));
}

std::string column_text(sqlite3_stmt *statement, int column) {
  if (sqlite3_column_type(statement, column) == SQLITE_NULL)
    return {};
  const auto *value = sqlite3_column_text(statement, column);
  const auto size = sqlite3_column_bytes(statement, column);
  return {reinterpret_cast<const char *>(value),
          static_cast<std::size_t>(size)};
}

std::string trimmed(std::string value) {
  const auto whitespace = [](unsigned char character) {
    return std::isspace(character) != 0;
  };
  value.erase(value.begin(),
              std::find_if_not(value.begin(), value.end(), whitespace));
  value.erase(std::find_if_not(value.rbegin(), value.rend(), whitespace).base(),
              value.end());
  return value;
}

std::size_t checked_limit(std::size_t limit) {
  if (limit == 0 || limit > 50)
    throw std::invalid_argument("O limite da consulta deve ficar entre 1 e 50");
  return limit;
}

std::filesystem::path default_data_root() {
  if (const auto *xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
    return std::filesystem::path{xdg} / "inde" / "linguistic";
  if (const auto *home = std::getenv("HOME"); home && *home)
    return std::filesystem::path{home} / ".local" / "share" / "inde" /
           "linguistic";
  return {};
}

void require_lexical_contract(sqlite3 *database) {
  auto statement = prepare(
      database,
      "SELECT contract_version, service_version, status, read_only_runtime, "
      "network_required FROM query_catalog_metadata WHERE singleton = 1");
  if (sqlite3_step(statement.get()) != SQLITE_ROW ||
      column_text(statement.get(), 0) != "0.5.0" ||
      column_text(statement.get(), 1) != "l06-query-1" ||
      column_text(statement.get(), 2) != "ready" ||
      sqlite3_column_int(statement.get(), 3) != 1 ||
      sqlite3_column_int(statement.get(), 4) != 0)
    throw std::runtime_error(
        "O catálogo lexical não corresponde ao contrato L-06 aceito");
}

void require_construction_contract(sqlite3 *database) {
  auto statement = prepare(
      database, "SELECT contract_version, pipeline_version, status, "
                "human_review_status, origin_kind, epistemic_status_preserved, "
                "distribution_scope, automatic_bindings FROM promotion_build");
  if (sqlite3_step(statement.get()) != SQLITE_ROW ||
      column_text(statement.get(), 0) != "0.6.0" ||
      column_text(statement.get(), 1) != "l08-promotion-1" ||
      column_text(statement.get(), 2) != "promoted" ||
      column_text(statement.get(), 3) != "accepted" ||
      column_text(statement.get(), 4) != "synthetic_fixture" ||
      column_text(statement.get(), 5) != "derived" ||
      column_text(statement.get(), 6) != "internal_productive" ||
      sqlite3_column_int(statement.get(), 7) != 0)
    throw std::runtime_error(
        "O catálogo construcional não corresponde à promoção L-08 aceita");
}

} // namespace

LinguisticCatalogPaths LinguisticCatalogPaths::from_environment() {
  const auto root = default_data_root();
  const auto *lexical = std::getenv("INDE_L06_CATALOG");
  const auto *constructions = std::getenv("INDE_L08_CATALOG");
  return {
      lexical && *lexical ? std::filesystem::path{lexical}
                          : root / "dictionary-l06-query.sqlite",
      constructions && *constructions ? std::filesystem::path{constructions}
                                      : root / "dictionary-l08-promoted.sqlite",
  };
}

project::LinguisticCatalogStatus SqliteLinguisticCatalog::status() const {
  project::LinguisticCatalogStatus result;
  try {
    auto database = open_read_only(paths_.lexical);
    require_lexical_contract(database.get());
    result.lexical_available = true;
    result.lexical_message =
        "Catálogo lexical L-06 disponível em modo somente leitura.";
  } catch (const std::exception &error) {
    result.lexical_message = error.what();
  }
  try {
    auto database = open_read_only(paths_.constructions);
    require_construction_contract(database.get());
    result.constructions_available = true;
    result.constructions_message =
        "Catálogo construcional L-08 disponível em modo somente leitura.";
  } catch (const std::exception &error) {
    result.constructions_message = error.what();
  }
  return result;
}

project::LexicalQueryResult
SqliteLinguisticCatalog::query_lexicon(const std::string &input,
                                       std::size_t limit) const {
  const auto query = trimmed(input);
  if (query.empty())
    throw std::invalid_argument(
        "Informe uma palavra ou expressão para consultar");
  if (query.size() > 256)
    throw std::invalid_argument("A consulta lexical excede 256 bytes");
  limit = checked_limit(limit);
  auto database = open_read_only(paths_.lexical);
  require_lexical_contract(database.get());

  project::LexicalQueryResult result;
  result.query = query;
  {
    auto count = prepare(
        database.get(),
        "SELECT COUNT(*) FROM lexical_query_entry "
        "WHERE lower(normalized) = lower(?1) OR lower(written) = lower(?1)");
    bind_text(database.get(), count.get(), 1, query);
    if (sqlite3_step(count.get()) != SQLITE_ROW)
      throw std::runtime_error(
          sqlite_message(database.get(), "contar resultados lexicais"));
    result.total =
        static_cast<std::size_t>(sqlite3_column_int64(count.get(), 0));
  }
  auto statement = prepare(
      database.get(),
      "SELECT form_id, lexeme_id, sense_id, written, ptbr_pos, upos, "
      "source_pos, definition_state, definition_text, sense_review_status "
      "FROM lexical_query_entry "
      "WHERE lower(normalized) = lower(?1) OR lower(written) = lower(?1) "
      "ORDER BY normalized, lexeme_id, sense_id LIMIT ?2");
  bind_text(database.get(), statement.get(), 1, query);
  bind_integer(database.get(), statement.get(), 2, limit);
  int step{};
  while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
    project::LexicalSenseCandidate candidate;
    candidate.form_id = column_text(statement.get(), 0);
    candidate.lexeme_id = column_text(statement.get(), 1);
    candidate.sense_id = column_text(statement.get(), 2);
    candidate.written = column_text(statement.get(), 3);
    candidate.ptbr_pos = column_text(statement.get(), 4);
    candidate.upos = column_text(statement.get(), 5);
    candidate.source_pos = column_text(statement.get(), 6);
    candidate.definition_state = column_text(statement.get(), 7);
    if (sqlite3_column_type(statement.get(), 8) != SQLITE_NULL)
      candidate.definition = column_text(statement.get(), 8);
    candidate.review_status = column_text(statement.get(), 9);
    result.candidates.push_back(std::move(candidate));
  }
  if (step != SQLITE_DONE)
    throw std::runtime_error(
        sqlite_message(database.get(), "consultar o léxico"));
  return result;
}

std::vector<project::LinguisticEntitySummary>
SqliteLinguisticCatalog::linguistic_entities(const std::string &input,
                                             std::size_t limit) const {
  const auto search = trimmed(input);
  if (search.size() > 256)
    throw std::invalid_argument("A pesquisa de construções excede 256 bytes");
  limit = checked_limit(limit);
  auto database = open_read_only(paths_.constructions);
  require_construction_contract(database.get());
  auto statement = prepare(
      database.get(),
      "WITH entities(kind, id, name, subtype, description, epistemic_status, "
      "review_status) AS ("
      "SELECT 0, id, name, construction_type, description, epistemic_status, "
      "review_status FROM construction UNION ALL "
      "SELECT 1, id, name, frame_type, definition, epistemic_status, "
      "review_status FROM frame UNION ALL "
      "SELECT 2, id, name, frame_type, 'Estrutura sintática explícita.', "
      "epistemic_status, review_status FROM syntactic_frame UNION ALL "
      "SELECT 3, id, canonical_form, mwe_type, "
      "'Expressão multipalavra com contiguidade ' || contiguity || '.', "
      "epistemic_status, review_status FROM mwe) "
      "SELECT kind, id, name, subtype, description, epistemic_status, "
      "review_status FROM entities "
      "WHERE ?1 = '' OR instr(lower(name), lower(?1)) > 0 "
      "ORDER BY kind, name, id LIMIT ?2");
  bind_text(database.get(), statement.get(), 1, search);
  bind_integer(database.get(), statement.get(), 2, limit);
  std::vector<project::LinguisticEntitySummary> result;
  int step{};
  while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
    project::LinguisticEntitySummary entity;
    switch (sqlite3_column_int(statement.get(), 0)) {
    case 0:
      entity.kind = project::LinguisticEntityKind::Construction;
      break;
    case 1:
      entity.kind = project::LinguisticEntityKind::Frame;
      break;
    case 2:
      entity.kind = project::LinguisticEntityKind::SyntacticFrame;
      break;
    case 3:
      entity.kind = project::LinguisticEntityKind::MultiwordExpression;
      break;
    default:
      throw std::runtime_error("Tipo linguístico inesperado no catálogo L-08");
    }
    entity.id = column_text(statement.get(), 1);
    entity.name = column_text(statement.get(), 2);
    entity.subtype = column_text(statement.get(), 3);
    entity.description = column_text(statement.get(), 4);
    entity.epistemic_status = column_text(statement.get(), 5);
    entity.review_status = column_text(statement.get(), 6);
    result.push_back(std::move(entity));
  }
  if (step != SQLITE_DONE)
    throw std::runtime_error(
        sqlite_message(database.get(), "consultar construções"));
  return result;
}

} // namespace inde::persistence
