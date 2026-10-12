#pragma once
#include "inde/experimental/text_blocks.hpp"
#include <cstdint>
namespace inde::project {
inline constexpr std::size_t composition_payload_limit = 2 * 1024 * 1024;
struct BlockDraft {
  std::string expected_revision, text;
};
struct Composition {
  std::string name;
  experimental::TextBlocksLab blocks;
  experimental::ReadingProfile profile;
  std::map<std::string, Document>
      origins; // SHA256 of complete snapshot -> frozen Document.
  std::map<std::string, BlockDraft> drafts; // block -> unapplied durable draft.
};
struct StoredComposition {
  Composition value;
  std::string project_id, sha256, updated_at;
  std::int64_t revision{};
};
struct CompositionSummary {
  std::string id, name, updated_at;
  std::int64_t revision{};
};
} // namespace inde::project
