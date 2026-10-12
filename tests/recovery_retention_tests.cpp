#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/document_recovery_store.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
#include "../src/persistence/recovery_codec.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

using namespace inde;
template<class F> void rejects(F f) {
  bool rejected=false; try { f(); } catch (const std::exception &) { rejected=true; }
  assert(rejected);
}
int main() {
  char folder[]="/tmp/inde-retention-XXXXXX";
  const auto *root=::mkdtemp(folder); assert(root);
  assert(::setenv("XDG_CONFIG_HOME",(std::filesystem::path(root)/"config").c_str(),1)==0);
  application::ProjectService service;
  service.create(std::filesystem::path(root)/"Checkpoint.inde","QA retenção");
  const auto owner=*service.current();
  auto base=service.writing().create_document("Água — 🙂");
  base.content="Água 🙂 descrição autoral.";
  base=application::TextSemanticService::create(base,project::builtin_annotation_types().front(),0,4,{});
  base=service.writing().update_document(base);
  persistence::DocumentRecoveryStore store;
  auto make=[&](bool clean) {
    auto state=base; if (!clean) state.content+=" Rascunho não salvo.";
    project::RecoveryRecord r{owner.manifest().project_id,project::new_uuid(),1,0,
                              {state,5,1},base,"pending",{}, {}};
    { persistence::RecoveryLease lease(owner,r.session_id); store.checkpoint(owner,r); }
    return *store.load(owner,r.session_id);
  };
  const auto pending=make(false), clean=make(true), clean2=make(true);
  auto page=store.retention(owner,30);
  assert(page.items.size()==3 && page.allocated_bytes>0);
  for (const auto &e : page.items) assert(e.exportable && !e.removable);
  page=store.retention(owner,0);
  auto find=[&](const std::string &id) {
    for (const auto &e : store.retention(owner,0).items) if (e.session_id==id) return e;
    throw std::runtime_error("Missing retention entry");
  };
  auto a=find(clean.session_id), b=find(clean2.session_id), p=find(pending.session_id);
  assert(a.removable && b.removable && !p.removable);
  rejects([&] { store.purge_exported(owner,{a},0); });
  assert(store.load(owner,a.session_id));
  {
    persistence::RecoveryLease active(owner,a.session_id,false);
    auto entry=find(a.session_id); assert(!entry.exportable && !entry.removable);
    rejects([&] { static_cast<void>(store.export_record(owner,a.session_id,a.checksum)); });
    rejects([&] { store.purge_exported(owner,{a},0); });
  }
  const auto archive=store.export_record(owner,a.session_id,a.checksum);
  const auto original=persistence::DocumentRecoveryStore::read_export(archive);
  assert(project::same_document_state(original.state.document,clean.state.document));
  assert(original.state.cursor==5 && original.state.bound==1 && original.checksum==a.checksum);
  assert(store.export_record(owner,a.session_id,a.checksum)==archive);
  // Idempotent export; pending export allowed, but never eligible for deletion.
  const auto pending_archive=store.export_record(owner,p.session_id,p.checksum);
  rejects([&] { store.purge_exported(owner,{p},0); });
  // A later failure rolls back earlier deletes in this same batch.
  rejects([&] { store.purge_exported(owner,{a,b},0); });
  assert(store.load(owner,a.session_id) && store.load(owner,b.session_id));
  auto stale=a; stale.checksum=std::string(64,'0');
  rejects([&] { store.purge_exported(owner,{stale},0); });
  rejects([&] { store.purge_exported(owner,{a,a},0); });
  rejects([&] { store.purge_exported(owner,{a},30); });
  // Calendar boundaries are decisions on checked payload metadata, not SQL
  // display columns. Old closed rows may be selected; future/invalid stay safe.
  const auto aged=make(true);
  const auto stamp=[&](const std::string &date) {
    auto record=*store.load(owner,aged.session_id); record.updated_at=date;
    const auto payload=persistence::recovery::encode(record);
    persistence::SqliteDatabase db(store.database_path(owner));
    auto q=db.prepare("UPDATE recovery_records SET updated_at=?,payload=?,checksum=? WHERE session_id=?");
    q.bind(1,date);q.bind(2,payload);q.bind(3,persistence::recovery::checksum(payload));
    q.bind(4,aged.session_id);q.run();
  };
  stamp("1970-01-01T00:00:00Z");
  for (const auto &entry : store.retention(owner,90).items)
    if (entry.session_id==aged.session_id) assert(entry.removable);
  stamp("2999-01-01T00:00:00Z"); assert(!find(aged.session_id).removable);
  stamp("2026-02-30T00:00:00Z"); assert(!find(aged.session_id).removable);
  stamp("1970-01-01T00:00:00Z");
  // Failure before publication cannot consume journal evidence.
  const auto export_folder=archive.parent_path();
  std::filesystem::permissions(export_folder,std::filesystem::perms::owner_read|std::filesystem::perms::owner_exec);
  rejects([&] { static_cast<void>(store.export_record(owner,b.session_id,b.checksum)); });
  std::filesystem::permissions(export_folder,std::filesystem::perms::owner_all);
  assert(store.load(owner,b.session_id));
  {
    std::ifstream input(archive); std::string bytes((std::istreambuf_iterator<char>(input)),{});
    auto damaged=bytes; const auto pos=damaged.find("exported_at"); assert(pos!=std::string::npos);
    damaged[pos]='X';
    { std::ofstream output(archive,std::ios::trunc); output<<damaged; }
    rejects([&] { static_cast<void>(store.read_export(archive)); });
    rejects([&] { store.purge_exported(owner,{a},0); });
    // Export never overwrites a damaged existing file.
    rejects([&] { static_cast<void>(store.export_record(owner,a.session_id,a.checksum)); });
    { std::ofstream output(archive,std::ios::trunc); output<<bytes; }
  }
  assert(store.purge_exported(owner,{a},0)==1 && !store.load(owner,a.session_id));
  assert(std::filesystem::exists(archive) && std::filesystem::exists(pending_archive));
  assert(store.load(owner,p.session_id)->status=="pending");
  assert(project::same_document_state(*service.writing().document(base.id),base));
  // Corrupt rows are protected individually, not silently rewritten or dropped.
  {
    persistence::SqliteDatabase db(store.database_path(owner));
    auto q=db.prepare("UPDATE recovery_records SET checksum='invalid' WHERE session_id=?");
    q.bind(1,b.session_id); q.run();
  }
  const auto corrupt=find(b.session_id); assert(!corrupt.exportable && !corrupt.removable);
  rejects([&] { store.purge_exported(owner,{corrupt},0); });
  auto link=std::filesystem::path(root)/"symbolic.json";
  std::filesystem::create_symlink(archive,link);
  rejects([&] { static_cast<void>(store.read_export(link)); });
  const auto deep=std::filesystem::path(root)/"deep.json";
  { std::ofstream output(deep); output<<std::string(100000,'[')<<std::string(100000,']'); }
  rejects([&] { static_cast<void>(store.read_export(deep)); });
  { std::ofstream output(deep,std::ios::trunc); output<<std::string("\xff",1); }
  rejects([&] { static_cast<void>(store.read_export(deep)); });
  // Duplicate metadata is not allowed to advertise a different checkpoint.
  {
    persistence::SqliteDatabase db(store.database_path(owner));
    auto q=db.prepare("UPDATE recovery_records SET title='wrong title' WHERE session_id=?");
    q.bind(1,p.session_id);q.run();
  }
  const auto metadata=find(p.session_id); assert(!metadata.exportable && !metadata.removable);
  rejects([&] { static_cast<void>(store.load(owner,p.session_id)); });
  service.close();
  std::cout<<"PASS: explicit retention, pending/active/corrupt protection, Unicode/semantic "
              "portable export, integrity, no overwrite, stale tokens, atomic rollback, recoverable purge\n";
  // Exact test-owned container created by mkdtemp; never a user path.
  std::filesystem::remove_all(root);
}
