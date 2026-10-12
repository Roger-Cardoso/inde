#include "inde/experimental/text_blocks.hpp"
#include "inde/experimental/lab_source.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/application/editorial_review.hpp"
#include "inde/project/document_state.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
using namespace inde;
using namespace inde::experimental;
template <class F> void rejects(F f) { bool failed{}; try { f(); } catch (const std::exception &) { failed = true; } assert(failed); }
project::Document sample(const std::string &title, const std::string &body) {
  project::Document d; d.id = project::new_uuid(); d.title = title; d.content = body;
  d.created_at = d.updated_at = project::utc_now();
  d.formatting = {{project::DocumentTextStyle::Bold, 0, 3, {}}}; return d;
}
void operations() {
  auto a = sample("Chegada", "Lia chegou.\nUma chave apareceu.\n");
  auto b = sample("Carta", "Uma carta mudou a história.\n");
  const auto original_a = a, original_b = b;
  TextBlocksLab lab; const auto ba = lab.import_document(a, 0, project::utf8_character_count(a.content));
  const auto bb = lab.import_document(b, 0, project::utf8_character_count(b.content));
  lab.rename_block(ba, "Cena da chegada"); const auto pa = lab.place(ba), pb = lab.place(bb), duplicate = lab.place(ba);
  ReadingProfile profile; assert(lab.project(profile).text == a.content + b.content + a.content);
  const auto v1 = lab.block(ba).current_revision; lab.use_version(duplicate, v1, v1);
  lab.revise(ba, v1, "Lia partiu.\n");
  assert(lab.revision(lab.occurrence(duplicate)).text == a.content);
  const auto v2 = lab.block(ba).current_revision;
  lab.use_version(pa, v1, v2); assert(lab.revision(lab.occurrence(pa)).id == v1);
  lab.follow_current(pa); assert(lab.revision(lab.occurrence(pa)).id == v2);
  const auto unchanged = lab.project(profile).text;
  rejects([&] { lab.split_selection(pa, 0, 999, "Inválido"); }); assert(lab.project(profile).text == unchanged);
  const auto selected = lab.split_selection(pa, 4, 10, "Verbo escolhido");
  assert(lab.project(profile).text == unchanged); assert(lab.block(lab.occurrence(selected).block_id).label == "Verbo escolhido");
  assert(lab.revision(lab.occurrence(duplicate)).text == a.content);
  lab.set_hidden(selected, true); assert(lab.project(profile).text != unchanged); lab.set_hidden(selected, false);
  const auto alt_a = lab.alternative(selected, "Partida tardia");
  const auto qa = lab.occurrence(alt_a); profile.selected_variants[*qa.variant_group] = *qa.variant;
  lab.revise(qa.block_id, lab.revision(qa).id, "voltou");
  const auto alt_b = lab.alternative(pb, "Outra carta"); const auto qb = lab.occurrence(alt_b);
  profile.selected_variants[*qb.variant_group] = *qb.variant;
  lab.revise(qb.block_id, lab.revision(qb).id, "Outra carta chegou.\n");
  auto combined = lab.project(profile).text; assert(combined.find("voltou") != std::string::npos && combined.find("Outra carta") != std::string::npos);
  auto base = lab.project({}).text; assert(base.find("voltou") == std::string::npos && base.find("Outra carta") == std::string::npos);
  const auto count = lab.blocks().size(); const auto promoted = lab.promote(alt_a, "Partida escolhida");
  assert(lab.blocks().size() == count + 1 && promoted != qa.block_id && !lab.occurrence(alt_a).variant);
  assert(lab.occurrence(selected).hidden && lab.block(qa.block_id).revisions.size() == 2);
  assert(lab.project(profile).text.find("voltou") != std::string::npos);
  auto output = materialize(lab, lab.project(profile), "Documento novo");
  assert(output.document.content == lab.project(profile).text && output.document.id != a.id && output.document.id != b.id);
  assert(project::same_document_state(a, original_a) && project::same_document_state(b, original_b));
  rejects([&] { static_cast<void>(lab.project({{}, {}, {{"foreign", *qa.variant}}})); });
  std::cout << "PASS workshop: author-chosen split, multiple sources/version selection, independent variant families, promotion, hidden originals\n";
}
void layers_and_publish() {
  const auto root = std::filesystem::temp_directory_path() / ("inde-workshop-tests-" + project::new_uuid());
  std::filesystem::create_directories(root); setenv("XDG_CONFIG_HOME", (root / "config").c_str(), 1);
  const auto path = root / "copy.inde"; application::ProjectService service; service.create(path, "Montagem QA");
  auto d = service.writing().create_document("Origem rica"); d.content = "Lia chegou.\nUma chave apareceu.\n";
  d.formatting = {{project::DocumentTextStyle::Bold, 0, 3, {}}}; d.inline_commands = {{"@", "nota", "Nota", "Modelo literal"}};
  auto types = service.writing().semantic_types(); const auto type = *project::find_annotation_type(types, "inde.semantic.narration", 1);
  d = application::TextSemanticService::create(d, type, 0, 3, {});
  d = application::TextSemanticService::create(d, type, 1, 15, {});
  d = application::TextSemanticService::relate(d, d.semantics.annotations[0].id, d.semantics.annotations[1].id, "inde.semantic.reference");
  auto axis = service.planning().create_time_axis("Tempo da origem"); auto point = service.planning().create_time_point(axis.id, 1, "Chegada");
  d = application::TextSemanticService::bind_fictional_time(d, d.semantics.annotations[0].id, axis.id, point.id);
  using K = project::SemanticFieldKind;
  d = application::TextSemanticService::create_document_perspective(d, {{"person", {K::Enum, {"third_person"}}}, {"focalization", {K::Enum, {"external"}}}});
  d = service.writing().update_document(d); const auto original = d;
  auto other = service.writing().create_document("Outra origem"); other.content = "Uma carta chegou.\n"; other = service.writing().update_document(other); service.close();
  auto cut = slice_layers(d, 0, 6); assert(!cut.warnings.empty()); assert(cut.document->semantics.annotations.size() == 1);
  assert(cut.document->content == "Lia ch" && cut.document->formatting.front().end_offset == 3);
  TextBlocksLab lab; auto block = lab.import_document(d, 0, project::utf8_character_count(d.content)); auto p = lab.place(block);
  auto whole = materialize(lab, lab.project({}), "Uma origem completa");
  assert(whole.document.semantics.annotations.size() == 3 && whole.document.semantics.relations.size() == 1);
  assert(whole.document.anchors[0].id != d.anchors[0].id);
  assert(whole.document.semantics.annotations[0].fictional_time[0].fictional_axis_id == axis.id);
  assert(whole.document.semantics.annotations[0].fictional_time[0].id != d.semantics.annotations[0].fictional_time[0].id);
  lab.place(block); auto duplicated = materialize(lab, lab.project({}), "Repetição");
  assert(duplicated.document.anchors.size() == d.anchors.size() * 2 && !duplicated.warnings.empty());
  assert(std::none_of(duplicated.document.semantics.annotations.begin(), duplicated.document.semantics.annotations.end(), [](auto &a) { return a.scope == project::AnnotationScope::Document; }));
  std::ofstream(path / "TEXT_BLOCKS_LAB_COPY") << "INDE_TEXT_BLOCKS_SPIKE_COPY_V1\n";
  LabSource publisher(path); rejects([&] { publisher.create_snapshot(whole.document); });
  std::ofstream(path / "TEXT_WORKSHOP_COPY") << "INDE_TEXT_WORKSHOP_COPY_V1\n";
  auto restored = publisher.create_snapshot(whole.document); assert(project::same_document_state(restored, whole.document));
  assert(project::same_document_state(publisher.load(d.id), original)); rejects([&] { publisher.create_snapshot(whole.document); });
  auto assembled = publisher.create_snapshot(duplicated.document); assert(assembled.content == d.content + d.content);
  assert(project::same_document_state(publisher.load(d.id), original));
  auto reviewed=application::set_editorial_state(d,"approved","Editora QA","");
  reviewed=application::propose_change(reviewed,0,3,"Mara","Sugestão apenas da fonte");
  TextBlocksLab reviewed_lab;auto reviewed_block=reviewed_lab.import_document(reviewed,0,project::utf8_character_count(reviewed.content));reviewed_lab.place(reviewed_block);
  auto reviewed_output=materialize(reviewed_lab,reviewed_lab.project({}),"Montagem não herda aprovação");
  assert(!reviewed_output.warnings.empty());
  assert(std::none_of(reviewed_output.document.semantics.annotations.begin(),reviewed_output.document.semantics.annotations.end(),[](const auto &a){return a.type_key=="inde.editor.review-state" || a.type_key=="inde.editor.suggestion";}));
  project::validate(reviewed_output.document);
  (void)p;
  std::cout << "PASS layers: styles/contained semantics/time/relations rekeyed, cut/global warnings, prepared-copy create/reopen, no overwrite/original changes\n";
}
int main() { operations(); layers_and_publish(); }
