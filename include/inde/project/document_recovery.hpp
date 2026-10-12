#pragma once
#include "inde/project/project.hpp"
#include "inde/project/writing.hpp"
#include <cstdint>

namespace inde::project {
inline constexpr std::size_t recovery_payload_limit = 64 * 1024 * 1024;
struct RecoveryState {
  Document document;
  std::size_t cursor{}, bound{};
};
struct RecoveryRecord {
  std::string project_id, session_id;
  std::uint64_t revision{}, durable_revision{};
  RecoveryState state;
  Document baseline;
  std::string status{"pending"}, updated_at, checksum;
};
struct RecoverySummary {
  std::string session_id, document_id, title, updated_at;
  std::uint64_t revision{};
};
struct RecoveryPage {
  std::vector<RecoverySummary> items;
  bool more{};
};
struct RecoveryRetentionEntry {
  std::string session_id, title, status, updated_at, checksum, explanation;
  std::size_t payload_bytes{};
  bool exportable{}, removable{};
};
struct RecoveryRetentionPage {
  std::vector<RecoveryRetentionEntry> items;
  bool more{};
  std::size_t allocated_bytes{}, reusable_bytes{};
};
} // namespace inde::project
