#pragma once
#include "inde/persistence/json.hpp"
#include <filesystem>
namespace inde::ui {
class ObjectDocumentEditor;
// Read-only observations for the owned GTK integration test. Mutations use UI.
struct ObjectNativeProbe {
  static const persistence::json::Value &state(const ObjectDocumentEditor &);
  static bool busy(const ObjectDocumentEditor &);
  static bool tools_open(const ObjectDocumentEditor &);
  static void reveal_object(ObjectDocumentEditor &, const std::string &);
  static void open_context(ObjectDocumentEditor &, const std::string &);
  static bool draft(const ObjectDocumentEditor &);
  static void choose_image(ObjectDocumentEditor &,
                           const std::filesystem::path &);
  static std::string error(const ObjectDocumentEditor &);
};
} // namespace inde::ui
