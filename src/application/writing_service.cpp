#include "inde/application/writing_service.hpp"

#include "inde/project/manifest.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace inde::application {

const project::Project &WritingService::require_project() const {
  const auto *value = session_.current();
  if (!value)
    throw std::runtime_error("Nenhum projeto está aberto");
  return *value;
}

void WritingService::validate_placement(
    const std::optional<std::string> &editorial_node_id) const {
  if (!editorial_node_id)
    return;
  const auto found = std::find_if(
      session_.structural_nodes().begin(), session_.structural_nodes().end(),
      [&](const auto &node) { return node.id == *editorial_node_id; });
  if (found == session_.structural_nodes().end())
    throw std::runtime_error("A unidade editorial do Documento não existe");
}

void WritingService::validate_entity_references(
    const project::Document &value) const {
  for (const auto &reference : value.entity_references) {
    // A FK repete a invariante dentro da transação. Esta leitura antecipa uma
    // mensagem compreensível ao caso de uso antes da gravação.
    if (!store_.referenced_entity_exists(require_project().path(),
                                         reference.entity_id))
      throw std::runtime_error("A entidade vinculada ao Documento não existe");
  }
}

void WritingService::validate_organization(
    const project::Document &value) const {
  if (value.group_id &&
      !store_.document_group(require_project().path(), *value.group_id))
    throw std::runtime_error("O grupo do Documento não existe");
  if (value.revision_of_id) {
    const auto source =
        store_.document(require_project().path(), *value.revision_of_id);
    if (!source)
      throw std::runtime_error(
          "O Documento de origem desta revisão não existe");
    auto ancestor = source->revision_of_id;
    std::size_t depth = 0;
    while (ancestor) {
      if (*ancestor == value.id)
        throw std::runtime_error(
            "A cadeia de revisões documentais não pode formar um ciclo");
      const auto document =
          store_.document(require_project().path(), *ancestor);
      ancestor = document ? document->revision_of_id : std::nullopt;
      if (++depth > 1000)
        throw std::runtime_error(
            "A cadeia de revisões documentais é profunda demais");
    }
  }
}

std::vector<project::Document>
WritingService::documents(const persistence::DocumentQuery &query) const {
  return store_.documents(require_project().path(), query);
}

std::vector<project::DocumentSummary> WritingService::document_summaries(
    const persistence::DocumentQuery &query) const {
  return store_.document_summaries(require_project().path(), query);
}

std::size_t
WritingService::document_count(const persistence::DocumentQuery &query) const {
  return store_.document_count(require_project().path(), query);
}

std::optional<project::Document>
WritingService::document(const std::string &id) const {
  return store_.document(require_project().path(), id);
}

std::vector<project::DocumentGroup> WritingService::document_groups() const {
  return store_.document_groups(require_project().path());
}

std::optional<project::DocumentGroup>
WritingService::document_group(const std::string &id) const {
  return store_.document_group(require_project().path(), id);
}

project::DocumentGroup
WritingService::create_document_group(std::string name,
                                      std::string description) {
  const auto &active = require_project();
  const auto now = project::utc_now();
  project::DocumentGroup value{project::new_uuid(), std::move(name),
                               std::move(description), now, now};
  project::validate(value);
  store_.save_group(active.path(), value);
  return value;
}

project::DocumentGroup
WritingService::update_document_group(const project::DocumentGroup &input) {
  const auto &active = require_project();
  const auto current = store_.document_group(active.path(), input.id);
  if (!current)
    throw std::runtime_error("Grupo documental não encontrado");
  auto value = input;
  value.created_at = current->created_at;
  value.updated_at = project::utc_now();
  project::validate(value);
  store_.save_group(active.path(), value);
  return value;
}

void WritingService::delete_document_group(const std::string &id) {
  const auto &active = require_project();
  if (!store_.document_group(active.path(), id))
    throw std::runtime_error("Grupo documental não encontrado");
  store_.remove_group(active.path(), id);
}

project::Document
WritingService::create_document(std::string title,
                                std::optional<std::string> editorial_node_id) {
  const auto &active = require_project();
  validate_placement(editorial_node_id);
  const auto now = project::utc_now();
  project::Document value{project::new_uuid(),
                          std::move(editorial_node_id),
                          std::move(title),
                          {},
                          now,
                          now,
                          std::nullopt,
                          {},
                          {},
                          {},
                          std::nullopt,
                          project::DocumentPurpose::MainText,
                          std::nullopt,
                          {},
                          {}};
  project::validate(value);
  validate_organization(value);
  validate_entity_references(value);
  store_.save(active.path(), value);
  return value;
}

project::Document
WritingService::update_document(const project::Document &input) {
  const auto &active = require_project();
  const auto current = store_.document(active.path(), input.id);
  if (!current)
    throw std::runtime_error("Documento não encontrado");
  validate_placement(input.editorial_node_id);
  auto value = input;
  value.created_at = current->created_at;
  value.updated_at = project::utc_now();
  project::validate(value);
  validate_organization(value);
  validate_entity_references(value);
  store_.save(active.path(), value);
  return value;
}

void WritingService::delete_document(const std::string &id) {
  const auto &active = require_project();
  if (!store_.document(active.path(), id))
    throw std::runtime_error("Documento não encontrado");
  store_.remove(active.path(), id);
}

} // namespace inde::application
