#pragma once
#include "inde/persistence/json.hpp"
#include <filesystem>
namespace inde::ui {
class ObjectDocumentWindow;
// Read-only observations for the owned GTK integration test. Mutations use UI.
struct ObjectNativeProbe {
  static const persistence::json::Value &state(const ObjectDocumentWindow &);
  static bool busy(const ObjectDocumentWindow &);
  static bool draft(const ObjectDocumentWindow &);
  static void choose_image(ObjectDocumentWindow &,
                           const std::filesystem::path &);
  static std::string error(const ObjectDocumentWindow &);
};
} // namespace inde::ui
