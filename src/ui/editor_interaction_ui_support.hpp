#pragma once
#include "inde/project/narrative.hpp"
#include "inde/ui/author_vocabulary.hpp"
#include "inde/ui/accessibility.hpp"
#include <algorithm>
#include <gtkmm.h>
namespace inde::ui::interaction_ui {
inline Gtk::Label *label(Gtk::Box &box, const std::string &text) {
  auto *w = Gtk::make_managed<Gtk::Label>(text);
  w->set_wrap(true);
  w->set_xalign(0);
  box.append(*w);
  return w;
}
inline Gtk::Entry *entry(Gtk::Box &box, const std::string &name,
                         const std::string &value = {}) {
  label(box, name);
  auto *w = Gtk::make_managed<Gtk::Entry>();
  w->set_placeholder_text(name);
  set_accessible_label(*w, name);
  w->set_text(value);
  box.append(*w);
  return w;
}
inline std::string clipped(const std::string &s, std::size_t n = 180) {
  auto u = Glib::ustring(s);
  return u.size() > n ? u.substr(0, n).raw() + "…" : s;
}
inline std::string type_name(const std::vector<project::EntityType> &types,
                             const std::string &id) {
  auto t = std::find_if(types.begin(), types.end(),
                        [&](const auto &t) { return t.id == id; });
  return t == types.end() ? "Categoria indisponível" : author::category_label(*t);
}
inline bool same_entity(const project::NarrativeEntity &a,
                        const project::NarrativeEntity &b) {
  return a.id == b.id && a.entity_type_id == b.entity_type_id &&
         a.name == b.name && a.summary == b.summary &&
         a.updated_at == b.updated_at;
}
} // namespace inde::ui::interaction_ui
