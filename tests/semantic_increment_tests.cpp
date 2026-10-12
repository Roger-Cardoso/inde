#include "../src/persistence/recovery_codec.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/project/document_state.hpp"
#include "inde/persistence/semantic_codec.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>
#include <cstdlib>

using namespace inde;
using S = application::TextSemanticService;
using K = project::SemanticFieldKind;
template <class F> void rejects(F f) {
  bool failed = false; try { f(); } catch (const std::exception &) { failed = true; }
  assert(failed);
}
int main() {
  const auto root = std::filesystem::temp_directory_path() / ("inde-semantic2-" + project::new_uuid());
  std::filesystem::create_directories(root);
  setenv("XDG_CONFIG_HOME", (root / "config").c_str(), 1);
  {
    application::ProjectService service; service.create(root / "project.inde", "QA semântica 2");
    const auto types = service.writing().semantic_types();
    assert(types.size() == project::builtin_annotation_types().size());
    auto perspective = *project::find_annotation_type(types, "inde.semantic.perspective", 1);
    auto narration = *project::find_annotation_type(types, "inde.semantic.narration", 1);
    assert(perspective.version == 1 && perspective.allow_overlap && perspective.system);
    assert(persistence::decode_semantic_type(persistence::encode_semantic_type(perspective)) == perspective);
    auto ets = service.narrative().entity_types();
    auto character = std::find_if(ets.begin(), ets.end(), [](auto &t) { return t.key=="character"; });
    auto location = std::find_if(ets.begin(), ets.end(), [](auto &t) { return t.key=="location"; });
    assert(character != ets.end() && location != ets.end());
    auto who = service.narrative().create_entity(character->id, "Focalizadora QA");
    auto where = service.narrative().create_entity(location->id, "Local QA");
    auto d = service.writing().create_document("Alvo de perspectiva");
    d.content = "Água 🙂 amanhã. Uma visão situada.";
    d.perspective = "Voz livre autoral: hipótese pessoal, sem classificação.";
    d.formatting = {{project::DocumentTextStyle::Bold, 0, 6}};
    d = service.writing().update_document(d); const auto baseline = d;
    d = S::create(d, perspective, 0, 6, {});
    const auto id = d.semantics.annotations.back().id;
    assert(d.semantics.annotations.back().status == "incomplete");
    assert(project::semantic_diagnostics(d, types).size() == 2);
    d = S::update(d, id, {{"person",{K::Enum,{"third_person"}}},
                         {"focalization",{K::Enum,{"internal"}}}}, types);
    auto diagnostics = project::semantic_diagnostics(d, types);
    assert(d.semantics.annotations.back().status == "active" && diagnostics.size()==1);
    assert(diagnostics.front().code == "perspective.viewpoint.unspecified" && !diagnostics.front().integrity);
    d = service.writing().update_document(d); // advisory is not a forced binding
    d = S::update(d, id, {{"person",{K::Enum,{"third_person"}}},
                         {"focalization",{K::Enum,{"internal"}}},
                         {"viewpoint",{K::Entity,{who.id}}},
                         {"note",{K::Text,{"Declaração humana 🙂"}}}}, types);
    assert(project::semantic_diagnostics(d,types).empty());
    assert(d.perspective==baseline.perspective && d.content==baseline.content);
    assert(d.formatting.front().end_offset==6 && d.semantics.annotations.front().origin=="human");
    auto bad = d;
    bad = S::update(bad,id,{{"viewpoint",{K::Entity,{where.id}}}},types);
    rejects([&] { service.writing().validate_semantic_document(bad); });
    rejects([&] { S::update(d,id,{{"person",{K::Enum,{"automatic"}}}},types); });
    rejects([&] { S::update(d,id,{{"focalization",{K::Enum,{"internal","external"}}}},types); });
    for (const auto &person : {"first_person","second_person","third_person"})
      for (const auto &focal : {"internal","external","omniscient","variable"}) {
        auto choice = S::update(d,id,{{"person",{K::Enum,{person}}},
                                    {"focalization",{K::Enum,{focal}}}},types);
        service.writing().validate_semantic_document(choice);
      }
    d = service.writing().update_document(d);
    application::DocumentEditSession history; history.open({baseline,{0,6}});
    assert(history.commit({d,{0,6}},"Perspectiva situada"));
    // Undo restores authorial state, not a historical persistence timestamp.
    // Do not make this assertion depend on both writes sharing one UTC second.
    assert(project::same_document_state(history.undo()->document,baseline,false));
    assert(project::same_document_state(history.redo()->document,d,false));
    auto edited = application::replace_selection({d,{0,0}},0,0,"🙂 ");
    application::SemanticAnchorEngine::inspect(edited.document);
    assert(edited.document.anchors.front().end_offset==8);
    assert(edited.document.semantics.anchors.front().state==project::AnchorResolution::Resolved);
    auto removed = application::replace_selection({d,{0,6}},0,6,"");
    assert(removed.document.semantics.anchors.front().state==project::AnchorResolution::Orphaned);
    assert(removed.document.semantics.annotations.front().id==id);
    auto repaired = S::reanchor(removed.document,id,0,3);
    assert(repaired.semantics.anchors.front().state==project::AnchorResolution::Resolved);
    // Incoming/outgoing pagination, identity joins, archive preservation and
    // parameter binding. Each source is independent and may only mutate itself.
    std::vector<project::Document> sources;
    for (int i=0;i<27;++i) {
      auto source=service.writing().create_document("Origem " + std::to_string(i));
      source.content="Retomada explícita 🙂.";
      source=S::create(source,narration,0,8,{});
      const auto sid=source.semantics.annotations.front().id;
      source=S::relate(source,sid,id,"inde.semantic.callback");
      source.semantics.relations.back().notes="Motivo da retomada, declarado.";
      rejects([&] { S::relate(source,sid,id,"inde.semantic.callback"); });
      rejects([&] { S::relate(source,sid,sid,"inde.semantic.reference"); });
      rejects([&] { S::relate(source,sid,id,"inde.semantic.inferred"); });
      sources.push_back(service.writing().update_document(source));
    }
    persistence::SemanticRelationQuery query; query.annotation_id=id; query.limit=12;
    auto page=service.writing().semantic_relations(query);
    assert(page.total==27 && page.rows.size()==12);
    std::set<std::string> seen;
    for (std::size_t at : {std::size_t(0),std::size_t(12),std::size_t(24)}) {
      query.offset=at; page=service.writing().semantic_relations(query);
      assert(page.total==27 && page.rows.size()==(at==24 ? 3 : 12));
      for (const auto &r:page.rows) {
        assert(seen.insert(r.relation_id).second && r.target.annotation_id==id);
        assert(r.target.document_id==d.id && r.target.document_title==d.title);
        assert(r.source.document_id!=d.id && r.type_key=="inde.semantic.callback");
      }
    }
    query.offset=0; query.exclude_source_document_id=sources.front().id;
    assert(service.writing().semantic_relations(query).total==26);
    query.exclude_source_document_id.reset(); query.limit=0;
    rejects([&] { static_cast<void>(service.writing().semantic_relations(query)); });
    query.limit=201; rejects([&] { static_cast<void>(service.writing().semantic_relations(query)); });
    query.limit=12; query.offset=10000001;
    rejects([&] { static_cast<void>(service.writing().semantic_relations(query)); });
    query.offset=0; query.annotation_id="' OR 1=1 --";
    assert(service.writing().semantic_relations(query).total==0);
    query.annotation_id=sources.front().semantics.annotations.front().id;
    query.direction=persistence::SemanticRelationDirection::Outgoing;
    page=service.writing().semantic_relations(query); assert(page.total==1 && page.rows.front().target.annotation_id==id);
    persistence::SemanticUseQuery lookup; lookup.annotation_id=id; lookup.include_archived=true;
    assert(service.writing().semantic_uses(lookup).total==1);
    lookup.exclude_document_id=d.id; assert(service.writing().semantic_uses(lookup).total==0);
    auto archived=S::archive(d,id,true); archived=service.writing().update_document(archived);
    query.annotation_id=id; query.direction=persistence::SemanticRelationDirection::Incoming;
    assert(service.writing().semantic_relations(query).rows.front().target.status=="archived");
    const auto state_before=*service.writing().document(d.id);
    rejects([&] { static_cast<void>(service.writing().update_document(S::remove(state_before,id))); });
    rejects([&] { service.writing().delete_document(d.id); });
    rejects([&] { service.narrative().delete_entity(who.id); });
    assert(project::same_document_state(state_before,*service.writing().document(d.id)));
    auto foreign=sources.front(); foreign.semantics.relations.front().target_annotation_id=project::new_uuid();
    rejects([&] { static_cast<void>(service.writing().update_document(foreign)); });
    assert(project::same_document_state(sources.front(),*service.writing().document(foreign.id)));
    project::RecoveryRecord checkpoint{service.current()->manifest().project_id,project::new_uuid(),3,2,
                                       {sources.front(),0,8},sources.front(),"pending","",""};
    auto encoded=persistence::recovery::encode(checkpoint);
    assert(project::same_document_state(sources.front(),persistence::recovery::decode(encoded,
                                          persistence::recovery::checksum(encoded)).state.document));
    checkpoint.state.document=state_before; checkpoint.baseline=baseline;
    encoded=persistence::recovery::encode(checkpoint);
    assert(project::same_document_state(state_before,persistence::recovery::decode(encoded,
                                          persistence::recovery::checksum(encoded)).state.document));
    const auto original=service.current()->path(); const auto project_id=service.current()->manifest().project_id;
    service.save_as(root/"copy.inde");
    assert(service.current()->manifest().project_id!=project_id);
    assert(service.writing().semantic_relations(query).total==27);
    service.close(); service.open(original);
    assert(service.writing().document(d.id)->perspective==baseline.perspective);
    for (auto &source:sources) {
      source=S::unrelate(source,source.semantics.relations.front().id);
      source=service.writing().update_document(source);
      service.writing().delete_document(source.id);
    }
    assert(service.writing().semantic_relations(query).total==0);
    service.writing().delete_document(d.id); service.narrative().delete_entity(who.id);
    service.narrative().delete_entity(where.id); service.close();
  }
  std::filesystem::remove_all(root);
  std::cout<<"PASS: situated perspective/legacy independence, canonical explicit focalizers, "
              "advisory diagnostics, Unicode anchors, undo/redo, relations both directions, "
              "27-source pagination, SQL literal safety, protected targets/atomic rollback, "
              "archive identity, checkpoint payload and save-as scope\n";
}
