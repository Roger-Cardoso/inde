#include "inde/application/composition_service.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/persistence/semantic_store.hpp"
#include "inde/persistence/sqlite_writing_repository.hpp"
#include <glib.h>
namespace inde::application {
const project::Project &CompositionService::owner() const {
  if (!session_.current())
    throw std::runtime_error("Abra um Projeto para usar Composições");
  return *session_.current();
}
std::vector<project::CompositionSummary>
CompositionService::list(std::size_t offset) const {
  return store_.list(owner(), offset);
}
std::vector<project::CompositionSummary> CompositionService::search(const std::string &query,std::size_t limit) const {
  return store_.search(owner(),query,limit);
}
project::StoredComposition
CompositionService::load(const std::string &id) const {
  return store_.load(owner(), id);
}
project::StoredComposition
CompositionService::create(const std::string &name) const {
  project::Composition c;
  c.name = name;
  return store_.save(owner(), c, 0, {});
}
project::StoredComposition
CompositionService::save(const project::StoredComposition &c,
                         bool draft_only) const {
  if (c.project_id != owner().manifest().project_id)
    throw std::runtime_error("Rascunho pertence a outro Projeto");
  return store_.save(owner(), c.value, c.revision, c.sha256, draft_only);
}
std::string CompositionService::import(project::Composition &c,
                                       const project::Document &d,
                                       std::size_t start,
                                       std::size_t end) const {
  const auto source = persistence::encode_document_snapshot(d),
             hash = persistence::composition_checksum(source);
  auto next = c;
  next.origins.emplace(hash, d);
  const auto block = next.blocks.import_document(d, start, end),
             placement = next.blocks.place(block);
  next.blocks.assign_origin_snapshot(block, hash);
  (void)persistence::encode_composition(next);
  c = std::move(next);
  return placement;
}
void CompositionService::current(
    const persistence::CompositionOutputGuard &g) const {
  if (g.project_id != owner().manifest().project_id)
    throw std::runtime_error("Saída pertence a outro Projeto");
  const auto c = load(g.composition_id);
  if (c.revision != g.revision || c.sha256 != g.sha256 ||
      persistence::composition_checksum(g.recipe) != g.sha256)
    throw std::runtime_error("Composição mudou; prepare novamente a saída");
}
PreparedComposition
CompositionService::prepare(const project::StoredComposition &c,
                            const std::string &title) const {
  const auto recipe = persistence::encode_composition(c.value);
  persistence::CompositionOutputGuard g{c.project_id,
                                        c.value.blocks.composition_id(),
                                        c.sha256, recipe, c.revision};
  current(g);
  if (!c.value.drafts.empty())
    throw std::runtime_error(
        "Aplique ou descarte os rascunhos antes de preparar saída");
  auto output = experimental::materialize(
      c.value.blocks, c.value.blocks.project(c.value.profile), title);
  if (!output.incorporated_occurrences)
    throw std::runtime_error("A leitura não incorpora nenhum trecho de texto");
  output.warnings.push_back(
      "Origens congeladas preservadas. A saída é independente, não altera "
      "fontes e não segue futuras edições da Composição.");
  auto reading = prepare_document_export(c.project_id, output.document);
  reading.limitations.insert(reading.limitations.end(), output.warnings.begin(),
                             output.warnings.end());
  reading.limitations.push_back("Composição " + g.composition_id +
                                "; revisão " + std::to_string(g.revision) +
                                "; SHA256 " + g.sha256);
  return {std::move(g), std::move(output), std::move(reading)};
}
project::Document
CompositionService::materialize(const PreparedComposition &prepared,
                                bool acknowledged) const {
  if (!acknowledged)
    throw std::runtime_error("Confira os avisos antes de materializar");
  current(prepared.source);
  // Re-derive from the checked recipe; callers cannot forge output or layers.
  const auto c = persistence::decode_composition(prepared.source.recipe,
                                                 prepared.source.sha256);
  auto output = experimental::materialize(c.blocks, c.blocks.project(c.profile),
                                          prepared.output.document.title)
                    .document;
  persistence::SqliteWritingRepository store;
  for (const auto &reference : output.entity_references)
    if (!store.referenced_entity_exists(owner().path(), reference.entity_id))
      throw std::runtime_error("Entidade histórica não existe mais; revise as "
                               "camadas antes de materializar");
  persistence::SemanticStore{}.validate(owner().path(), output);
  store.save_composition_output(owner().path(), output, prepared.source);
  return output;
}
void CompositionService::export_reading(const PreparedComposition &prepared,
                                        const std::filesystem::path &folder,
                                        bool acknowledged) const {
  if (!acknowledged)
    throw std::runtime_error("Confira os avisos antes de exportar");
  current(prepared.source);
  const auto c = load(prepared.source.composition_id);
  const auto trusted = prepare(c, prepared.output.document.title);
  write_document_export(trusted.reading, folder);
}
} // namespace inde::application
