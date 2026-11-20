#include "object_composition_adapter.hpp"
#include "object_commands.hpp"
#include "inde/application/project_service.hpp"
#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include <filesystem>
#include <fstream>
#include <glib.h>
#include <iostream>
#include <map>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace inde;
namespace {
void require(bool yes, const char *message) { if (!yes) throw std::runtime_error(message); }
std::map<std::string, std::string> files(const fs::path &root) {
  std::map<std::string, std::string> result;
  for (const auto &entry : fs::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file()) continue;
    std::ifstream input(entry.path(), std::ios::binary); require(bool(input), "Fixture file must be readable");
    result.emplace(entry.path().lexically_relative(root).string(), std::string(std::istreambuf_iterator<char>(input), {}));
  }
  return result;
}
struct Fixture {
  fs::path root = fs::temp_directory_path() / ("inde-obj01-composition-service-" + project::new_uuid());
  Fixture() {
    require(fs::create_directory(root), "Use a new fixture directory");
    g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  }
  ~Fixture() { std::error_code ignored; fs::remove_all(root, ignored); }
};
}
int main() {
  try {
    Fixture fixture; const auto path = fixture.root / "Synthetic.inde";
    application::ProjectService service; service.create(path, "Leitura de Composição OBJ-01 sintética");
    auto d = service.writing().create_document("Origem com alternativa privada");
    d.content = "Primeiro🙂\nOculto\nÚltimo";
    d.paragraphs = project::identify_paragraphs(d.content);
    d.formatting = {{project::DocumentTextStyle::Bold, 0, 9, {}}};
    const auto top = project::new_uuid();
    d.resmas = {{project::new_uuid(), "Resma", top, 10, 16,
                  {{top, "Corrente", {}}, {project::new_uuid(), "Coberta", std::make_shared<const project::DocumentLeafContent>(project::DocumentLeafContent{"ALTERNATIVA PRIVADA", {}})}}}};
    d.visibility_profiles = {{project::new_uuid(), "Lente autoral", true, {},
                               {{project::VisibilityRuleKind::Paragraph, d.paragraphs.rows[1].id}}}};
    d = service.writing().update_document(d);
    const auto doc_bytes = persistence::encode_document_snapshot(d);
    auto stored = service.compositions().create("Montagem com todos os modos");
    const auto live = service.compositions().import(stored.value, d, 0, project::utf8_character_count(d.content));
    const auto block = stored.value.blocks.occurrence(live).block_id;
    const auto old = stored.value.blocks.block(block).current_revision;
    const auto pin = stored.value.blocks.place(block); stored.value.blocks.pin(pin, old);
    const auto ref = stored.value.blocks.place(block); stored.value.blocks.reference_only(ref);
    const auto fork = stored.value.blocks.place(block); stored.value.blocks.fork(fork, old);
    const auto hidden = stored.value.blocks.place(block); stored.value.blocks.set_hidden(hidden, true);
    stored.value.blocks.revise(block, old, "APLICADO NOVO🙂\n");
    const auto alt = stored.value.blocks.alternative(live, "Outra variante");
    (void)alt;
    stored = service.compositions().save(stored);
    stored.value.drafts[block] = {stored.value.blocks.block(block).current_revision, "RASCUNHO PRIVADO NÃO APLICADO"};
    stored = service.compositions().save(stored, true);
    const auto id = stored.value.blocks.composition_id();
    const auto payload = persistence::encode_composition(stored.value);
    const auto before = files(path);
    auto result = objects::compat::read_composition(service.compositions().load(id));
    require(result.projection && result.diagnostics.empty(), "Read stored Composition from real schema 37 repository");
    auto &view = *result.projection;
    require(view.check_source(service.compositions().load(id)).current, "Exact repository envelope matches capture");
    require(persistence::encode_composition(view.source_snapshot().value) == payload, "Full aggregate captured without dropping history/drafts");
    require(view.reading().text.find("RASCUNHO") == std::string::npos, "Private pending draft never incorporated");
    objects::CompositionEditor plan(view.graph());
    auto change = plan.apply(0, {objects::ReorderOccurrences{view.graph().root_layout_id,
                                 {view.occurrences().back().occurrence_id}, view.occurrences().front().occurrence_id},
                                 objects::ChangeLayout{view.graph().root_layout_id, objects::LayoutKind::Row}});
    require(change.accepted && change.changed && plan.undo(plan.revision()).accepted && plan.current() == view.graph(), "Object commands/undo independent of source aggregate");
    require(files(path) == before, "Adapter and object commands leave every project file byte-identical");
    const auto loaded = service.compositions().load(id);
    require(loaded.revision == stored.revision && loaded.sha256 == stored.sha256 &&
            persistence::encode_composition(loaded.value) == payload, "Stored revisions, identities, classifications, origins and drafts unchanged");
    require(persistence::encode_document_snapshot(*service.writing().document(d.id)) == doc_bytes, "Original Document never rewritten by Composition read");
    {
      persistence::SqliteDatabase db(persistence::ProjectDatabaseRepository::database_path(path));
      require(persistence::SchemaMigrator{}.current_version(db) == 37 && db.query_text("PRAGMA integrity_check") == "ok", "Schema and SQLite integrity preserved");
      auto fk = db.prepare("PRAGMA foreign_key_check"); require(!fk.step(), "Foreign keys preserved");
    }
    auto source_edit = *service.writing().document(d.id); source_edit.title = "Origem mudou depois da captura";
    (void)service.writing().update_document(source_edit); // Synthetic source writer.
    require(view.check_source(service.compositions().load(id)).current,
            "Live follows block revision, never implicitly a later Document origin");
    auto other = service.compositions().load(id); other.value.drafts[block].text += " alterado por outra sessão";
    other = service.compositions().save(other, true); // Synthetic Composition writer.
    const auto conflict = view.check_source(other);
    require(!conflict.current && !conflict.diagnostics.empty() &&
            conflict.diagnostics[0].code == "source_composition_revision_changed", "Durable draft-only save invalidates source stamp");
    require(persistence::encode_composition(view.source_snapshot().value) == payload, "Old capture remains frozen after both repository changes");
    std::cout << "INDE-OBJ-01: composition repository integration passed; full project bytes conserved by read/commands, schema 37, frozen origins, all modes, private draft and competing writer verified\n";
    return 0;
  } catch (const std::exception &error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
