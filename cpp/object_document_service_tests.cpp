#include "object_document_adapter.hpp"
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
void require(bool yes, const char *why) { if (!yes) throw std::runtime_error(why); }
std::map<std::string, std::string> bytes(const fs::path &root) {
  std::map<std::string, std::string> files;
  for (const auto &entry : fs::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file()) continue;
    std::ifstream file(entry.path(), std::ios::binary);
    files.emplace(entry.path().lexically_relative(root).string(),
                   std::string(std::istreambuf_iterator<char>(file), {}));
  }
  return files;
}
struct Fixture {
  fs::path root = fs::temp_directory_path() / ("inde-obj01-document-service-" + project::new_uuid());
  Fixture() { fs::create_directory(root); g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true); }
  ~Fixture() { std::error_code ignored; fs::remove_all(root, ignored); }
};
}
int main() {
  try {
    Fixture fixture;
    const auto path = fixture.root / "Synthetic.inde";
    application::ProjectService service;
    service.create(path, "Compatibilidade OBJ-01 sintética");
    const auto owner = service.current()->manifest().project_id;
    auto d = service.writing().create_document("Documento com ordem e visibilidade explícitas");
    d.content = "Primeiro🙂\nOculto\nÚltimo";
    d.paragraphs = project::identify_paragraphs(d.content);
    d.formatting = {{project::DocumentTextStyle::Bold, 0, 9, {}}};
    d.anchors = {{project::new_uuid(), "Marcação declarada", 0, 9}};
    d.visibility_profiles = {{project::new_uuid(), "Lente", true, {},
                              {{project::VisibilityRuleKind::Paragraph, d.paragraphs.rows[1].id}}}};
    d = service.writing().update_document(d);
    auto composition = service.compositions().create("Montagem preexistente");
    const auto live = service.compositions().import(composition.value, d, 0,
                                                    project::utf8_character_count(d.content));
    const auto &placement = composition.value.blocks.occurrence(live);
    const auto block = placement.block_id;
    const auto revision = composition.value.blocks.block(block).current_revision;
    const auto pinned = composition.value.blocks.place(block);
    composition.value.blocks.pin(pinned, revision);
    const auto reference = composition.value.blocks.place(block);
    composition.value.blocks.reference_only(reference);
    composition = service.compositions().save(composition);
    composition.value.drafts[block] = {revision, "RASCUNHO PRIVADO NÃO APLICADO"};
    composition = service.compositions().save(composition, true);
    const auto composition_id = composition.value.blocks.composition_id();
    const auto original_composition = persistence::encode_composition(composition.value);
    const auto before_files = bytes(path);
    const auto before_document = persistence::encode_document_snapshot(d);
    auto loaded = service.writing().document(d.id);
    require(loaded.has_value(), "Source document must load from existing schema 37 repository");
    auto read = objects::compat::read_document(owner, *loaded);
    require(read.projection && read.diagnostics.empty(), "Read-only adapter accepts repository Document");
    auto &view = *read.projection;
    require(view.check_source(owner, *service.writing().document(d.id)).current, "Loaded source freshness verified");
    require(persistence::encode_document_snapshot(view.source_snapshot()) == before_document,
            "All persisted document layers preserved exactly");
    objects::CompositionEditor copy(view.graph());
    auto operation = copy.apply(0, {
      objects::ReorderOccurrences{view.graph().root_layout_id,
                                  {view.paragraphs()[2].occurrence_id}, view.paragraphs()[0].occurrence_id},
      objects::ChangeLayout{view.graph().root_layout_id, objects::LayoutKind::Row}});
    require(operation.accepted && operation.changed, "Visual object plan independently transformable");
    require(copy.undo(copy.revision()).accepted && copy.current() == view.graph(), "Undo of object plan exact");
    require(bytes(path) == before_files, "Read/graph operations must not modify any project file");
    const auto still = service.compositions().load(composition_id);
    require(still.revision == composition.revision && still.sha256 == composition.sha256 &&
            persistence::encode_composition(still.value) == original_composition,
            "Pinned/live/reference placements, origins, revisions and durable draft untouched");
    require(persistence::encode_document_snapshot(*service.writing().document(d.id)) == before_document,
            "Source document never receives object order");
    {
      persistence::SqliteDatabase db(persistence::ProjectDatabaseRepository::database_path(path));
      require(persistence::SchemaMigrator{}.current_version(db) == 37, "Existing schema unchanged");
      require(db.query_text("PRAGMA integrity_check") == "ok", "Fixture database integrity");
      auto fk = db.prepare("PRAGMA foreign_key_check"); require(!fk.step(), "Fixture foreign key integrity");
    }
    auto edited = *service.writing().document(d.id);
    edited.title = "Mudança concorrente explícita na fixture";
    edited = service.writing().update_document(edited); // Synthetic competing writer only.
    const auto fresh = service.writing().document(d.id);
    const auto conflict = view.check_source(owner, *fresh);
    require(!conflict.current && !conflict.diagnostics.empty() &&
            conflict.diagnostics[0].code == "source_snapshot_changed", "Repository change detected without overwriting it");
    require(persistence::encode_document_snapshot(view.source_snapshot()) == before_document,
            "Frozen source remains independent after repository update");
    std::cout << "INDE-OBJ-01: repository read-adapter integration passed; project bytes unchanged by read/commands, schema 37, composition/drafts conserved, freshness conflict detected\n";
    return 0;
  } catch (const std::exception &e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
