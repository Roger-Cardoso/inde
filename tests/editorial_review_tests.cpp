#include "editorial_migration_fixture.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/application/editorial_review.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/semantic_codec.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <cassert>
#include <glib.h>
#include <iostream>
#include <source_location>
using namespace inde;
using namespace inde::application;
template <class F>
void rejects(F f, const std::source_location location =
                      std::source_location::current()) {
  bool failed{};
  try {
    f();
  } catch (const std::exception &) {
    failed = true;
  }
  if (!failed)
    throw std::runtime_error("Expected refusal at line " +
                             std::to_string(location.line()));
}
void comparison(const std::string &a, const std::string &b,
                bool coarse = false) {
  project::Document left, right;
  left.content = a;
  right.content = b;
  const auto d = compare_documents(left, right);
  std::string observed, proposed;
  std::size_t li = 1, ri = 1;
  for (const auto &v : d.lines) {
    if (v.kind != DifferenceKind::Added) {
      observed += v.text;
      assert(v.left_line == li++);
    }
    if (v.kind != DifferenceKind::Removed) {
      proposed += v.text;
      assert(v.right_line == ri++);
    }
  }
  assert(observed == a && proposed == b && d.coarse == coarse);
}
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("inde-editorial-review-tests-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  const auto path = root / "Editorial.inde";
  editorial_migration_test(root);
  application::ProjectService service;
  service.create(path, "Revisão editorial — QA");
  auto d = service.writing().create_document("Água 🙂 — original");
  d.content = "Água 🙂 retorna.\nA ponte espera.\r\nFim";
  d.formatting = {{project::DocumentTextStyle::Bold, 0, 4, {}}};
  d.perspective = "Mara";
  d.word_goal = 500;
  d = service.writing().update_document(d);
  const auto original = d;
  const auto types = service.writing().semantic_types();
  for (const auto &t : types)
    assert(persistence::decode_semantic_type(
               persistence::encode_semantic_type(t)) == t);
  const auto *comment =
      project::find_annotation_type(types, "inde.editor.comment", 1);
  assert(comment);
  using K = project::SemanticFieldKind;
  d = TextSemanticService::create(
      d, *comment, 0, 4,
      {{"note", {K::Text, {"Rever o ritmo"}}}, {"state", {K::Enum, {"open"}}}});
  const auto comment_id = d.semantics.annotations.back().id;
  assert(review_counts(d).open_comments == 1);
  rejects(
      [&] { (void)set_editorial_state(d, "approved", "Editora", "Não pode"); });
  d = decide_comment(d, comment_id, true);
  assert(review_counts(d).open_comments == 0 &&
         review_counts(d).resolved_comments == 1);
  const auto resolved = d;
  assert(decide_comment(resolved, comment_id, false).content == d.content);
  d = propose_change(d, 0, 4, "Memória", "Ritmo mais preciso");
  const auto suggestion = d.semantics.annotations.back().id;
  assert(d.content == original.content &&
         review_counts(d).pending_suggestions == 1);
  rejects([&] { (void)TextSemanticService::remove(d, suggestion); });
  rejects([&] { (void)TextSemanticService::archive(d, suggestion, true); });
  rejects([&] { (void)TextSemanticService::update(d, suggestion, {}, types); });
  auto incomplete = d;
  incomplete.semantics.annotations.back().fields.erase("replacement");
  rejects([&] { (void)decide_suggestion(incomplete, suggestion, true); });
  rejects(
      [&] { (void)set_editorial_state(d, "approved", "Editora", "Pendente"); });
  rejects([&] { (void)propose_change(d, 0, 4, "Água", ""); });
  rejects([&] { (void)propose_change(d, 1, 1, "", ""); });
  rejects([&] { (void)propose_change(d, 0, 999999, "", ""); });
  rejects([&] { (void)propose_change(d, 0, 4, "\xff", ""); });
  rejects([&] { (void)propose_change(d, 0, 4, std::string("x\0y", 3), ""); });
  auto rejected = decide_suggestion(d, suggestion, false);
  assert(rejected.content == d.content &&
         review_counts(rejected).pending_suggestions == 0);
  rejects([&] { (void)decide_suggestion(rejected, suggestion, true); });
  auto changed = replace_selection({d, {4, 0}}, 0, 4, "Outro").document;
  rejects([&] { (void)decide_suggestion(changed, suggestion, true); });
  assert(d.content == original.content);
  auto missing = d;
  missing.anchors.clear();
  rejects([&] { (void)decide_suggestion(missing, suggestion, true); });
  auto accepted = decide_suggestion(d, suggestion, true);
  assert(accepted.content == "Memória 🙂 retorna.\nA ponte espera.\r\nFim");
  assert(review_field(accepted.semantics.annotations.back(), "state") ==
         "accepted");
  auto deleted = propose_change(original, 5, 6, "", "Remover emoji");
  deleted =
      decide_suggestion(deleted, deleted.semantics.annotations.back().id, true);
  assert(deleted.content == "Água  retorna.\nA ponte espera.\r\nFim");
  rejects([&] { (void)set_editorial_state(accepted, "approved", " \t", ""); });
  rejects([&] { (void)set_editorial_state(accepted, "alien", "Editora", ""); });
  d = set_editorial_state(accepted, "in_review", "Editora", "Primeiro passe");
  assert(!editorial_state(d).outdated);
  d = set_editorial_state(d, "approved", "Editora", "Aprovado neste estado");
  assert(editorial_state(d).state == "approved" &&
         !editorial_state(d).outdated);
  auto clock = d;
  clock.updated_at = "Outra data";
  clock.created_at = "Outra data";
  assert(!editorial_state(clock).outdated);
  auto title = d;
  title.title += " — mudado";
  assert(editorial_state(title).outdated);
  auto format = d;
  format.formatting.clear();
  assert(editorial_state(format).outdated);
  auto body = d;
  body.content += "!";
  assert(editorial_state(body).outdated);
  auto reopened = decide_comment(d, comment_id, false);
  assert(editorial_state(reopened).outdated);
  auto duplicate = d;
  auto event = duplicate.semantics
                   .annotations[duplicate.semantics.annotations.size() - 2];
  event.id = project::new_uuid();
  duplicate.semantics.annotations.push_back(event);
  rejects([&] { (void)editorial_state(duplicate); });
  auto archived = d;
  archived.semantics.annotations.back().status = "archived";
  rejects([&] { (void)editorial_state(archived); });
  d = service.writing().update_document_if_unchanged(d, original);
  const auto durable = d;
  auto external = d;
  external.title = "Outra edição concorrente";
  external = service.writing().update_document_if_unchanged(external, d);
  rejects([&] {
    (void)service.writing().update_document_if_unchanged(
        set_editorial_state(d, "draft", "Autor", ""), d);
  });
  assert(project::same_document_state(external,
                                      *service.writing().document(d.id)));
  rejects([&] {
    (void)service.writing().create_editorial_revision(d, "Fonte mudou");
  });
  d = service.writing().update_document_if_unchanged(d, external);
  rejects([&] { (void)service.writing().create_editorial_revision(d, "  "); });
  const auto revision =
      service.writing().create_editorial_revision(d, "Passe da editora");
  assert(revision.id != d.id && revision.revision_of_id == d.id &&
         revision.purpose == project::DocumentPurpose::Revision);
  assert(revision.content == d.content &&
         std::equal(revision.formatting.begin(), revision.formatting.end(),
                    d.formatting.begin(), d.formatting.end(),
                    [](const auto &a, const auto &b) {
                      return a.style == b.style &&
                             a.start_offset == b.start_offset &&
                             a.end_offset == b.end_offset && a.value == b.value;
                    }) &&
         revision.word_goal == d.word_goal &&
         revision.perspective == d.perspective);
  assert(revision.anchors.empty() && revision.entity_references.empty() &&
         revision.semantics.annotations.empty());
  assert(project::same_document_state(d, *service.writing().document(d.id)));
  assert(service.writing().document(d.id));
  DocumentEditSession session;
  session.open({d, {7, 0}});
  assert(session.commit({accepted, {7, 0}}, "Decisão editorial"));
  assert(session.undo()->document.semantics.annotations.size() ==
         d.semantics.annotations.size());
  assert(session.redo()->document.content == accepted.content);
  service.close();
  service.open(path);
  const auto reread = service.writing().document(d.id);
  assert(reread && project::same_document_state(*reread, durable, false));
  assert(editorial_state(*reread).state == "approved" &&
         !editorial_state(*reread).outdated);
  assert(service.writing().document(revision.id));
  service.close();
  {
    persistence::SqliteDatabase db(path / "data/project.sqlite3");
    assert(db.query_text("PRAGMA integrity_check") == "ok");
    assert(!db.prepare("PRAGMA foreign_key_check").step());
  }
  for (const auto &a : std::vector<std::string>{"", "Água 🙂", "a\n",
                                                "a\r\n\nFim", "a\nb\nc\n"})
    for (const auto &b : std::vector<std::string>{"", "Água 🙂!", "a\n",
                                                  "a\r\nFim", "b\na\nc\n"})
      comparison(a, b);
  comparison(std::string(21000, '\n'), "fim\n", true);
  project::Document invalid;
  invalid.content = "\xff";
  rejects([&] { (void)compare_documents(invalid, {}); });
  invalid.content = std::string("a\0b", 3);
  rejects([&] { (void)compare_documents(invalid, {}); });
  std::stop_source cancellation;
  cancellation.request_stop();
  rejects([&] { (void)compare_documents({}, {}, cancellation.get_token()); });
  auto layers = original;
  layers.formatting.clear();
  assert(compare_documents(original, layers).layers_differ);
  std::cout << "PASS editorial review: explicit decisions, Unicode "
               "suggestion/deletion, stale source/CAS, fingerprint/history, "
               "independent revision, reopen, integrity, readonly exact "
               "diff/cancellation\n";
  // Isolated disposable fixture retained for diagnosis; no user project is
  // opened.
}
