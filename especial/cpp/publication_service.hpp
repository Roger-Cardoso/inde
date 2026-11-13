#pragma once
#include "snapshot_model.hpp"
#include "inde/application/project_service.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include <map>
#include <memory>

namespace inde::publication {
struct Site {
  std::string id, name, author, description;
  std::string theme{"violeta"}, origin, base_path, destination{"local"}, repository, branch{"main"};
  std::int64_t revision{};
  bool removed{};
  std::vector<std::string> catalog;
};
struct Edition {
  std::string id, site_id, title, work_id, structure_id, slug;
  application::ProductionChoices choices;
  std::vector<std::string> entities;
  std::map<std::string,std::int64_t> public_numbers;
  std::int64_t next_number{1}, revision{};
  bool removed{};
  V public_identity;
};
struct Prepared {
  std::string id, site_id, edition_id, source_hash, payload_hash, payload;
  std::int64_t site_revision{}, edition_revision{};
  bool catalog{};
};
V encode(const Site &);
V encode(const Edition &);
Site site_from_json(const V &);
Edition edition_from_json(const V &);

// All operations use the same live ProjectService as the owning INDE window.
// Publication is an explicitly versioned extension; the base schema stays 28.
class Service {
public:
  explicit Service(application::ProjectService &source): source_(source) {}
  std::filesystem::path database_path() const;
  std::vector<Site> sites(bool include_removed=false) const;
  std::vector<Edition> editions(const std::string &site,bool include_removed=false) const;
  Site site(const std::string &) const;
  Edition edition(const std::string &) const;
  Site create_site(Site);
  Site update_site(Site,std::int64_t expected);
  Site update_catalog(const std::string &,std::int64_t expected,const std::vector<std::string> &);
  Site remove_site(const std::string &,std::int64_t expected);
  Site restore_site(const std::string &,std::int64_t expected);
  Edition create_edition(Edition);
  Edition update_edition(Edition,std::int64_t expected);
  Edition bind_literature_identity(const std::string &,std::int64_t expected,const V &);
  Edition remove_edition(const std::string &,std::int64_t expected);
  Edition restore_edition(const std::string &,std::int64_t expected);
  Prepared prepare(const std::string &,std::int64_t expected_site,std::int64_t expected_edition);
  Prepared prepare_catalog(const std::string &,std::int64_t expected_site);
  bool is_current(const Prepared &) const;
  std::vector<Prepared> preparations(const std::string &) const;
  std::filesystem::path materialize(const Prepared &) const;
  void adopt_project_copy(const std::string &previous_project_id);
  application::ProjectService &source() { return source_; }
private:
  const project::Project &project() const;
  std::unique_ptr<persistence::SqliteDatabase> database(bool create=false) const;
  std::unique_ptr<persistence::SqliteDatabase> writable_database(bool create=false) const;
  void upgrade(persistence::SqliteDatabase &) const;
  void validate_catalog(const Site &) const;
  V capture_catalog(const Site &) const;
  V capture(const Edition &,const Site &) const;
  void validate_site(const Site &) const;
  void validate_edition(Edition &,bool allocate_numbers) const;
  void validate_public_identity(const Edition &) const;
  Site change_site_state(const std::string &,std::int64_t,bool);
  Edition change_edition_state(const std::string &,std::int64_t,bool);
  application::ProjectService &source_;
};
}
