#pragma once
#include "inde/persistence/json.hpp"
#include <filesystem>
#include <memory>

namespace inde::objects::pilot {
// The owner serializer uses default stream precision. Pilot geometry requires
// round-trippable doubles; this isolated codec preserves max_digits10.
[[nodiscard]] std::string encode_json(const persistence::json::Value &);
// P2-A only: synthetic, authoring-private document + PNG. No legacy writer,
// GTK integration, tables, publication or claim of unsaved recovery.
class Session final {
public:
  explicit Session(const std::filesystem::path &workspace);
  ~Session();
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  [[nodiscard]] persistence::json::Value
  request(const persistence::json::Value &);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace inde::objects::pilot
