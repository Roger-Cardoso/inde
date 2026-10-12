#pragma once

#include "inde/application/project_service.hpp"
#include "inde/project/writing.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace inde::demo {

using inde::project::RelationDirectionality;
using inde::project::StructuralNodeType;

struct EntitySpec {
  const char *key;
  const char *name;
  const char *summary;
};

struct ChapterSpec {
  const char *title;
  const char *synopsis;
  const char *scene_a;
  const char *scene_b;
};

struct EventSpec {
  const char *key;
  const char *name;
  const char *summary;
  const char *axis;
  int point;
  const char *location;
  std::array<const char *, 3> participants;
};

class DemoBuilder {
public:
  explicit DemoBuilder(std::filesystem::path output);

  void build();

private:
  std::string add_node(const std::string &key, const std::string &work,
                       const std::optional<std::string> &parent,
                       StructuralNodeType type, const std::string &title,
                       const std::string &subtitle, const std::string &synopsis,
                       const std::string &status = "Planejamento",
                       const std::string &custom_type = {});

  void add_entity(const std::string &type, const EntitySpec &spec);

  void add_relation(const std::string &type, const std::string &source,
                    const std::string &target, const std::string &description);

  static std::pair<std::size_t, std::size_t>
  unicode_range(const std::string &content, const std::string &needle);

  static void add_format(inde::project::Document &document,
                         inde::project::DocumentTextStyle style,
                         const std::string &needle);

  static std::string add_anchor(inde::project::Document &document,
                                const std::string &label,
                                const std::string &needle);

  void add_document_reference(inde::project::Document &document,
                              const std::string &entity,
                              const std::optional<std::string> &anchor,
                              const std::string &notes);

  inde::project::Document begin_document(const std::string &title,
                                         const std::string &content,
                                         std::optional<std::string> node_key,
                                         std::optional<std::size_t> word_goal);

  void finish_document(const std::string &key,
                       inde::project::Document document);

  void create_catalog();

  void create_structures();

  void create_main_structure();

  void create_novella_structure();

  void create_story_structure();

  void create_dossier_structure();

  void create_entities();

  void create_event_entities();

  void create_relations();

  void create_timeline();

  std::string display_name(const std::string &key) const;

  void create_scopes_and_references();

  void create_documents();

  void organize_documents();

  void create_start_document();

  void create_prologue_document();

  void create_salt_on_paper_document();

  void create_archive_document();

  void create_two_maps_document();

  void create_evacuation_document();

  void create_open_atlas_document();

  void create_lighthouse_letter_document();

  void create_salt_garden_document();

  void create_protocol_dossier_document();

  void create_continuity_notebook_document();

  void create_discarded_scene_document();

  void create_narrative_structures();

  void audit() const;

  std::filesystem::path output_;
  inde::application::ProjectService service_;
  std::map<std::string, std::string> ips_;
  std::map<std::string, std::string> works_;
  std::map<std::string, std::string> nodes_;
  std::map<std::string, std::string> types_;
  std::map<std::string, std::string> entities_;
  std::vector<std::string> entity_keys_;
  std::map<std::string, std::string> relation_types_;
  std::map<std::string, std::string> axes_;
  std::map<std::string, std::vector<std::string>> points_;
  std::map<std::string, std::string> occurrences_;
  std::map<std::string, std::string> documents_;
  std::vector<EventSpec> events_;
};

} // namespace inde::demo
