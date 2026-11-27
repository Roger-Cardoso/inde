#include "inde/application/document_edit_session.hpp"
#include "inde/application/project_service.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
#include "object_composite_pilot.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
namespace fs = std::filesystem;
using V = inde::persistence::json::Value;
using Session = inde::objects::pilot::Session;
void check(bool yes, const char *message) {
  if (!yes)
    throw std::runtime_error(message);
}
V view(Session &s) { return s.request(V::Object{{"op", "view"}}).at("state"); }
V op(Session &s, const std::string &name, V::Object args = {}) {
  args["op"] = name;
  args["expected"] = view(s).at("revision");
  auto r = s.request(args);
  if (!r.at("accepted").as_boolean())
    throw std::runtime_error(r.at("error").as_string());
  return r.at("state");
}
int main() {
  try {
    char pattern[] = "/tmp/inde-native-objects-XXXXXX";
    const auto *name = ::mkdtemp(pattern);
    check(name, "temporary directory");
    const fs::path root = name;
    ::setenv("XDG_CONFIG_HOME", (root / "config").c_str(), 1);
    inde::application::ProjectService service;
    service.create(root / "Autor.inde", "Autor");
    auto d = service.writing().create_document("Matemática e história");
    d.content = "Uma viagem.\nA equação acompanha a escrita 🙂.";
    d.paragraphs = inde::project::identify_paragraphs(d.content);
    d.formatting = {{inde::project::DocumentTextStyle::Bold, 0, 3, {}}};
    d = service.writing().update_document(d);
    const auto original = inde::persistence::encode_document_snapshot(d);
    const auto owner = service.current()->manifest().project_id;
    const auto parent = service.current()->path() / "documents/composed";
    fs::create_directory(parent);
    fs::permissions(parent, fs::perms::owner_all);
    const auto folder = parent / d.id;
    std::string id;
    {
      Session s(folder, owner, d);
      auto state = view(s);
      check(state.at("native_document").as_boolean(), "native flag");
      check(state.at("source_document").as_string() == d.id, "wrong document");
      check(state.at("document_title").as_string() == d.title, "wrong title");
      state = op(s, "create_equation",
                 {{"before", ""},
                  {"source", "\\frac{a}{b}"},
                  {"mode", "block"},
                  {"label", "Fração"}});
      id = state.at("selection").as_string();
      op(s, "save");
      const auto confirmed=view(s);
      const auto database=folder/"obj01-piloto.sqlite3";
      const auto read_bytes=[](const fs::path &path) {
        std::ifstream input(path,std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input),{});
      };
      const auto bytes=read_bytes(database);
      const auto again=op(s,"save");
      check(again.at("saved_revision").as_integer()==confirmed.at("saved_revision").as_integer() &&
            again.at("recovery_generation").as_integer()==confirmed.at("recovery_generation").as_integer() &&
            read_bytes(database)==bytes,"No-op confirmation rewrote composition receipts");

      op(s, "set_context", {{"context", "text"}});
      op(s, "edit_equation",
         {{"id", id},
          {"source", "x^2+y^2"},
          {"mode", "inline"},
          {"label", "Distância"}});
    }
    check(inde::persistence::encode_document_snapshot(
              *service.writing().document(d.id)) == original,
          "owner source changed");
    {
      Session s(folder, owner, d);
      auto state = view(s);
      check(state.at("dirty").as_boolean(), "unsaved state not recovered");
      check(state.at("equations").as_array()[0].at("source").as_string() ==
                "x^2+y^2",
            "source lost");
      op(s, "set_context", {{"context", "structure"}});
      op(s, "remove", {{"id", id}});
      op(s, "save");
      op(s, "restore_removed", {{"id", id}, {"before", ""}});
      op(s, "save");
    }
    service.save_as(root / "Copia.inde");
    {
      Session s(root / "Copia.inde/documents/composed" / d.id,
                service.current()->manifest().project_id,
                *service.writing().document(d.id));
      check(view(s).at("equations").as_array().size() == 1,
            "save as lost objects");
    }
    auto changed = *service.writing().document(d.id);
    changed.content += " Novo texto.";
    changed.paragraphs = inde::project::identify_paragraphs(changed.content);
    changed = service.writing().update_document(changed);
    bool refused = false;
    try {
      Session s(root / "Copia.inde/documents/composed" / d.id,
                service.current()->manifest().project_id, changed);
    } catch (const std::exception &e) {
      refused = std::string(e.what()) == "native_document_source_changed";
    }
    check(refused, "source drift accepted");
    // Live Writing binding adopts complete canonical edits while the original
    // capture remains immutable; object undo cannot resurrect stale body text.
    {
      const auto livefolder = root / "Live.inde";
      fs::create_directory(livefolder);
      const auto live = livefolder / "composition";
      const auto edited =
          inde::application::replace_selection({d, {}}, 4, 4, " nova").document;
      {
        Session s(live, owner, d, inde::objects::pilot::DocumentBinding::Live);
        auto st = op(s, "create_equation",
                     {{"before", ""},
                      {"source", "x^2"},
                      {"mode", "block"},
                      {"label", "Mantida"}});
        const auto eid = st.at("selection").as_string();
        op(s, "set_context", {{"context", "composition"}});
        op(s, "set_presentation",
           {{"id", eid},
            {"width", 60},
            {"horizontal", "center"},
            {"text_align", "left"},
            {"vertical", "start"}});
        s.synchronize_document(edited);
        check(inde::project::same_document_state(s.current_document(), edited),
              "live full source mismatch");
        check(s.current_document().paragraphs.rows[0].id ==
                  d.paragraphs.rows[0].id,
              "live paragraph identity lost");
        check(s.current_document().formatting.size() == d.formatting.size(),
              "live formatting lost");
        check(view(s).at("equations").as_array().size() == 1,
              "live equation lost");
        op(s, "undo");
        check(s.current_document().content == edited.content,
              "object undo resurrected old body");
        op(s, "redo");
        check(s.current_document().content == edited.content,
              "object redo resurrected old body");
        op(s, "set_context", {{"context", "structure"}});
        auto paragraphs = view(s).at("paragraphs").as_array();
        op(s, "move",
           {{"id", paragraphs[1].at("id")},
            {"before", paragraphs[0].at("id")},
            {"intent", "authorial"}});
        auto committed = s.current_document();
        committed.updated_at = "2026-11-23T15:00:01Z";
        s.synchronize_document(committed);
        op(s, "undo");
        check(s.current_document().content == edited.content,
              "owner commit erased move undo");
        auto split = inde::application::replace_selection({edited, {}}, 0, 0,
                                                          "Nova linha.\n")
                         .document;
        s.synchronize_document(split);
        check(view(s).at("paragraphs").as_array().size() == 3,
              "split paragraph not projected");
        s.synchronize_document(edited);
        check(view(s).at("paragraphs").as_array().size() == 2,
              "text undo not projected");
        check(std::get<double>(
                  view(s).at("presentation").at(eid).at("width").storage()) ==
                  60,
              "live placement lost");
        auto other = edited;
        other.id = "another";
        bool rejected = false;
        try {
          s.synchronize_document(other);
        } catch (const std::exception &) {
          rejected = true;
        }
        check(rejected, "foreign document accepted");
        op(s, "save");
      }
      {
        Session s(live, owner, edited,
                  inde::objects::pilot::DocumentBinding::Live);
        check(view(s).at("equations").as_array().size() == 1,
              "live reopen lost equation");
        check(s.current_document().content == edited.content,
              "live reopen lost body");
      }
      std::ifstream original_file(live / "origem-documento.json");
      const std::string captured(
          (std::istreambuf_iterator<char>(original_file)), {});
      check(captured == original, "live changed immutable provenance");
    }
    // Many object-history entries may share the same owner snapshot. Adopting
    // an edit must preserve every style step without resurrecting stale text.
    {
      const auto history = root / "history-adoption";
      auto after = inde::application::replace_selection(
          {d, {}}, d.content.find('\n'), d.content.find('\n') + 1, "").document;
      {
        Session s(history, owner, d, inde::objects::pilot::DocumentBinding::Live);
        op(s, "set_context", {{"context", "composition"}});
        const auto paragraph = view(s).at("paragraphs").as_array()[0].at("id").as_string();
        for(int i=0;i<32;++i)
          op(s,"set_presentation",{{"id",paragraph},{"width",20+i},
             {"horizontal","start"},{"text_align","left"},{"vertical","start"}});
        s.synchronize_document(after);
        for(int i=0;i<32;++i) {
          auto state=op(s,"undo");
          check(s.current_document().content==after.content,"History adoption undo resurrected old text");
          if(i<31)check(std::get<double>(state.at("presentation").at(paragraph).at("width").storage())==50-i,
                         "History adoption lost a style step");
          else check(!state.at("presentation").as_object().contains(paragraph),"History adoption lost initial presentation");
        }
        for(int i=0;i<32;++i) {
          auto state=op(s,"redo");
          check(s.current_document().content==after.content,"History adoption redo resurrected old text");
          check(std::get<double>(state.at("presentation").at(paragraph).at("width").storage())==20+i,
                "History adoption redo lost a style step");
        }
        op(s,"save");
      }
      Session reopened(history,owner,after,inde::objects::pilot::DocumentBinding::Live);
      check(reopened.current_document().content==after.content,"History adoption reopen lost accepted text");
    }
    {
      const auto path=root/"clean-confirmation-conflict";
      Session stale(path,owner,d,inde::objects::pilot::DocumentBinding::Live);
      // A second cooperating session is excluded by the workspace lease.
      // Inject coherent external revision drift directly to exercise the
      // confirmation check even when the local snapshot is clean.
      {
        inde::persistence::SqliteDatabase external(path/"obj01-piloto.sqlite3");
        external.execute("BEGIN IMMEDIATE");
        external.execute("UPDATE saved SET revision=revision+1 WHERE id=1");
        external.execute("UPDATE working SET base_revision=base_revision+1 WHERE id=1");
        external.execute("COMMIT");
      }
      const auto result=stale.request(V::Object{{"op","save"},{"expected",view(stale).at("revision")}});
      check(!result.at("accepted").as_boolean() && result.at("error").as_string()=="persisted_revision_conflict",
            "Clean confirmation bypassed an external revision conflict");
    }
    bool pilot_refused = false;
    try {
      Session s(folder, inde::objects::pilot::Profile::P2D2);
    } catch (const std::exception &) {
      pilot_refused = true;
    }
    check(pilot_refused, "pilot accepts real project path");
    inde::persistence::SqliteDatabase db(root /
                                         "Copia.inde/data/project.sqlite3");
    check(db.query_integer("SELECT MAX(version) FROM schema_migrations") == 37,
          "canonical schema changed");
    check(db.query_text("PRAGMA integrity_check") == "ok",
          "canonical integrity");
    service.close();
    // The test owns this newly created temporary tree only.
    fs::remove_all(root);
    std::cout << "Native document binding, source preservation, recovery, "
                 "save-as, stale refusal and schema37: OK\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
