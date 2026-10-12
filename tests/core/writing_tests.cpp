#include "inde/application/catalog_service.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/project_session.hpp"
#include "inde/application/proofreading_service.hpp"
#include "inde/application/spelling_provider.hpp"
#include "inde/application/structural_service.hpp"
#include "inde/application/temporal_view.hpp"
#include "inde/application/timeline_projection.hpp"
#include "inde/persistence/catalog_repository.hpp"
#include "inde/persistence/dictionary_store.hpp"
#include "inde/persistence/enchant_spelling_provider.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/project_database_importer.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/project_repository.hpp"
#include "inde/persistence/recent_projects_store.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_catalog_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/persistence/sqlite_structural_repository.hpp"
#include "inde/persistence/structural_repository.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sqlite3.h>

#include "core_test_cases.hpp"
#include "test_support.hpp"
#include "sqlite_fixture.hpp"

namespace inde::tests {

void writing_documents_persist_without_collapsing_editorial_identity() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;

  bool guarded_without_project = false;
  try {
    static_cast<void>(service.writing().documents());
  } catch (const std::exception &) {
    guarded_without_project = true;
  }
  assert(guarded_without_project);

  const auto source_path =
      service.create(temporary.path / "Fonte escrita", "Fonte escrita").path();
  const auto source_project_id = service.current()->manifest().project_id;
  const auto ip_id =
      service.create_intellectual_property("Universo", "", "").id;
  const auto work_id =
      service.create_work(ip_id, "Romance", "", "", "pt-BR", "Em escrita").id;
  const auto chapter_id =
      service
          .create_structural_node(work_id, std::nullopt,
                                  inde::project::StructuralNodeType::Chapter,
                                  "Capítulo do Sol")
          .id;
  const auto entity_types = service.narrative().entity_types();
  const auto character_type =
      std::find_if(entity_types.begin(), entity_types.end(),
                   [](const auto &type) { return type.key == "character"; });
  assert(character_type != entity_types.end());
  const auto character = service.narrative().create_entity(
      character_type->id, "Aurora", "Personagem vinculada à Escrita");

  const auto free_document =
      service.writing().create_document("Rascunho 100% livre");
  auto placed_document =
      service.writing().create_document("A chegada", chapter_id);
  placed_document.content =
      "Rascunho 100% livre é citado aqui.\n\n"
      "O sol nasce sobre a cidade.\n\nA personagem finalmente chega.\n" +
      std::string(256 * 1024, 'x') + "\nFim Unicode: 🚀";
  placed_document.word_goal = 90000;
  placed_document.formatting.push_back(
      {inde::project::DocumentTextStyle::Bold, 0, 8});
  const auto anchor_id = inde::project::new_uuid();
  placed_document.anchors.push_back({anchor_id, "Chegada de Aurora", 38, 66});
  placed_document.entity_references.push_back(
      {inde::project::new_uuid(), character.id, anchor_id,
       "Aurora participa deste trecho"});
  placed_document = service.writing().update_document(placed_document);
  const auto group = service.writing().create_document_group(
      "Capítulos em revisão", "Conjunto virtual de trabalho");
  placed_document.group_id = group.id;
  placed_document.purpose = inde::project::DocumentPurpose::Revision;
  placed_document.revision_of_id = free_document.id;
  placed_document.revision_label = "v2 — ritmo";
  placed_document.perspective = "Aurora";
  placed_document = service.writing().update_document(placed_document);

  assert(service.writing().document_count() == 2);
  assert(service.writing().document_groups().size() == 1);
  inde::persistence::DocumentQuery by_group;
  by_group.group_id = group.id;
  assert(service.writing().document_count(by_group) == 1);
  inde::persistence::DocumentQuery by_purpose;
  by_purpose.purpose = inde::project::DocumentPurpose::Revision;
  by_purpose.perspective = "Aur";
  by_purpose.revisions_only = true;
  assert(service.writing().document_summaries(by_purpose).front().id ==
         placed_document.id);
  assert(!free_document.editorial_node_id);
  assert(placed_document.editorial_node_id == chapter_id);

