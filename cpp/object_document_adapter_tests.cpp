#include "object_document_adapter.hpp"
#include "object_commands.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

namespace o = inde::objects;
namespace c = inde::objects::compat;
namespace p = inde::project;
namespace {
int failures{};
const std::string owner = "10000000-0000-4000-8000-000000000001";
void check(bool yes, const std::string &label) {
  if (!yes) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
bool code(const std::vector<o::Diagnostic> &ds, const std::string &s) {
  return std::any_of(ds.begin(), ds.end(), [&](const auto &d) { return d.code == s; });
}
p::Document document(std::string body = "Água🙂\nLinha oculta\n\nFim\n") {
  p::Document d;
  d.id = "20000000-0000-4000-8000-000000000001";
  d.title = "Fixture de compatibilidade OBJ-01";
  d.created_at = d.updated_at = "2026-11-20T15:00:00Z";
  d.content = std::move(body); d.paragraphs = p::identify_paragraphs(d.content);
  return d;
}
p::Document rich_document() {
  auto d = document();
  d.editorial_node_id = p::new_uuid(); d.group_id = p::new_uuid();
  d.word_goal = 1000; d.purpose = p::DocumentPurpose::Revision;
  d.revision_of_id = p::new_uuid(); d.revision_label = "Revisão explicitamente declarada";
  d.perspective = "Campo autoral preservado literalmente";
  d.formatting = {{p::DocumentTextStyle::Bold, 0, 5, {}},
                   {p::DocumentTextStyle::Foreground, 6, 11, "#112233"}};
  d.anchors = {{p::new_uuid(), "Âncora explícita", 0, 5},
               {p::new_uuid(), "Ponto final", 24, 24}};
  d.entity_references = {{p::new_uuid(), p::new_uuid(), d.anchors[0].id, "Referência declarada"}};
  d.inline_commands = {{"@", "nota", "Literal", "Texto que não deve ser executado"}};
  auto evidence = inde::application::SemanticAnchorEngine::evidence(d, d.anchors[0]);
  evidence.state = p::AnchorResolution::Ambiguous;
  evidence.reason = "Divergência preexistente; requer decisão humana";
  d.semantics.anchors.push_back(evidence);
  p::TextAnnotation a;
  a.id = p::new_uuid(); a.anchor_id = d.anchors[0].id; a.type_key = "inde.editor.comment";
  a.created_at = a.updated_at = d.created_at;
  a.fields["note"] = {p::SemanticFieldKind::Text, {"Comentário declarado"}};
  a.fields["state"] = {p::SemanticFieldKind::Enum, {"open"}};
  a.notes = "Notas de autoria"; a.opaque_payload = "Conteúdo opaco preservado";
  a.fictional_time = {{p::new_uuid(), p::new_uuid(), p::new_uuid(), {}, "human", "fonte", "nota", d.created_at}};
  a.dates = {{p::new_uuid(), p::new_uuid(), "reference", "Data declarada", d.created_at}};
  d.semantics.annotations.push_back(a);
  a.id = p::new_uuid(); a.notes = "Outro comentário"; a.fictional_time.clear(); a.dates.clear();
  d.semantics.annotations.push_back(a);
  d.semantics.relations = {{p::new_uuid(), d.semantics.annotations[0].id,
                            d.semantics.annotations[1].id, "inde.semantic.reference", "human", "Relação explícita", d.created_at}};
  auto leaf = std::make_shared<p::DocumentLeafContent>();
  leaf->text = "ALTERNATIVA PRIVADA🙂\nMais uma linha";
  leaf->formatting = {{p::DocumentTextStyle::Italic, 0, 5, {}}};
  const auto top = p::new_uuid();
  d.resmas = {{p::new_uuid(), "Resma declarada", top, 6, 18,
                {{top, "Corrente", {}}, {p::new_uuid(), "Coberta", leaf}}}};
  d.visibility_profiles = {{p::new_uuid(), "Lente privada", true, {},
                             {{p::VisibilityRuleKind::Paragraph, d.paragraphs.rows[1].id},
                              {p::VisibilityRuleKind::Anchor, d.anchors[0].id}}}};
  d.paragraphs.retired.push_back(p::new_uuid());
  p::validate(d);
  return d;
}
std::unique_ptr<c::DocumentReadProjection> read(const p::Document &d) {
  auto result = c::read_document(owner, d);
  if (!result.projection) {
    for (const auto &e : result.diagnostics) std::cerr << e.code << ':' << e.subject_id << '\n';
    throw std::runtime_error("Valid fixture was refused");
  }
  return std::move(result.projection);
}
void plain_unicode_and_empty() {
  for (const auto &text : {"", "\n", "\n\n", "🙂\né\n漢\n", "igual\nigual\n"}) {
    auto d = document(text); const auto before = inde::persistence::encode_document_snapshot(d);
    auto view = read(d);
    check(o::validate(view->graph()).empty(), "valid object graph for Unicode/empty source");
    check(view->paragraphs().size() == d.paragraphs.rows.size(), "one exact binding per paragraph");
    std::string rendered;
    for (std::size_t i = 0; i < view->paragraphs().size(); ++i) {
      const auto &b = view->paragraphs()[i]; const auto &r = d.paragraphs.rows[i];
      if (i) rendered += '\n';
      const auto value = view->paragraph_text(b.occurrence_id);
      check(value.has_value() && *value == *r.content, "paragraph payload resolves by occurrence identity");
      if (value) rendered += *value;
      check(b.paragraph_id == r.id && b.source_start == r.start && b.source_end == r.end,
            "Unicode offsets and IDs preserved; LF stays in continuous source");
    }
    check(rendered == d.content && !view->paragraph_text("unknown"), "exact continuous rendering, unknown distinct from empty");
    check(inde::persistence::encode_document_snapshot(d) == before, "read has no source mutation");
  }
}
void complete_layers_and_private_content() {
  auto d = rich_document(); const auto before = inde::persistence::encode_document_snapshot(d);
  auto view = read(d);
  check(inde::persistence::encode_document_snapshot(view->source_snapshot()) == before &&
        p::same_document_state(d, view->source_snapshot(), true), "complete document capture including all declared layers");
  check(code(view->notices(), "private_alternatives_retained") &&
        code(view->notices(), "editor_visibility_not_applied") &&
        code(view->notices(), "textual_move_constraints_retained"), "private/full-source boundaries explicit");
  check(view->graph().occurrences.size() == d.paragraphs.rows.size(), "profile does not remove hidden author text");
  const auto &hidden = view->paragraphs()[1];
  check(view->paragraph_text(hidden.occurrence_id) == "Linha oculta", "hidden source remains exact");
  check(view->source_snapshot().resmas[0].leaves[1].covered->text.find("PRIVADA") != std::string::npos,
        "covered leaf retained privately without becoming active paragraph");
  check(view->source_snapshot().semantics.anchors[0].state == p::AnchorResolution::Ambiguous,
        "no automatic semantic repair or inference");
}
void independent_ownership() {
  auto d = rich_document(); auto view = read(d);
  const auto before = inde::persistence::encode_document_snapshot(view->source_snapshot());
  // Keep legitimate mutable handles to simulate a caller changing its original
  // allocations. The capture must not alias even such covered payloads.
  auto mutable_leaf = std::make_shared<p::DocumentLeafContent>(*d.resmas[0].leaves[1].covered);
  d.resmas[0].leaves[1].covered = mutable_leaf;
  auto other_view = read(d);
  mutable_leaf->text = "Mudança posterior no original";
  d.title = "Outro título"; d.content = "Outra fonte"; d.paragraphs = p::identify_paragraphs(d.content);
  check(inde::persistence::encode_document_snapshot(view->source_snapshot()) == before &&
        inde::persistence::encode_document_snapshot(other_view->source_snapshot()) == before,
        "deep frozen snapshot independent from caller metadata/text/covered allocations");
}
void stable_scoped_identity() {
  auto d = document("igual\nigual"); auto first = read(d), second = read(d);
  check(first->graph() == second->graph() && first->paragraphs() == second->paragraphs() &&
        first->source() == second->source(), "repeated capture deterministic");
  check(first->paragraphs()[0].object_id != first->paragraphs()[1].object_id, "equal prose never merges identities");
  auto other_doc = d; other_doc.id = p::new_uuid(); auto other = read(other_doc);
  auto other_project = c::read_document("10000000-0000-4000-8000-000000000002", d);
  check(other->paragraphs()[0].object_id != first->paragraphs()[0].object_id &&
        other_project.projection->paragraphs()[0].object_id != first->paragraphs()[0].object_id,
        "paragraph UUID reused in different document/project stays scoped");
  d.title += " nova"; auto edited = read(d);
  check(edited->graph() == first->graph() && edited->source() != first->source(),
        "identity independent of snapshot revision; metadata still changes freshness");
}
void freshness_all_layers() {
  const auto original = rich_document(); auto view = read(original);
  check(view->check_source(owner, original).current, "exact original still current");
  auto changed = [&](auto edit, const std::string &label) {
    auto d = original; edit(d); const auto result = view->check_source(owner, d);
    check(!result.current && code(result.diagnostics, "source_snapshot_changed"), label);
  };
  changed([](auto &d) { d.title += " alterado"; }, "title-only conflict without timestamp change");
  changed([](auto &d) { d.updated_at += "Z"; }, "timestamp participates in exact stamp");
  changed([](auto &d) { d.visibility_profiles[0].name += " alterado"; }, "profile-only conflict");
  changed([](auto &d) { d.formatting[0].style = p::DocumentTextStyle::Italic; }, "style-only conflict");
  changed([](auto &d) { d.entity_references[0].notes += " alterado"; }, "binding-only conflict");
  changed([](auto &d) { d.semantics.annotations[0].notes += " alterado"; }, "semantic-only conflict");
  changed([](auto &d) { auto leaf = std::make_shared<p::DocumentLeafContent>(*d.resmas[0].leaves[1].covered);
                       leaf->text += " posterior"; d.resmas[0].leaves[1].covered = leaf; }, "covered private text conflict");
  changed([](auto &d) { d.paragraphs.retired.push_back(p::new_uuid()); }, "retired identity conflict");
  auto d = original; d.content = "Divergente";
  check(code(view->check_source(owner, d).diagnostics, "invalid_source_document"), "malformed source refused without blessing it");
  check(code(view->check_source("different", original).diagnostics, "source_project_conflict"), "different project refused");
  d = original; d.id = p::new_uuid();
  check(code(view->check_source(owner, d).diagnostics, "source_document_conflict"), "different document refused");
}
void identity_and_validity_refusals() {
  auto d = document();
  auto refuses = [&](const p::Document &source, const std::string &expected) {
    const auto r = c::read_document(owner, source);
    check(!r.projection && code(r.diagnostics, expected), "refusal has no partial graph: " + expected);
  };
  auto no_ids = d; no_ids.paragraphs = {};
  refuses(no_ids, "paragraph_identity_required");
  auto legacy = d; for (auto &row : legacy.paragraphs.rows) row.content.reset();
  refuses(legacy, "canonical_paragraphs_required");
  auto invalid = d; invalid.paragraphs.rows[0].id = invalid.paragraphs.rows[1].id;
  refuses(invalid, "invalid_source_document");
  invalid = d; invalid.content[0] = static_cast<char>(0xff);
  refuses(invalid, "invalid_source_text");
  invalid = d; invalid.title.push_back('\0');
  refuses(invalid, "invalid_source_text");
  invalid = d; invalid.content += " divergência";
  refuses(invalid, "invalid_source_document");
  const auto missing_owner = c::read_document("", d);
  check(!missing_owner.projection && code(missing_owner.diagnostics, "invalid_source_project_id"), "project identity required");
  const auto before = inde::persistence::encode_document_snapshot(d);
  (void)c::read_document(owner, invalid);
  check(inde::persistence::encode_document_snapshot(d) == before, "failed capture never changes another source");
}
void limits_without_truncation() {
  auto d = document();
  for (const auto &limits : {c::DocumentReadLimits{1, 8 * 1024 * 1024, 16 * 1024 * 1024},
                             c::DocumentReadLimits{10000, 1, 16 * 1024 * 1024},
                             c::DocumentReadLimits{10000, 8 * 1024 * 1024, 1},
                             c::DocumentReadLimits{10001, 8 * 1024 * 1024, 16 * 1024 * 1024}}) {
    auto r = c::read_document(owner, d, limits);
    check(!r.projection && r.diagnostics.size() == 1, "each projection cap fails without truncation");
  }
  auto many = document(std::string(9999, '\n'));
  auto boundary = read(many);
  check(boundary->paragraphs().size() == 10000 && o::validate(boundary->graph()).empty(), "10000 paragraph boundary accepted intact");
  many.content += '\n'; many.paragraphs = p::identify_paragraphs(many.content);
  auto over = c::read_document(owner, many);
  check(!over.projection && code(over.diagnostics, "projection_paragraph_limit"), "10001 paragraphs refused, not silently capped");
}
void reading_constraints_and_visual_only() {
  auto d = document("Antes\nX\nY🙂\nDepois");
  const auto top = p::new_uuid();
  d.resmas = {{p::new_uuid(), "Resma de duas linhas", top, 6, 10,
                {{top, "Corrente", {}}, {p::new_uuid(), "Coberta", std::make_shared<const p::DocumentLeafContent>(p::DocumentLeafContent{"Privado", {}})}}}};
  auto view = read(d);
  check(view->reading_groups().size() == 3 && view->reading_groups()[1].occurrence_ids ==
        std::vector<std::string>{view->paragraphs()[1].occurrence_id, view->paragraphs()[2].occurrence_id},
        "multiline resma uses existing reading-block contract");
  const auto frozen = inde::persistence::encode_document_snapshot(view->source_snapshot());
  o::CompositionEditor editor(view->graph());
  auto r = editor.apply(0, {o::ReorderOccurrences{view->graph().root_layout_id,
                            {view->paragraphs()[2].occurrence_id}, view->paragraphs()[0].occurrence_id}});
  check(r.accepted && r.changed && view->check_source(owner, d).current &&
        inde::persistence::encode_document_snapshot(view->source_snapshot()) == frozen &&
        inde::persistence::encode_document_snapshot(d) == frozen,
        "reordering a graph copy never reorders authorial text or splits source resma");
  check(editor.undo(editor.revision()).accepted && editor.current() == view->graph(), "object undo still independent and exact");
}
void generated_reorders_preserve_binding() {
  auto d = document("igual\n🙂\nigual\né\n\n漢\nFim");
  auto initial = read(d);
  std::mt19937 random(0x0B101D0CU);
  for (int i = 0; i < 250; ++i) {
    std::shuffle(d.paragraphs.rows.begin(), d.paragraphs.rows.end(), random);
    p::position_paragraphs(d.paragraphs); d.content = p::render_paragraphs(d.paragraphs);
    const auto before = inde::persistence::encode_document_snapshot(d); auto view = read(d);
    for (const auto &binding : view->paragraphs()) {
      auto old = std::find_if(initial->paragraphs().begin(), initial->paragraphs().end(),
                              [&](const auto &b) { return b.paragraph_id == binding.paragraph_id; });
      check(old != initial->paragraphs().end() && old->object_id == binding.object_id &&
            old->occurrence_id == binding.occurrence_id && old->text_sha256 == binding.text_sha256,
            "generated reorder preserves ID-based binding without matching prose or offsets");
    }
    check(inde::persistence::encode_document_snapshot(d) == before && view->check_source(owner, d).current,
          "generated captures remain read-only and fresh");
  }
}
} // namespace
int main() {
  try {
    plain_unicode_and_empty(); complete_layers_and_private_content(); independent_ownership();
    stable_scoped_identity(); freshness_all_layers(); identity_and_validity_refusals();
    limits_without_truncation(); reading_constraints_and_visual_only(); generated_reorders_preserve_binding();
  } catch (const std::exception &e) { std::cerr << "ERROR: " << e.what() << '\n'; return 1; }
  if (failures) { std::cerr << failures << " failures\n"; return 1; }
  std::cout << "INDE-OBJ-01: 9 document read-adapter groups passed (250 generated source orders, 10000 paragraph boundary)\n";
}
