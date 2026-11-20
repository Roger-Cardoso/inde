#include "object_change_session.hpp"
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
std::map<std::string,std::string> files(const fs::path &root) {
  std::map<std::string,std::string> r;
  for(const auto &e:fs::recursive_directory_iterator(root)) {
    if(!e.is_regular_file()) continue;
    std::ifstream input(e.path(),std::ios::binary); require(bool(input),"Readable fixture file");
    r.emplace(e.path().lexically_relative(root).string(),std::string(std::istreambuf_iterator<char>(input),{}));
  }
  return r;
}
struct Fixture {
  fs::path root=fs::temp_directory_path()/("inde-obj01-change-session-"+project::new_uuid());
  Fixture(){require(fs::create_directory(root),"Fresh fixture directory");g_setenv("XDG_CONFIG_HOME",(root/"config").c_str(),true);}
  ~Fixture(){std::error_code ignored;fs::remove_all(root,ignored);}
};
}
int main(){
  try{
    Fixture fixture; const auto path=fixture.root/"Synthetic.inde";
    application::ProjectService service;service.create(path,"Sessões OBJ-01 sintéticas");
    const auto owner=service.current()->manifest().project_id;
    auto d=service.writing().create_document("Documento privado");d.content="A🙂\nBé\nC";
    d.paragraphs=project::identify_paragraphs(d.content);d.formatting={{project::DocumentTextStyle::Bold,0,2,{}}};
    d.anchors={{project::new_uuid(),"Declarada",0,2}};d=service.writing().update_document(d);
    auto stored=service.compositions().create("Montagem privada");
    const auto live=service.compositions().import(stored.value,d,0,project::utf8_character_count(d.content));
    const auto block=stored.value.blocks.occurrence(live).block_id;const auto old=stored.value.blocks.block(block).current_revision;
    const auto pin=stored.value.blocks.place(block);stored.value.blocks.pin(pin,old);
    const auto ref=stored.value.blocks.place(block);stored.value.blocks.reference_only(ref);
    const auto fork=stored.value.blocks.place(block);stored.value.blocks.fork(fork,old);
    const auto hidden=stored.value.blocks.place(block);stored.value.blocks.set_hidden(hidden,true);
    stored.value.blocks.revise(block,old,"APLICADO NOVO🙂\n");stored=service.compositions().save(stored);
    stored.value.drafts[block]={stored.value.blocks.block(block).current_revision,"RASCUNHO PRIVADO NÃO APLICADO"};
    stored=service.compositions().save(stored,true);const auto id=stored.value.blocks.composition_id();
    const auto doc_bytes=persistence::encode_document_snapshot(d),comp_bytes=persistence::encode_composition(stored.value);
    const auto before=files(path);
    auto dv=compat::read_document(owner,*service.writing().document(d.id));
    auto cv=compat::read_composition(service.compositions().load(id));require(dv.projection && cv.projection,"Real stored sources captured");
    DocumentChangeSession ds(*dv.projection,{0,2});CompositionChangeSession cs(*cv.projection);
    auto stale=ds.prepare({});
    auto visual_d=ds.prepare({ChangePurpose::VisualOnly,{ChangeLayout{ds.graph().root_layout_id,LayoutKind::Row}}});
    auto visual_c=cs.prepare({ChangePurpose::VisualOnly,{ChangeLayout{cs.graph().root_layout_id,LayoutKind::Row}}});
    require(visual_d.ticket && visual_c.ticket,"Visual plans prepared");
    require(ds.apply(*visual_d.ticket,owner,*service.writing().document(d.id)).accepted &&
            cs.apply(*visual_c.ticket,service.compositions().load(id)).accepted,"Visual plans applied only to working state");
    require(!ds.apply(*stale.ticket,owner,d).accepted,"Previously prepared ticket invalidated");
    auto move_d=ds.prepare({ChangePurpose::AuthorialReorder,{ReorderOccurrences{ds.graph().root_layout_id,
          {ds.graph().layouts[0].children[2]},ds.graph().layouts[0].children[0]}}});
    auto move_c=cs.prepare({ChangePurpose::AuthorialReorder,{ReorderOccurrences{cs.graph().root_layout_id,
          {cs.graph().layouts[0].children.back()},cs.graph().layouts[0].children.front()}}});
    require(move_d.ticket && move_c.ticket,"Authorial plans prepared over visual arrangement");
    const auto expected_d=persistence::encode_document_snapshot(move_d.ticket->plan().preview().document);
    const auto expected_c=persistence::encode_composition(move_c.ticket->plan().preview());
    require(ds.apply(*move_d.ticket,owner,*service.writing().document(d.id)).accepted &&
            cs.apply(*move_c.ticket,service.compositions().load(id)).accepted,"Authorial candidates applied exactly");
    require(ds.current().document.content=="C\nA🙂\nBé" && ds.source_dirty() && cs.source_dirty(),"Working source order changed");
    const auto final_d=ds.graph(),final_c=cs.graph();dv.projection.reset();cv.projection.reset();
    for(int i=0;i<2;++i)require(ds.undo(ds.revision(),owner,*service.writing().document(d.id)).accepted &&
            cs.undo(cs.revision(),service.compositions().load(id)).accepted,"Mixed steps undone together");
    require(!ds.dirty() && !cs.dirty() && persistence::encode_document_snapshot(ds.current().document)==doc_bytes &&
            persistence::encode_composition(cs.current())==comp_bytes,"Exact source origin restored");
    for(int i=0;i<2;++i)require(ds.redo(ds.revision(),owner,*service.writing().document(d.id)).accepted &&
            cs.redo(cs.revision(),service.compositions().load(id)).accepted,"Mixed steps replayed together");
    require(persistence::encode_document_snapshot(ds.current().document)==expected_d &&
            persistence::encode_composition(cs.current())==expected_c && ds.graph()==final_d && cs.graph()==final_c,"Exact candidate and graphs restored");
    require(files(path)==before,"Prepare/apply/undo/redo leave ALL project file bytes unchanged");
    const auto loaded=service.compositions().load(id);
    require(loaded.revision==stored.revision && loaded.sha256==stored.sha256 && persistence::encode_composition(loaded.value)==comp_bytes &&
            persistence::encode_document_snapshot(*service.writing().document(d.id))==doc_bytes,"No working state masquerades as persisted state");
    {
      persistence::SqliteDatabase db(persistence::ProjectDatabaseRepository::database_path(path));
      require(persistence::SchemaMigrator{}.current_version(db)==37 && db.query_text("PRAGMA integrity_check")=="ok","Schema 37/integrity intact");
      auto fk=db.prepare("PRAGMA foreign_key_check");require(!fk.step(),"Foreign keys intact");
    }
    auto fresh_d=*service.writing().document(d.id);fresh_d.title+=" escritor sintético";fresh_d=service.writing().update_document(fresh_d);
    const auto dr=ds.revision();require(!ds.undo(dr,owner,fresh_d).accepted && ds.revision()==dr &&
            persistence::encode_document_snapshot(ds.current().document)==expected_d,"External durable document conflict blocks replay without overwrite");
    auto fresh_c=service.compositions().load(id);fresh_c.value.drafts[block].text+=" outro escritor";fresh_c=service.compositions().save(fresh_c,true);
    const auto cr=cs.revision();require(!cs.undo(cr,fresh_c).accepted && cs.revision()==cr &&
            persistence::encode_composition(cs.current())==expected_c,"External durable draft conflict blocks replay without overwrite");
    std::cout<<"INDE-OBJ-01: working-session repository integration passed; apply/undo/redo in memory, all project file bytes unchanged, schema37 integrity, durable external conflicts refused\n";
    return 0;
  }catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
