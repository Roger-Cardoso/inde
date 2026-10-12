#pragma once
#include <gtkmm.h>
#include <functional>
#include <string>
#include <vector>

namespace inde::ui {
// Lock bits do not change a chord. AltGr, Super and surplus modifiers do.
unsigned shortcut_modifiers(Gdk::ModifierType);
bool shortcut_matches(guint key, Gdk::ModifierType, const std::string &accelerator);
bool focus_inside(Gtk::Widget *focus, const Gtk::Widget &region);
bool text_input_focus(Gtk::Widget *focus);
bool input_composition_active(Gtk::Widget *focus);

struct KeyboardCommand {
  std::string id, label, group, accelerator, aliases;
  std::function<bool(Gtk::Widget *)> scope;
  std::function<std::string()> unavailable; // empty means enabled
  std::function<void()> activate;
};

// One router per window. Commands have explicit owners/scopes, not label-based
// button scraping. Palette activation revalidates after restoring its origin.
class KeyboardCommands {
public:
  KeyboardCommands(Gtk::Window &, Gtk::Widget &anchor,
                   std::function<bool()> blocked,
                   std::function<void(const std::string &)> status = {});
  ~KeyboardCommands();
  void add(KeyboardCommand);
  void show(bool help = false);
  bool dispatch(guint, Gdk::ModifierType);
  bool rebind(const std::string &id, const std::string &accelerator, std::string &error);
  void restore_defaults();
  std::string binding(const KeyboardCommand &) const;
  const std::vector<KeyboardCommand> &commands() const { return commands_; }
  void regions(std::function<std::vector<Gtk::Widget *>()> value) { regions_ = std::move(value); }
private:
  void rebuild();
  void execute(std::string id);
  Gtk::Widget *origin() const;
  void restore_origin();
  void cycle_region(bool reverse);
  Gtk::Window &window_;
  std::function<bool()> blocked_;
  std::function<void(const std::string &)> status_;
  std::function<std::vector<Gtk::Widget *>()> regions_;
  std::vector<KeyboardCommand> commands_;
  GWeakRef origin_{};
  GWeakRef last_context_focus_{};
  Glib::RefPtr<Gtk::TextBuffer> origin_buffer_;
  Glib::RefPtr<Gtk::TextMark> origin_cursor_, origin_bound_;
  int origin_entry_cursor_{}, origin_entry_bound_{};
  bool origin_editable_{};
  Gtk::Popover palette_;
  Gtk::Box body_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Label heading_, hint_;
  Gtk::SearchEntry search_;
  Gtk::ScrolledWindow scroll_;
  Gtk::ListBox results_;
  Gtk::Box bindings_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Entry binding_entry_;
  Gtk::Button assign_{"Alterar atalho"}, remove_{"Sem atalho"}, reset_{"Restaurar padrões"};
  std::vector<std::string> result_ids_;
  bool help_{};
  sigc::connection focus_changed_;
};
}
