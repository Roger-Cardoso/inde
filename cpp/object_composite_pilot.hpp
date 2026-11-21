#pragma once
#include "inde/persistence/json.hpp"
#include <filesystem>
#include <functional>
#include <memory>

namespace inde::objects::pilot {
// The owner serializer uses default stream precision. Pilot geometry requires
// round-trippable doubles; this isolated codec preserves max_digits10.
[[nodiscard]] std::string encode_json(const persistence::json::Value &);
enum class Profile { P2A, P2B, P2C };
enum class PersistenceStage {
  BeforeWrite,
  AfterBlobs,
  AfterSnapshot,
  BeforeCommit,
  AfterCommit
};
using PersistenceHook = std::function<void(std::string_view, PersistenceStage)>;
// Synthetic, authoring-private pilot. P2-B adds tables and bounded live
// history; the default P2-A format stays unchanged. No canonical writer or
// recovery.
class Session final {
public:
  explicit Session(const std::filesystem::path &workspace,
                   Profile profile = Profile::P2A,
                   const std::filesystem::path &seed_previous = {},
                   PersistenceHook hook = {});
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
