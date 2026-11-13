#pragma once
#include "comic_model.hpp"
#include "publication_service.hpp"
namespace inde::comic {
struct Prepared {
  std::string id, comic_id, site_id, payload, payload_hash;
  std::int64_t revision{}, site_revision{};
};
class Service {
public:
  explicit Service(publication::Service &p) : publication_(p) {}
  std::filesystem::path database_path() const;
  std::vector<Book> books(bool removed = false) const;
  Book book(const std::string &) const;
  Book save(Book, std::int64_t expected);
  Book retire(const std::string &, std::int64_t expected, bool removed);
  Prepared prepare(const std::string &comic_id, const std::string &site_id,
                   std::int64_t expected_comic, std::int64_t expected_site,
                   const std::filesystem::path &resources);
  bool is_current(const Prepared &) const;
  std::filesystem::path materialize(const Prepared &) const;
  void adopt_project_copy(const std::string &previous);
  publication::Service &publication() { return publication_; }

private:
  const project::Project &project() const;
  std::unique_ptr<persistence::SqliteDatabase>
  database(bool create = false) const;
  void validate_work(const Book &) const;
  publication::Service &publication_;
};
} // namespace inde::comic
