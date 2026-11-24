#pragma once
#include <functional>
#include <gtkmm.h>

namespace inde::ui {
// A paragraph viewport onto Writing's authoritative buffer. It owns no document
// or history: accepted input goes through the original buffer's edit guards.
class ObjectTextProjection final : public Gtk::TextView {
public:
  ObjectTextProjection(const Glib::RefPtr<Gtk::TextBuffer> &, int start,
                       int end);
  ~ObjectTextProjection() override;
  void refresh();
  bool contains(int offset) const;
  void focus_offset(int offset);
  bool pending_input() const { return cross_edit_ || preedit_active_; }
  std::function<bool(guint, Gdk::ModifierType)> shortcut;
  std::function<void(const char *)> preedit;
  std::function<void(int, bool)> navigate;
  std::function<bool(const std::string &)> replace_selection;
  std::function<bool()> editable;
  std::function<void()> layout_changed;

protected:
  void on_realize() override;
  void on_unrealize() override;
  void size_allocate_vfunc(int width, int height, int baseline) override;

private:
  Glib::RefPtr<Gtk::TextBuffer> source_;
  Glib::RefPtr<Gtk::TextMark> start_, end_;
  bool reflecting_ = false;
  sigc::connection pending_;
  sigc::connection cross_pending_;
  bool cross_edit_ = false;
  bool primary_detached_ = false;
  bool preedit_active_ = false;
  gulong preedit_handler_ = 0;
  int layout_width_ = -1, layout_height_ = -1;
  Glib::ustring cross_source_, cross_local_;
  int cross_cursor_{}, cross_bound_{}, cross_first_{}, cross_suffix_{};
  std::vector<sigc::connection> connections_;
  int start() const;
  int end() const;
  void schedule();
  void select_source();
  bool ready() const;
  bool begin_cross_edit();
  void finish_cross_edit();
  void restore_primary_registration();
};
} // namespace inde::ui