  inde::persistence::DocumentQuery by_content;
  by_content.search = "sol";
  const auto content_results = service.writing().documents(by_content);
  assert(content_results.size() == 1);
  assert(content_results.front().id == placed_document.id);
  const auto content_summaries =
      service.writing().document_summaries(by_content);
  assert(content_summaries.size() == 1);
  assert(content_summaries.front().id == placed_document.id);
  assert(content_summaries.front().character_count > 256 * 1024);
  assert(content_summaries.front().search_match ==
         inde::project::DocumentSearchMatch::Content);
  assert(content_summaries.front().entity_reference_count == 1);

  inde::persistence::DocumentQuery ranked;
  ranked.search = "Rascunho 100% livre";
  const auto ranked_results = service.writing().document_summaries(ranked);
  assert(ranked_results.size() == 2);
  assert(ranked_results.front().id == free_document.id);
  assert(ranked_results.front().search_match ==
         inde::project::DocumentSearchMatch::Name);
  assert(ranked_results.back().id == placed_document.id);
  assert(ranked_results.back().search_match ==
         inde::project::DocumentSearchMatch::Content);

  inde::persistence::DocumentQuery literal_wildcard;
  literal_wildcard.search = "_";
  const auto literal_results = service.writing().documents(literal_wildcard);
  assert(literal_results.empty());

  inde::persistence::DocumentQuery by_placement;
  by_placement.editorial_node_id = chapter_id;
  assert(service.writing().document_count(by_placement) == 1);
  assert(service.writing().documents(by_placement).front().id ==
         placed_document.id);

  inde::persistence::DocumentQuery by_entity;
  by_entity.entity_id = character.id;
  assert(service.writing().document_count(by_entity) == 1);
  assert(service.writing().document_summaries(by_entity).front().id ==
         placed_document.id);

  inde::application::PlanningContext entities_used_in_writing;
  entities_used_in_writing.require_document_reference = true;
  const auto used_snapshot =
      service.planning().explore_entities(entities_used_in_writing);
  assert(used_snapshot.matching_count == 1);
  assert(used_snapshot.items.front().entity.id == character.id);
  assert(used_snapshot.items.front().primary_reason.find("Documento") !=
         std::string::npos);
  entities_used_in_writing.document_id = placed_document.id;
  const auto document_snapshot =
      service.planning().explore_entities(entities_used_in_writing);
  assert(document_snapshot.matching_count == 1);
  assert(document_snapshot.items.front().entity.id == character.id);

  auto invalid = placed_document;
  invalid.title.clear();
  bool invalid_title_rejected = false;
  try {
    static_cast<void>(service.writing().update_document(invalid));
  } catch (const std::exception &) {
    invalid_title_rejected = true;
  }
  assert(invalid_title_rejected);
  assert(service.writing().document(placed_document.id)->title == "A chegada");

  auto invalid_reference = placed_document;
  invalid_reference.entity_references.push_back(
      {inde::project::new_uuid(), inde::project::new_uuid(), std::nullopt, ""});
  bool invalid_reference_rejected = false;
  try {
    static_cast<void>(service.writing().update_document(invalid_reference));
  } catch (const std::exception &) {
    invalid_reference_rejected = true;
  }
  assert(invalid_reference_rejected);
  assert(service.writing()
             .document(placed_document.id)
             ->entity_references.size() == 1);

  std::string long_paragraph;
  for (int index = 0; index < 125; ++index)
    long_paragraph += "palavra ";
  const auto statistics = inde::project::analyze_document_text(
      "eco eco eco.\n\n" + long_paragraph, 200, 120);
  assert(statistics.words == 128);
  assert(statistics.paragraphs == 2);
  assert(statistics.sentences == 1);
  assert(statistics.reading_minutes == 1);
  assert(!statistics.repeated_words.empty());
  assert(statistics.long_paragraphs.size() == 1);
  const auto line_paragraphs =
      inde::project::analyze_document_text("primeiro\nsegundo\n\nterceiro");
  assert(line_paragraphs.paragraphs == 3);
  assert(inde::project::analyze_document_text("Espere... o quê?!").sentences ==
         2);

