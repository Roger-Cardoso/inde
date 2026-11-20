#pragma once
#include "object_change_session.hpp"
namespace inde::objects::planning::checkpoint_detail {
inline constexpr std::size_t byte_limit = 64 * 1024 * 1024;
struct Data {
  bool composition{}, pending{};
  std::string project_id, base, working, timestamp;
  std::int64_t persisted_revision{};
  application::TextSelection selection;
  Layout layout;
  std::vector<std::string> object_order;
  std::vector<Occurrence>
      frames; // Only id/frame serialized; no object authority.
};
void bounded_source(const std::string &);
SessionCheckpoint encode(const Data &);
Data decode(const SessionCheckpoint &);
Composition presentation(const Data &, Composition canonical);
} // namespace inde::objects::planning::checkpoint_detail
