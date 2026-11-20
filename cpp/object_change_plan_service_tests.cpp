#include "object_change_plan.hpp"
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
using namespace inde::objects;
using namespace inde::objects::planning;
namespace {
void require(bool yes, const char *why) { if (!yes) throw std::runtime_error(why); }
std::map<std::string, std::string> files(const fs::path &root) {
  std::map<std::string, std::string> result;
  for (const auto &entry : fs::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file()) continue;
    std::ifstream input(entry.path(), std::ios::binary);
    require(bool(input), "Fixture file must be readable");
    result.emplace(entry.path().lexically_relative(root).string(), std::string(std::istreambuf_iterator<char>(input), {}));
  }
  return result;
}
struct Fixture {
  fs::path root = fs::temp_directory_path() / ("inde-obj01-change-plan-" + project::new_uuid());
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
    application::ProjectService service; service.create(path, "Planos OBJ-01 sintéticos");
    const auto owner = service.current()->manifest().project_id;
    auto d = service.writing().create_document("Origem privada");
    d.content = "A🙂\nBé\nC";
    d.paragraphs = project::identify_paragraphs(d.content);
    d.formatting = {{project::DocumentTextStyle::Bold, 0, 2, {}}};
    d.anchors = {{project::new_uuid(), "Marcação declarada", 0, 2}};
    d.visibility_profiles = {{project::new_uuid(), "Lente", true, {},
                              {{project::VisibilityRuleKind::Paragraph, d.paragraphs.rows[1].id}}}};
    d = service.writing().update_document(d);
    auto stored = service.compositions().create("Montagem privada");
    const auto live = service.compositions().import(stored.value, d, 0, project::utf8_character_count(d.content));
    const auto block = stored.value.blocks.occurrence(live).block_id;
    const auto old = stored.value.blocks.block(block).current_revision;
    const auto pin = stored.value.blocks.place(block); stored.value.blocks.pin(pin, old);
    const auto ref = stored.value.blocks.place(block); stored.value.blocks.reference_only(ref);
    const auto fork = stored.value.blocks.place(block); stored.value.blocks.fork(fork, old);
    const auto hidden = stored.value.blocks.place(block); stored.value.blocks.set_hidden(hidden, true);
    stored.value.blocks.revise(block, old, "APLICADO NOVO🙂\n");
    stored = service.compositions().save(stored);
    stored.value.drafts[block] = {stored.value.blocks.block(block).current_revision, "RASCUNHO PRIVADO NÃO APLICADO"};
    stored = service.compositions().save(stored, true);
    const auto id = stored.value.blocks.composition_id();
    const auto source_document = persistence::encode_document_snapshot(d);
    const auto source_composition = persistence::encode_composition(stored.value);
    const auto before = files(path);
    auto doc = compat::read_document(owner, *service.writing().document(d.id));
    auto comp = compat::read_composition(service.compositions().load(id));
    require(doc.projection && comp.projection, "Capture both real repository aggregates");
    ChangeRequest doc_request{ChangePurpose::AuthorialReorder,
      {ReorderOccurrences{doc.projection->graph().root_layout_id,
          {doc.projection->paragraphs()[2].occurrence_id}, doc.projection->paragraphs()[0].occurrence_id}}};
    auto document_plan = plan_document_change(*doc.projection, owner, *service.writing().document(d.id), doc_request, {0, 2});
    require(document_plan.plan && document_plan.plan->review().source_changed, "Create authorial document candidate through real text engine");
    require(document_plan.plan->preview().document.content == "C\nA🙂\nBé", "Document candidate has requested reading order");
    doc_request.purpose = ChangePurpose::VisualOnly;
    auto visual_plan = plan_document_change(*doc.projection, owner, d, doc_request, {0, 2});
    require(visual_plan.plan && !visual_plan.plan->review().source_changed && visual_plan.plan->review().graph_changed,
            "Same object command supports explicit visual intent without source change");
    ChangeRequest comp_request{ChangePurpose::AuthorialReorder,
      {ReorderOccurrences{comp.projection->graph().root_layout_id,
          {comp.projection->occurrences().back().occurrence_id}, comp.projection->occurrences().front().occurrence_id}}};
    auto composition_plan = plan_composition_change(*comp.projection, service.compositions().load(id), comp_request);
    require(composition_plan.plan && composition_plan.plan->review().source_changed, "Create authorial Composition candidate including hidden occurrence");
    require(composition_plan.plan->preview_reading().text == composition_plan.plan->before_reading().text,
            "Hidden occurrence movement changes source order without changing visible text");
    require(composition_plan.plan->preview_reading().text.find("RASCUNHO") == std::string::npos, "Preview never applies draft");
    auto restored = composition_plan.plan->preview(); restored.blocks.reorder(composition_plan.plan->source_order());
    require(persistence::encode_composition(restored) == source_composition, "All aggregate fields besides order preserved exactly");
    doc.projection.reset(); comp.projection.reset();
    require(document_plan.plan->check_source(owner, *service.writing().document(d.id)).current &&
            composition_plan.plan->check_source(service.compositions().load(id)).current, "Plans own frozen captures after adapters destroyed");
    const auto loaded = service.compositions().load(id);
    require(loaded.revision == stored.revision && loaded.sha256 == stored.sha256 &&
            persistence::encode_composition(loaded.value) == source_composition, "Repository revision/hash/private aggregate unchanged");
    require(persistence::encode_document_snapshot(*service.writing().document(d.id)) == source_document, "Repository document unchanged");
    require(files(path) == before, "Planning/freshness checks leave every project file byte-identical");
    {
      persistence::SqliteDatabase db(persistence::ProjectDatabaseRepository::database_path(path));
      require(persistence::SchemaMigrator{}.current_version(db) == 37 && db.query_text("PRAGMA integrity_check") == "ok", "Schema 37 and database integrity preserved");
      auto fk = db.prepare("PRAGMA foreign_key_check"); require(!fk.step(), "Foreign keys preserved");
    }
    auto edited = *service.writing().document(d.id); edited.title = "Outro escritor sintético";
    (void)service.writing().update_document(edited);
    require(!document_plan.plan->check_source(owner, *service.writing().document(d.id)).current, "Detect durable document conflict");
    auto other = service.compositions().load(id); other.value.drafts[block].text += " outra sessão";
    other = service.compositions().save(other, true);
    require(!composition_plan.plan->check_source(other).current, "Detect durable draft-only Composition conflict");
    require(persistence::encode_document_snapshot(document_plan.plan->before().document) == source_document &&
            persistence::encode_composition(composition_plan.plan->before()) == source_composition,
            "Old plans stay immutable after sequential synthetic writers");
    std::cout << "INDE-OBJ-01: real repository dry-run passed; all project bytes conserved by planning, schema 37, source conflicts and immutable previews verified\n";
    return 0;
  } catch (const std::exception &error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
