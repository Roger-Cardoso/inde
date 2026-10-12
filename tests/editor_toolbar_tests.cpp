#include "../src/persistence/recovery_codec.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/application/inline_commands.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/text_clipboard.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/inline_command_codec.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/project/document_state.hpp"
#include "schema17_downgrade.hpp"
#include <cassert>
#include <iostream>
using namespace inde;
template <class F> void rejects(F f) {
  bool failed{};
  try {
    f();
  } catch (const std::exception &) {
    failed = true;
  }
  assert(failed);
}
int main() {
  using S = project::DocumentTextStyle;
  using K = project::SemanticFieldKind;
  const auto root = std::filesystem::temp_directory_path() /
                    ("inde-toolbar-" + project::new_uuid());
  std::filesystem::create_directories(root);
  setenv("XDG_CONFIG_HOME", (root / "config").c_str(), 1);
  {
    application::ProjectService service;
    service.create(root / "project.inde", "QA barra");
    auto d = service.writing().create_document("Título Unicode 🙂");
    d.content = "Água 🙂 no título.\nOutro parágrafo.";
    d.formatting = {{S::Bold, 0, 6},
                    {S::Heading, 0, 16},
                    {S::FontFamily, 0, 6, "Serif"},
                    {S::FontSize, 0, 6, "18"},
                    {S::Foreground, 0, 6, "#ab34ef"},
                    {S::AlignCenter, 0, 16},
                    {S::Heading6, 17, 33}};
    d.inline_commands = {
        {"@", "nota", "Nota autoral", "Nota: água 🙂\nMais texto"},
        {"@", "nota", "Outra nota", "Outro"},
        {"@@", "", "Prefixo longo", "Literal"}};
    auto comment = *project::find_annotation_type(
        service.writing().semantic_types(), "inde.editor.comment", 1);
    d = application::TextSemanticService::create(
        d, comment, 0, 6,
        {{"note", {K::Text, {"Rever esta água 🙂"}}},
         {"state", {K::Enum, {"open"}}}});
    d = service.writing().update_document(d);
    auto saved = *service.writing().document(d.id);
    assert(project::same_document_state(d, saved));
    auto bad = d;
    bad.formatting[3].value = "97";
    rejects([&] { static_cast<void>(service.writing().update_document(bad)); });
    for (auto value :
         {"0", "5", "97", "10.5", "10px", "-1", "10;DROP TABLE documents"})
      rejects(
          [&] { project::validate_format_span({S::FontSize, 0, 1, value}); });
    for (auto value : {"red", "#123", "#12345678", "#zzzzzz"})
      rejects(
          [&] { project::validate_format_span({S::Foreground, 0, 1, value}); });
    rejects([&] { project::validate_format_span({S::Bold, 0, 1, "x"}); });
    auto color_changed = d;
    color_changed.formatting[4].value = "#abcdef";
    assert(!project::same_document_state(d, color_changed));
    application::DocumentEditSession history;
    history.open({d, {0, 6}});
    assert(history.commit({color_changed, {0, 6}}, "Cor"));
    assert(project::same_document_state(history.undo()->document, d, false));
    assert(project::same_document_state(history.redo()->document, color_changed,
                                        false));
    auto fragment = application::copy_fragment(d, 0, 6, true);
    auto round =
        application::decode_fragment(application::encode_fragment(fragment));
    assert(round.formatting.size() == fragment.formatting.size());
    for (std::size_t i = 0; i < round.formatting.size(); ++i)
      assert(round.formatting[i].value == fragment.formatting[i].value);
    auto pasted = application::paste_fragment({d, {0, 0}}, 17, 17, round, true);
    assert(pasted.document.inline_commands == d.inline_commands);
    assert(pasted.document.semantics.annotations.front().id ==
           d.semantics.annotations.front().id);
    assert(std::any_of(
        pasted.document.formatting.begin(), pasted.document.formatting.end(),
        [](auto &s) { return s.start_offset == 17 && s.value == "#ab34ef"; }));
    assert(persistence::decode_inline_commands(
               persistence::encode_inline_commands(d.inline_commands)) ==
           d.inline_commands);
    auto q = application::inline_query("🙂 @no", 5);
    assert(q && q->start == 2 && q->token == "@no");
    assert(application::inline_candidates(d.inline_commands, *q).size() == 2);
    q = application::inline_query("@@", 2);
    assert(application::inline_candidates(d.inline_commands, *q) ==
           std::vector<std::size_t>{2});
    assert(!application::inline_query(std::string(129, '@'), 129));
    assert(!application::inline_query(" ", 1));
    auto replaced = application::replace_selection(
        {d, {6, 6}}, 0, 6, d.inline_commands.front().content);
    assert(replaced.document.content.starts_with("Nota: água 🙂"));
    assert(replaced.document.semantics.anchors.front().state ==
           project::AnchorResolution::Ambiguous);
    project::RecoveryRecord checkpoint;
    checkpoint.project_id = service.current()->manifest().project_id;
    checkpoint.session_id = project::new_uuid();
    checkpoint.revision = 1;
    checkpoint.state = {d, 0, 6};
    checkpoint.baseline = d;
    checkpoint.updated_at = project::utc_now();
    auto encoded = persistence::recovery::encode(checkpoint);
    auto decoded = persistence::recovery::decode(
        encoded, persistence::recovery::checksum(encoded));
    assert(project::same_document_state(decoded.state.document, d));
    auto legacy = d;
    legacy.inline_commands.clear();
    legacy.semantics = {};
    legacy.formatting = {{S::Bold, 0, 6}};
    checkpoint.state = {legacy, 0, 6};
    checkpoint.baseline = legacy;
    for (int version : {1, 2}) {
      auto obj =
          persistence::json::parse(persistence::recovery::encode(checkpoint))
              .as_object();
      obj["version"] = version;
      for (auto name : {"state", "baseline"}) {
        auto doc = obj[name].as_object();
        doc.erase("inline_commands");
        if (version == 1)
          doc.erase("semantics");
        auto formats = doc["formatting"].as_array();
        for (auto &format : formats) {
          auto s = format.as_object();
          s.erase("value");
          format = s;
        }
        doc["formatting"] = formats;
        obj[name] = doc;
      }
      auto payload = persistence::json::serialize(obj, false);
      auto old = persistence::recovery::decode(
          payload, persistence::recovery::checksum(payload));
      assert(project::same_document_state(old.state.document, legacy));
    }
    const auto original_id = service.current()->manifest().project_id;
    service.save_as(root / "copy.inde");
    assert(service.current()->manifest().project_id != original_id);
    assert(project::same_document_state(*service.writing().document(d.id), d));
    service.close();
    service.open(root / "copy.inde");
    assert(project::same_document_state(*service.writing().document(d.id), d));
    service.close();
  }
  {
    persistence::SqliteDatabase db(root / "migration.sqlite3");
    persistence::SchemaMigrator migration;
    migration.migrate(db);
    downgrade_schema17_for_fixture(db);
    assert(migration.current_version(db) == 16);
    db.execute("ALTER TABLE documents ADD COLUMN inline_commands TEXT");
    rejects([&] { migration.migrate(db); });
    assert(migration.current_version(db) == 16);
    assert(
        db.query_integer(
            "SELECT count(*) FROM pragma_table_info('document_format_spans') "
            "WHERE name='value'") == 0);
    db.execute("ALTER TABLE documents DROP COLUMN inline_commands");
    migration.migrate(db);
    assert(migration.current_version(db) == persistence::current_database_schema_version);
    assert(db.query_text("PRAGMA integrity_check") == "ok");
    assert(!db.prepare("PRAGMA foreign_key_check").step());
  }
  std::filesystem::remove_all(root);
  std::cout << "PASS: editor format, inline, comments, clipboard, checkpoint "
               "v1/v2/v3, save-as, CAS state and v17 rollback\n";
}
