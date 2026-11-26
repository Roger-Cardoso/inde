#include "inde/application/contextual_planning.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/document_state.hpp"
#include "object_composite_pilot.hpp"
#include "project_file.hpp"
#include "project_file_transport.hpp"
#include <fstream>
#include <glib.h>
#include <iostream>
#include <locale.h>
using namespace inde;
using namespace project_file;
using V = persistence::json::Value;
using Session = objects::pilot::Session;
namespace fs = std::filesystem;
void check(bool yes, const char *what) {
  if (!yes)
    throw std::runtime_error(what);
}
V view(Session &s) { return s.request(V::Object{{"op", "view"}}).at("state"); }
V op(Session &s, const std::string &name, V::Object args = {}) {
  args["op"] = name;
  args["expected"] = view(s).at("revision");
  auto result = s.request(args);
  if (!result.at("accepted").as_boolean())
    throw std::runtime_error(name + ": " + result.at("error").as_string());
  return result.at("state");
}
std::map<std::string, std::string> inventory(const fs::path &root) {
  std::map<std::string, std::string> out;
  for (const auto &e : fs::recursive_directory_iterator(root))
    if (e.is_regular_file()) {
      std::ifstream f(e.path(), std::ios::binary);
      const std::string data{std::istreambuf_iterator<char>(f), {}};
      out[e.path().lexically_relative(root).generic_string()] =
          sha256(Bytes(data.begin(), data.end()));
    }
  return out;
}
int main(int argc, char **argv) {
  auto *uuid = g_uuid_string_random();
  const auto root = argc == 2 ? fs::absolute(argv[1])
                              : fs::temp_directory_path() /
                                    ("inde-p5-native-" + std::string(uuid));
  g_free(uuid);
  const bool keep = argc == 2;
  try {
    check(!fs::exists(root), "Synthetic root must be new");
    fs::create_directories(root);
    const auto numeric=newlocale(LC_NUMERIC_MASK,"pt_BR.UTF-8",nullptr);
    if(numeric) {
      const auto previous=uselocale(numeric);
      bool correct=false;
      try {
        const auto decoded=persistence::json::parse("{\"x\":0.6120081954998241}");
        correct=std::get<double>(decoded.at("x").storage())==0.6120081954998241;
      } catch(...) {uselocale(previous);freelocale(numeric);throw;}
      uselocale(previous);freelocale(numeric);
      check(correct,"Native owner JSON lost a decimal under pt_BR");
    }

    g_setenv("XDG_CONFIG_HOME", (root / "perfil").c_str(), true);
    application::ProjectService service;
    const auto source = root / "Original-sintetico.inde";
    service.create(source, "Demonstração P5 — projeto e estados");
    auto doc = service.writing().create_document("Uma obra transportável");
    doc.content = "A porta se abriu.\nO personagem atravessou.";
    doc.paragraphs = project::identify_paragraphs(doc.content);
    doc.formatting = {{project::DocumentTextStyle::Bold, 0, 7, {}}};
    doc.anchors = {{project::new_uuid(), "Abertura", 0, 7}};
    doc.perspective = "Narrador observador — campo autoral";
    doc.inline_commands = {{"@", "nota", "Anotação", "Literal preservado"}};
    const auto top = project::new_uuid();
    auto covered = std::make_shared<project::DocumentLeafContent>();
    covered->text = "Outra abertura — alternativa privada";
    covered->formatting = {{project::DocumentTextStyle::Italic, 0, 5, {}}};
    doc.resmas = {{project::new_uuid(), "Aberturas", top, 0, 7,
                   {{top, "Atual", {}},
                    {project::new_uuid(), "Alternativa", covered}}}};
    doc.visibility_profiles = {
        {project::new_uuid(), "Lente de trabalho", true, {},
         {{project::VisibilityRuleKind::Paragraph, doc.paragraphs.rows[1].id}}}};
    doc.semantics.anchors.push_back(
        application::SemanticAnchorEngine::evidence(doc, doc.anchors[0]));
    project::TextAnnotation annotation;
    annotation.id = project::new_uuid();
    annotation.anchor_id = doc.anchors[0].id;
    annotation.type_key = "inde.editor.comment";
    annotation.created_at = annotation.updated_at = doc.created_at;
    annotation.fields["note"] = {project::SemanticFieldKind::Text,
                                  {"Sentido declarado pelo autor"}};
    annotation.fields["state"] = {project::SemanticFieldKind::Enum, {"open"}};
    doc.semantics.annotations.push_back(annotation);
    doc = service.writing().update_document(doc);
    const auto original_doc = persistence::encode_document_snapshot(doc);
    auto montage = service.compositions().create("Composição textual preservada");
    const auto live = service.compositions().import(
        montage.value, doc, 0, project::utf8_character_count(doc.content));
    const auto block = montage.value.blocks.occurrence(live).block_id;
    const auto revision = montage.value.blocks.block(block).current_revision;
    const auto pinned = montage.value.blocks.place(block);
    montage.value.blocks.pin(pinned, revision);
    const auto reference = montage.value.blocks.place(block);
    montage.value.blocks.reference_only(reference);
    montage = service.compositions().save(montage);
    montage.value.drafts[block] = {revision, "Rascunho privado da composição"};
    montage = service.compositions().save(montage, true);
    const auto montage_id = montage.value.blocks.composition_id();
    const auto original_montage = persistence::encode_composition(montage.value);
    const auto owner = service.current()->manifest().project_id;
    application::ContextualPlanningService planning(service);
    const auto ip =
        service.create_intellectual_property("Universo fictício", "", "").id;
    const auto work = service
                          .create_work(ip, "Obra de demonstração", "", "",
                                       "pt-BR", "Planejamento")
                          .id;
    const auto plan = planning.create_plan(work, "Travessia");
    const auto part = planning.create_moment(
        planning.snapshot(plan), "Atravessar a porta", "Ligação conservada");
    planning.associate_text(planning.snapshot(plan), part,
                            planning.text_document(doc.id));
    const auto original_plan = planning.snapshot(plan).revision;
    const auto composition = source / "documents/composed" / doc.id;
    fs::create_directories(composition.parent_path());
    V objects;
    {
      Session s(composition, owner, doc, objects::pilot::DocumentBinding::Live);
      op(s, "set_context", {{"context", "structure"}});
      const auto a = op(s, "create_table_sized",
                        {{"before", ""},
                         {"title", "Tabela"},
                         {"header", true},
                         {"rows", 2},
                         {"columns", 2}})
                         .at("selection")
                         .as_string();
      const auto b = op(s, "create_equation",
                        {{"before", ""},
                         {"source", "x^2+y^2"},
                         {"mode", "inline"},
                         {"label", "Distância"}})
                         .at("selection")
                         .as_string();
      op(s, "place_beside", {{"id", b}, {"other", a}, {"side", "left"}});
      op(s, "import",
         {{"before", ""},
          {"caption", "Recurso incorporado"},
          {"alt", ""},
          {"decorative", true},
          {"data", "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAEUlEQVR4nGN"
                   "waDjwH4QZYAwAVPQJ/Q4itk8AAAAASUVORK5CYII="}});
      objects = op(s, "save");
    }
    service.save();
    service.close();
    fs::create_directories(source / "vazia");
    const auto source_bytes = inventory(source);
    const auto packet = root / "Projeto-portatil.inde";
    const auto first = pack_directory(source, packet, g_getenv("P5_TRANSPORT_BLOCKS") ?
        TransportProfile::Blocks : TransportProfile::WholeFile);
    {
      auto store = Store::open(packet);
      unpack_directory(store, first.sequence, root / "Reaberta.inde");
    }
    check(inventory(root / "Reaberta.inde") == source_bytes,
          "Byte roundtrip mismatch");
    service.open(root / "Reaberta.inde");
    check(service.current()->manifest().project_id == owner,
          "Project identity changed");
    check(persistence::encode_document_snapshot(
              *service.writing().document(doc.id)) == original_doc,
          "Document layers changed");
    check(planning.snapshot(plan).revision == original_plan,
          "Planning/reference changed");
    check(persistence::encode_composition(
              service.compositions().load(montage_id).value) == original_montage,
          "Textual composition modes or private draft changed");
    {
      Session s(root / "Reaberta.inde/documents/composed" / doc.id, owner,
                *service.writing().document(doc.id),
                objects::pilot::DocumentBinding::Live);
      auto state = view(s);
      for (const auto *key : {"tables", "equations", "figures", "rows",
                              "author_order", "visual_order"})
        check(project_file::encode_json(state.at(key)) ==
                  project_file::encode_json(objects.at(key)),
              "Composed object changed");
      auto changed =
          application::replace_selection(
              {*service.writing().document(doc.id), {}},
              static_cast<std::size_t>(g_utf8_strlen(doc.content.c_str(), -1)),
              static_cast<std::size_t>(g_utf8_strlen(doc.content.c_str(), -1)),
              " Continuação preservada.")
              .document;
      changed = service.writing().update_document(changed);
      s.synchronize_document(changed);
      op(s, "save");
    }
    planning.set_note(planning.snapshot(plan), part, "Nota nova");
    service.save();
    service.close();
    State second, restored;
    {
      auto store = Store::open(packet);
      second = update_directory(store, store.head(), root / "Reaberta.inde",
                                packet, "Escrita e planejamento alterados");
      check(second.sequence == 2, "New state missing");
      restored =
          store.restore(second, first.sequence, "Retomar o primeiro estado");
      check(restored.sequence == 3 && restored.hash == first.hash,
            "Restoration changed content");
      unpack_directory(store, restored.sequence, root / "Estado-anterior.inde");
      unpack_directory(store, second.sequence, root / "Estado-posterior.inde");
      store.verify();
    }
    check(inventory(root / "Estado-anterior.inde") == source_bytes,
          "Full original state was not restored");
    check(inventory(source) == source_bytes, "Original source was modified");
    service.open(root / "Estado-posterior.inde");
    check(service.writing().document(doc.id)->content.ends_with(
              "Continuação preservada."),
          "Later document state lost");
    check(planning.snapshot(plan).participations.front().note == "Nota nova",
          "Later planning state lost");
    service.close();
    std::ofstream report(root / "verificacao.json");
    report << project_file::encode_json(
                  V::Object{{"project_id", owner},
                            {"initial_sequence", first.sequence},
                            {"changed_sequence", second.sequence},
                            {"restored_sequence", restored.sequence},
                            {"original_preserved", true},
                            {"full_document_snapshot_equal", true},
                            {"alternatives_visibility_semantics_equal", true},
                            {"composition_modes_and_draft_equal", true},
                            {"objects_and_rows_equal", true},
                            {"planning_and_text_link_equal", true},
                            {"later_state_preserved", true},
                            {"schema", 37}})
           << '\n';
    report.close();
    if (!keep)
      fs::remove_all(root);
    std::cout << "P5 native schema37 roundtrip: full Document, PNG, table, "
                 "equation, divergent row, planning/text link, edition, "
                 "restoration and original preservation OK\n";
    if (keep)
      std::cout << "Synthetic demonstration: " << root << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\nSynthetic evidence: " << root << '\n';
    return 1;
  }
}