  service.save();
  service.close();
  service.open(source_path);
  assert(service.writing().document_count() == 2);
  assert(service.writing().document(placed_document.id)->content ==
         placed_document.content);
  const auto reopened_document = service.writing().document(placed_document.id);
  assert(reopened_document->word_goal == 90000);
  assert(reopened_document->formatting.size() == 1);
  assert(reopened_document->anchors.size() == 1);
  assert(reopened_document->entity_references.size() == 1);
  assert(reopened_document->group_id == group.id);
  assert(reopened_document->purpose ==
         inde::project::DocumentPurpose::Revision);
  assert(reopened_document->revision_of_id == free_document.id);
  assert(reopened_document->revision_label == "v2 — ritmo");
  assert(reopened_document->perspective == "Aurora");

  const auto copy = service.save_as(temporary.path / "Cópia escrita");
  const auto copy_project_id = copy.manifest().project_id;
  assert(copy_project_id != source_project_id);
  assert(service.writing().document_count() == 2);
  assert(service.writing().document_groups().size() == 1);
  auto copied_document = *service.writing().document(placed_document.id);
  copied_document.content += "\nEsta frase existe apenas na cópia.";
  copied_document = service.writing().update_document(copied_document);

  // A colocação é uma referência opcional: remover a unidade editorial
  // preserva o Documento e apenas limpa sua localização na cópia.
  service.delete_structural_branch(chapter_id);
  assert(!service.writing()
              .document(placed_document.id)
              ->editorial_node_id.has_value());
  bool used_entity_refused{};
  try { service.narrative().delete_entity(character.id); }
  catch(const std::exception&){ used_entity_refused=true; }
  assert(used_entity_refused && service.writing().document(placed_document.id)->entity_references.size()==1);
  auto unlinked=*service.writing().document(placed_document.id);
  unlinked.entity_references.clear();
  static_cast<void>(service.writing().update_document(unlinked));
  service.narrative().delete_entity(character.id);
  assert(service.writing()
             .document(placed_document.id)
             ->entity_references.empty());
  assert(service.writing().document(placed_document.id)->anchors.size() == 1);
  assert(service.writing().document(placed_document.id)->formatting.size() ==
         1);
  bool used_source_refused{};
  try { service.writing().delete_document(free_document.id); }
  catch(const std::exception&){ used_source_refused=true; }
  assert(used_source_refused && service.writing().document(free_document.id));
  unlinked=*service.writing().document(placed_document.id);
  unlinked.revision_of_id.reset();
  static_cast<void>(service.writing().update_document(unlinked));
  service.writing().delete_document(free_document.id);
  assert(service.writing().document_count() == 1);

  inde::persistence::SqliteDatabase copied_database(copy.path() / "data" /
                                                    "project.sqlite3");
  assert(copied_database.query_text(
             "SELECT DISTINCT project_id FROM documents") == copy_project_id);
  assert(copied_database.query_text(
             "SELECT DISTINCT project_id FROM document_groups") ==
         copy_project_id);

  inde::application::ProjectService original;
  original.open(source_path);
  assert(original.writing().document_count() == 2);
  const auto original_document =
      original.writing().document(placed_document.id);
  assert(original_document);
  assert(original_document->content == placed_document.content);
  assert(original_document->editorial_node_id == chapter_id);
  assert(original_document->word_goal == 90000);
  assert(original_document->formatting.size() == 1);
  assert(original_document->anchors.size() == 1);
  assert(original_document->entity_references.size() == 1);
  assert(original_document->group_id == group.id);
  assert(original_document->perspective == "Aurora");
}

