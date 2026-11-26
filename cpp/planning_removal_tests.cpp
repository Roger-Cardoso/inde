#include "inde/application/contextual_planning.hpp"
#include "inde/application/project_service.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/document_state.hpp"
#include <glib.h>
#include <iostream>

using namespace inde;
void require(bool yes, const char *what) { if (!yes) throw std::runtime_error(what); }
template<class F> void refuses(F f) {
  bool failed=false;
  try { f(); } catch (const std::exception &) { failed=true; }
  require(failed,"Expected refusal");
}
int main() {
  const auto root=std::filesystem::temp_directory_path()/("inde-delete-planning-"+project::new_uuid());
  std::filesystem::create_directories(root);
  g_setenv("XDG_CONFIG_HOME",(root/"config").c_str(),true);
  try {
    application::ProjectService s;
    const auto path=root/"Sintetico.inde";
    s.create(path,"Planejamento sintético");
    application::ContextualPlanningService c(s);
    const auto ip=s.create_intellectual_property("Teste","","").id;
    const auto work=s.create_work(ip,"Obra","","","pt-BR","Planejamento").id;
    const auto plan=c.create_plan(work,"Remover"), other=c.create_plan(work,"Preservar");
    auto snap=[&]{return c.snapshot(plan);};
    const auto a=c.create_moment(snap(),"Compartilhado","Conteúdo conservado");
    const auto b=c.create_moment(snap(),"Segundo","");
    const auto moment=snap().participations.front().moment_id;
    const auto reuse=c.reuse_moment(c.snapshot(other),c.moment(moment));
    auto items=application::contextual_visual_layout(snap());
    application::ContextualVisualItem lane;
    lane.id=project::new_uuid(); lane.kind="lane";lane.label="Faixa";
    items.push_back(lane);
    c.save_visual(snap(),items);
    c.set_role(snap(),a,"Protagonista",true);
    const auto initial=snap();
    auto undo=[&]{
      const auto impact=s.reversal_impact(s.project_operations().front().id);
      require(impact.allowed,"Reversal refused");
      s.confirm_project_operation(impact);
    };
    c.remove_visual_selection(snap(),{a,lane.id});
    require(snap().participations.size()==1 && snap().visual.size()==1,"Mixed selection not removed");
    require(c.moment(moment).description=="Conteúdo conservado","Shared content removed");
    require(c.snapshot(other).participations.front().id==reuse,"Other use changed");
    undo();
    require(snap().revision==initial.revision,"Undo did not restore complete selection and role");
    // Invalid, duplicate, foreign and stale IDs must not remove any subset.
    for(const auto &ids:std::vector<std::vector<std::string>>{{},{a,a},{a,"absent"},{a,reuse}}) {
      refuses([&]{c.remove_visual_selection(snap(),ids);});
      require(snap().revision==initial.revision,"Partial removal");
    }
    auto stale=snap(); c.set_note(snap(),b,"Nova nota");
    refuses([&]{c.remove_visual_selection(stale,{a});});
    refuses([&]{c.remove_plan(stale);});
    const auto declaration=c.declare(snap(),b,"fact","Estado","Ativo");
    auto blocked=snap();
    refuses([&]{c.remove_visual_selection(blocked,{a,b,lane.id});});
    require(snap().revision==blocked.revision,"Linked selection partly removed");
    c.remove_declaration(snap(),declaration);
    // detach also uses complete dependency checks; visual IDs can differ from use IDs.
    items=application::contextual_visual_layout(snap());
    for(auto &v:items) if(v.participation_id==b) v.id=project::new_uuid();
    c.save_visual(snap(),items);
    c.detach(snap(),b);
    require(snap().participations.size()==1,"Detach alias failed");
    undo();
    // Distinct ID namespaces can legally overlap. A visual item named like a
    // different participation must never make the list detach the wrong use.
    items=application::contextual_visual_layout(snap());
    for(auto &v:items) if(v.participation_id==a) v.id=b;
    c.save_visual(snap(),items);
    c.detach(snap(),b);
    require(snap().participations.size()==1 && snap().participations.front().id==a,
            "Detach confused participation and visual IDs");
    undo();
    c.remove_visual_selection(snap(),{b});
    require(snap().participations.size()==1 && snap().participations.front().id==b,
            "Canvas removal confused visual and participation IDs");
    undo();
    const auto bridge=c.add_bridge(c.snapshot(other),reuse,snap(),a,"context","");
    blocked=snap();
    refuses([&]{c.remove_plan(blocked);});
    refuses([&]{c.detach(blocked,a);});
    require(snap().revision==blocked.revision,"External bridge guard mutated plan");
    c.remove_bridge(c.snapshot(other),bridge);
    const auto interior=c.develop(snap(),a);
    refuses([&]{c.remove_plan(c.snapshot(interior));});
    // Deleting an enclosing plan preserves its shared moment and developed interior.
    const auto slot=c.create_slot(snap(),"Escolha");
    const auto option=c.create_option(snap(),slot,"Opção");
    c.choose_option(snap(),slot,option); c.assign_option(snap(),b,option);
    c.declare(snap(),b,"fact","Local","Sim");
    auto doc=s.writing().create_document("Documento preservado");
    doc.content="Texto vinculado preservado.";
    doc.paragraphs=project::identify_paragraphs(doc.content);
    doc=s.writing().update_document(doc);
    const auto link=c.associate_text(snap(),a,c.text_document(doc.id));
    blocked=snap(); refuses([&]{c.detach(blocked,a);});
    require(snap().revision==blocked.revision,"Linked document guard mutated plan");
    c.remove_text_link(snap(),link);
    c.associate_range(snap(),a,doc,0,5,"Trecho de escrita");
    blocked=snap(); refuses([&]{c.detach(blocked,a);});
    require(snap().revision==blocked.revision,"Anchor guard mutated plan");
    c.add_dependency(snap(),a,b,"context","Ligação local");
    const auto document_bytes=persistence::encode_document_snapshot(*s.writing().document(doc.id));
    const auto complete=snap(), preserved=c.snapshot(other);
    c.remove_plan(complete);
    refuses([&]{c.snapshot(plan);});
    require(c.snapshot(interior).plan_id==interior,"Interior removed");
    require(c.moment(moment).title=="Compartilhado","Moment removed with plan");
    require(c.snapshot(other).revision==preserved.revision,"Unrelated plan modified");
    require(persistence::encode_document_snapshot(*s.writing().document(doc.id))==document_bytes,"Deleting plan changed linked writing");
    undo();
    require(snap().revision==complete.revision,"Plan reversal incomplete");
    c.remove_plan(snap());
    s.close(); s.open(path);
    refuses([&]{c.snapshot(plan);});
    require(c.snapshot(other).participations.size()==1,"Reopen lost unrelated use");
    persistence::SqliteDatabase db(path/"data/project.sqlite3");
    require(db.query_text("PRAGMA integrity_check")=="ok","Integrity failure");
    require(!db.prepare("PRAGMA foreign_key_check").step(),"Dangling reference");
    require(db.query_integer("SELECT MAX(version) FROM schema_migrations")==37,"Unexpected migration");
    s.close();
    std::filesystem::remove_all(root);
    std::cout<<"Planning removal: selection, guards, shared content, atomicity, reversal, restart and integrity OK\n";
    return 0;
  } catch(const std::exception &e) {
    std::cerr<<e.what()<<"\nSynthetic evidence: "<<root<<"\n"; return 1;
  }
}
