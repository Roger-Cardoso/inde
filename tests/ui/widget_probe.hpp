#pragma once

#include <gtkmm.h>
#include <stdexcept>
#include <string>

namespace inde::test::ui {
inline bool combo_has_id(const Gtk::ComboBoxText &combo, const std::string &id) {
  const auto model=combo.get_model();
  const auto column=combo.get_id_column();
  if(!model || column<0)return false;
  for(const auto &row:model->children()) {
    Glib::ustring value;row.get_value(column,value);
    if(value.raw()==id)return true;
  }
  return false;
}
inline void require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

template <class T, class Predicate>
T *find(Gtk::Widget &root, Predicate predicate) {
  if (auto *value = dynamic_cast<T *>(&root); value && predicate(*value))
    return value;
  for (auto *child = root.get_first_child(); child;
       child = child->get_next_sibling())
    if (auto *found = find<T>(*child, predicate))
      return found;
  return nullptr;
}

inline void click(Gtk::Widget &root, const std::string &label) {
  auto *button = find<Gtk::Button>(
      root, [&](auto &b) {
        return b.get_label().raw() == label &&
               dynamic_cast<Gtk::ToggleButton *>(&b) == nullptr;
      });
  if (!button)
    button = find<Gtk::Button>(
      root, [&](auto &b) {
        return b.get_label().raw() == label || find<Gtk::Label>(b,[&](auto &l){return l.get_text().raw()==label;});
      });
  require(button && button->get_sensitive(), "Button unavailable: " + label);
  g_signal_emit_by_name(button->gobj(), "clicked");
}

inline Gtk::Stack &pages(Gtk::Widget &root, const std::string &page) {
  auto *stack = find<Gtk::Stack>(
      root, [&](auto &s) { return s.get_child_by_name(page) != nullptr; });
  require(stack, "Missing stack page: " + page);
  return *stack;
}

template <class Predicate>
std::size_t count(Gtk::Widget &root, Predicate predicate) {
  std::size_t result = predicate(root) ? 1 : 0;
  for (auto *child = root.get_first_child(); child;
       child = child->get_next_sibling())
    result += count(*child, predicate);
  return result;
}
} // namespace inde::test::ui