void textual_references_preserve_identity_and_protect_sources() {
  using namespace inde;
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  application::ProjectService service;
  const auto original_path =
      service.create(temporary.path / "Referências", "Referências").path();
  auto source = service.writing().create_document("Guia");
  auto target = service.writing().create_document("Profecia Ω");
  target.content = "Ação 🌍\nQuando a lua surgir, voltaremos.";
  const auto anchor_id = project::new_uuid();
  target.anchors.push_back({anchor_id, "Juramento", 7, 37});
  target = service.writing().update_document(target);
  const auto third = service.writing().create_document("Apêndice");
  const auto rejected = [](auto operation) {
    bool failed = false;
    try {
      operation();
    } catch (const std::exception &) {
      failed = true;
    }
    assert(failed);
  };

  // Migração aditiva de v12 preserva corpo e identidade de âncora existentes.
  service.close();
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(original_path));
    downgrade_v14_to_v13(db);
    db.execute("DROP TABLE document_text_references; DROP INDEX "
               "document_anchors_identity_idx;"
               "DELETE FROM schema_migrations WHERE version = 13;");
    assert(persistence::SchemaMigrator{}.current_version(db) == 12);
  }
  service.open(original_path);
  assert(service.writing().document(target.id)->content == target.content);
  assert(service.writing().document(target.id)->anchors.front().id ==
         anchor_id);
  assert(service.writing().text_references({target.id, true}).total == 0);

  const auto link = service.writing().add_text_reference(
      source.id, target.id, anchor_id, "Fonte canônica");
  const auto whole = service.writing().add_text_reference(source.id, target.id);
  const auto other =
      service.writing().add_text_reference(third.id, target.id, anchor_id);
  assert(service.writing().text_references({target.id, true}).total == 3);
  assert(
      service.writing().text_references({target.id, true, anchor_id}).total ==
      2);
  const auto outgoing = service.writing().text_references({source.id, false});
  assert(outgoing.total == 2);
  auto query = persistence::WritingStore::TextReferenceQuery{
      target.id, true, std::nullopt, 1, 0};
  const auto first = service.writing().text_references(query);
  assert(first.items.size() == 1 && first.total == 3);
  query.offset = 1;
  const auto second = service.writing().text_references(query);
  assert(second.items.size() == 1 &&
         first.items[0].reference.id != second.items[0].reference.id);
  query.offset = 100;
  assert(service.writing().text_references(query).items.empty());
  query.limit = 0;
  rejected(
      [&] { static_cast<void>(service.writing().text_references(query)); });
  query.limit = 501;
  rejected(
      [&] { static_cast<void>(service.writing().text_references(query)); });
  rejected([&] {
    static_cast<void>(
        service.writing().add_text_reference(source.id, target.id, anchor_id));
  });
  rejected([&] {
    static_cast<void>(
        service.writing().add_text_reference(source.id, target.id));
  });
  rejected([&] {
    static_cast<void>(
        service.writing().add_text_reference(source.id, source.id));
  });
  rejected([&] {
    static_cast<void>(
        service.writing().add_text_reference(source.id, third.id, anchor_id));
  });
  rejected([&] {
    static_cast<void>(
        service.writing().add_text_reference(source.id, "missing"));
  });
  rejected([&] { service.writing().remove_text_reference(third.id, link.id); });

  // A FK protege também chamadas diretas ao repositório, inclusive contra
  // âncora pertencente a outro Documento e gravações parciais no change_log.
  persistence::SqliteWritingRepository repository;
  const auto change_count = [&] {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(original_path));
    return db.query_integer("SELECT count(*) FROM change_log");
  };
  const auto changes_before_rejections = change_count();
  auto invalid = link;
  invalid.id = project::new_uuid();
  invalid.target_document_id = third.id;
  rejected([&] { repository.add_text_reference(original_path, invalid); });
  invalid.target_document_id = "missing";
  invalid.target_anchor_id.reset();
  rejected([&] { repository.add_text_reference(original_path, invalid); });
  rejected([&] { service.writing().delete_document(target.id); });
  rejected([&] { repository.remove(original_path, target.id); });
  auto without_anchor = target;
  without_anchor.anchors.clear();
  without_anchor.content = "Não deve ser salvo";
  rejected([&] {
    static_cast<void>(service.writing().update_document(without_anchor));
  });
  rejected([&] { repository.save(original_path, without_anchor); });
  assert(service.writing().document(target.id)->content == target.content);
  assert(change_count() == changes_before_rejections);

  target.title = "Profecia renomeada";
  target.anchors.front().label = "Promessa";
  target.content = "Antes: " + target.content;
  target.anchors.front().start_offset += 7;
  target.anchors.front().end_offset += 7;
  target = service.writing().update_document(target);
  auto changed =
      service.writing().text_references({source.id, false, anchor_id});
  assert(changed.items.front().reference.id == link.id);
  assert(changed.items.front().target_title == target.title);
  assert(changed.items.front().target_anchor_label == "Promessa");
  assert(service.writing().document(source.id)->content.empty());
  service.close();
  service.open(original_path);
  assert(service.writing().text_references({target.id, true}).total == 3);

  // Salvar como conserva as referências com nova identidade do Projeto.
  const auto copied_path = service.save_as(temporary.path / "Cópia").path();
  assert(copied_path != original_path);
  assert(service.writing().text_references({target.id, true}).total == 3);
  // Consultar usos não exige bloqueio de escrita nem materializa o corpo.
  {
    persistence::SqliteDatabase writer(
        persistence::ProjectDatabaseRepository::database_path(copied_path));
    persistence::SqliteTransaction pending(writer);
    assert(service.writing().text_references({target.id, true}).total == 3);
  }
  service.writing().delete_document(source.id);
  assert(service.writing().text_references({target.id, true}).total == 1);
  service.writing().remove_text_reference(third.id, other.id);
  target.anchors.clear();
  target = service.writing().update_document(target);
  service.writing().delete_document(target.id);
  service.close();
  service.open(original_path);
  assert(service.writing().text_references({target.id, true}).total == 3);
  assert(service.writing().document(target.id)->anchors.front().id ==
         anchor_id);
  for (const auto &path : {original_path, copied_path}) {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(path));
    assert(db.query_text("PRAGMA integrity_check") == "ok");
    auto fk = db.prepare("PRAGMA foreign_key_check");
    assert(!fk.step());
  }

  // Paginação real acima do tamanho da UI, com fonte de 1 MiB.
  target = *service.writing().document(target.id);
  target.content += std::string(1024 * 1024, 'x');
  target = service.writing().update_document(target);
  for (int index = 0; index < 65; ++index) {
    const auto consumer =
        service.writing().create_document("Uso " + std::to_string(index));
    static_cast<void>(service.writing().add_text_reference(
        consumer.id, target.id, anchor_id));
  }
  auto page_query =
      persistence::WritingStore::TextReferenceQuery{target.id, true};
  const auto page_a = service.writing().text_references(page_query);
  page_query.offset = 50;
  const auto page_b = service.writing().text_references(page_query);
  assert(page_a.total == 68 && page_a.items.size() == 50 &&
         page_b.items.size() == 18);
  for (const auto &a : page_a.items)
    for (const auto &b : page_b.items)
      assert(a.reference.id != b.reference.id);
  const auto started = std::chrono::steady_clock::now();
  for (int index = 0; index < 100; ++index)
    assert(service.writing().text_references(page_query).total == 68);
  const auto ms = std::chrono::duration<double, std::milli>(
                      std::chrono::steady_clock::now() - started)
                      .count();
  std::cout << "Referências textuais: 68 usos, fonte 1 MiB, página 50; média "
            << ms / 100.0 << " ms/consulta.\n";
}

} // namespace inde::tests
