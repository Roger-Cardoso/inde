#include "inde/application/project_service.hpp"
#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "object_checkpoint_store.hpp"
#include <cerrno>
#include <filesystem>
#include <glib.h>
#include <iostream>
#include <stdexcept>
using namespace inde;
using namespace inde::objects;
using namespace inde::objects::planning;
using namespace inde::objects::planning::recoverylab;
namespace fs = std::filesystem;
namespace {
void require(bool yes, const char *why) {
  if (!yes)
    throw std::runtime_error(why);
}
struct Fixture {
  fs::path root = fs::temp_directory_path() /
                  ("inde-obj01-store-service-" + project::new_uuid());
  Fixture() {
    require(fs::create_directory(root), "Exclusive new fixture");
    fs::permissions(root, fs::perms::owner_all);
    g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  }
  ~Fixture() {
    std::error_code ignored;
    fs::remove_all(root, ignored);
  }
};
} // namespace
int main() {
  try {
    Fixture fixture;
    const auto path = fixture.root / "Synthetic.inde";
    application::ProjectService service;
    service.create(path, "Armazenamento OBJ-01 sintético");
    const auto owner = service.current()->manifest().project_id;
    auto d = service.writing().create_document("Fonte privada");
    d.content = "A🙂\nBé\nC";
    d.paragraphs = project::identify_paragraphs(d.content);
    d.formatting = {{project::DocumentTextStyle::Bold, 0, 2, {}}};
    d.anchors = {{project::new_uuid(), "Declarada", 0, 2}};
    d = service.writing().update_document(d);
    auto c = service.compositions().create("Montagem privada");
    const auto live = service.compositions().import(
        c.value, d, 0, project::utf8_character_count(d.content));
    const auto block = c.value.blocks.occurrence(live).block_id,
               version = c.value.blocks.block(block).current_revision;
    const auto pin = c.value.blocks.place(block);
    c.value.blocks.pin(pin, version);
    const auto ref = c.value.blocks.place(block);
    c.value.blocks.reference_only(ref);
    const auto hidden = c.value.blocks.place(block);
    c.value.blocks.set_hidden(hidden, true);
    c = service.compositions().save(c);
    c.value.drafts[block] = {version, "RASCUNHO PRIVADO"};
    c = service.compositions().save(c, true);
    const auto cid = c.value.blocks.composition_id();
    const Scope doc_scope{owner, d.id, SourceKind::Document},
        comp_scope{owner, cid, SourceKind::Composition};
    auto dv = compat::read_document(owner, d);
    auto cv = compat::read_composition(c);
    require(dv.projection && cv.projection, "Real repository sources captured");
    DocumentChangeSession ds(*dv.projection, {0, 2});
    CompositionChangeSession cs(*cv.projection);
    auto dp =
        ds.prepare({ChangePurpose::AuthorialReorder,
                    {ReorderOccurrences{ds.graph().root_layout_id,
                                        {ds.graph().layouts[0].children[2]},
                                        ds.graph().layouts[0].children[0]}}});
    auto cp = cs.prepare(
        {ChangePurpose::AuthorialReorder,
         {ReorderOccurrences{cs.graph().root_layout_id,
                             {cs.graph().layouts[0].children.back()},
                             cs.graph().layouts[0].children.front()}}});
    require(dp.ticket && cp.ticket && ds.apply(*dp.ticket, owner, d).accepted &&
                cs.apply(*cp.ticket, c).accepted,
            "Apply private working changes");
    auto di = ds.begin_save();
    auto ci = cs.begin_save();
    require(di.ticket && ci.ticket, "Freeze exact save intents");
    auto doc_store =
        CheckpointStore::create_new(fixture.root / "document-store", doc_scope);
    auto comp_store = CheckpointStore::create_new(
        fixture.root / "composition-store", comp_scope);
    require(doc_store->publish(ds.checkpoint()).durable &&
                comp_store->publish(cs.checkpoint()).durable,
            "Checkpoint durable before authorial CAS");
    require(persistence::encode_document_snapshot(*service.writing().document(
                d.id)) == persistence::encode_document_snapshot(d) &&
                service.compositions().load(cid).revision == c.revision,
            "Store publication never writes project SQLite");
    (void)service.writing().update_document_if_unchanged(di.ticket->candidate(),
                                                         di.ticket->expected());
    auto request = ci.ticket->expected();
    request.value = ci.ticket->candidate();
    (void)service.compositions().save(request);
    auto saved_doc = *service.writing().document(d.id);
    auto saved_comp = service.compositions().load(cid);
    // Generation 1 remains enough to classify the commit-before-ack outcome
    // after closing/reopening the store; no in-memory ticket is used by
    // recovery.
    doc_store.reset();
    comp_store.reset();
    doc_store = CheckpointStore::open_existing(fixture.root / "document-store",
                                               doc_scope);
    comp_store = CheckpointStore::open_existing(
        fixture.root / "composition-store", comp_scope);
    auto dr = DocumentChangeSession::restore(
        doc_store->latest(), owner, saved_doc, RecoveryMode::AdoptMatchingSave);
    auto cr = CompositionChangeSession::restore(
        comp_store->latest(), saved_comp, RecoveryMode::AdoptMatchingSave);
    require(dr.session && cr.session && !dr.session->source_dirty() &&
                !cr.session->source_dirty(),
            "Stored pending generation classifies committed candidates");
    require(ds.acknowledge_save(*di.ticket, owner, saved_doc).accepted &&
                cs.acknowledge_save(*ci.ticket, saved_comp).accepted,
            "Actual CAS readbacks acknowledged");
    require(doc_store->publish(ds.checkpoint()).durable &&
                comp_store->publish(cs.checkpoint()).durable,
            "Acknowledged generation published without replacing pending one");
    require(doc_store->inventory().generations.size() == 2 &&
                comp_store->inventory().generations.size() == 2,
            "Both sides of savepoint remain available");
    auto clean_doc =
        DocumentChangeSession::restore(doc_store->latest(), owner, saved_doc);
    auto clean_comp =
        CompositionChangeSession::restore(comp_store->latest(), saved_comp);
    require(clean_doc.session && clean_comp.session &&
                !clean_doc.session->dirty() && !clean_comp.session->dirty(),
            "Latest acknowledged generations resume clean sources");
    // A coordinator must not start the authorial writer unless checkpoint
    // publish returned durable. The fixture exercises this guard with ENOSPC.
    auto pending =
        ds.prepare({ChangePurpose::AuthorialReorder,
                    {ReorderOccurrences{ds.graph().root_layout_id,
                                        {ds.graph().layouts[0].children[1]},
                                        ds.graph().layouts[0].children[0]}}});
    require(pending.ticket &&
                ds.apply(*pending.ticket, owner, saved_doc).accepted,
            "Second private change");
    auto next = ds.begin_save();
    require(bool(next.ticket), "Second save barrier");
    StoreOptions disk_full;
    disk_full.inject_errno = [](IoPoint p) {
      return p == IoPoint::Write ? ENOSPC : 0;
    };
    auto failed = CheckpointStore::create_new(fixture.root / "failed-store",
                                              doc_scope, disk_full);
    const auto outcome = failed->publish(ds.checkpoint());
    bool writer_called{};
    if (outcome.durable) {
      writer_called = true;
      (void)service.writing().update_document_if_unchanged(
          next.ticket->candidate(), next.ticket->expected());
    }
    require(!outcome.durable && !writer_called && ds.save_pending() &&
                persistence::encode_document_snapshot(
                    *service.writing().document(d.id)) ==
                    persistence::encode_document_snapshot(saved_doc),
            "Failed durable checkpoint prevents authorial write and preserves "
            "pending work");
    require(
        ds.cancel_save(*next.ticket, owner, saved_doc).accepted &&
            ds.source_dirty(),
        "Untouched origin permits explicit cancellation without losing work");
    {
      persistence::SqliteDatabase db(
          persistence::ProjectDatabaseRepository::database_path(path));
      require(persistence::SchemaMigrator{}.current_version(db) == 37 &&
                  db.query_text("PRAGMA integrity_check") == "ok",
              "Schema37 integrity preserved");
      auto fk = db.prepare("PRAGMA foreign_key_check");
      require(!fk.step(), "Foreign keys intact");
    }
    std::cout << "INDE-OBJ-01: durable store + real repository integration "
                 "passed; pre-CAS and post-ack generations, independent "
                 "Document/Composition stores, checkpoint-failure write "
                 "barrier and schema37 integrity verified\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
