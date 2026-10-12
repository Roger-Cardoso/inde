#pragma once
#include "inde/ui/ui_preferences.hpp"
namespace inde::ui::author {
// A presentation preference, not a reclassification or database migration.
inline bool nonfiction_groups() {
  return ui_preference("vocabulary", "work-group", "universe") == "collection";
}
inline std::string group_name(bool plural = false, bool capital = false) {
  if (nonfiction_groups())
    return plural ? (capital ? "Conjuntos de obras" : "conjuntos de obras")
                  : (capital ? "Conjunto de obras" : "conjunto de obras");
  return plural ? (capital ? "Universos" : "universos")
                : (capital ? "Universo" : "universo");
}
inline std::string catalog_name() { return "Obras e " + group_name(true); }
} // namespace inde::ui::author
