#include "object_text_projection.hpp"
#include <algorithm>
#include <utility>

namespace inde::ui {
ObjectTextProjection::ObjectTextProjection(
    const Glib::RefPtr<Gtk::TextBuffer> &source, int a, int b)
    : Glib::ObjectBase("IndeObjectTextProjection"), Gtk::TextView(),
      source_(source),
      start_(source->create_mark(source->get_iter_at_offset(a), true)),
      end_(source->create_mark(source->get_iter_at_offset(b), false)) {
  set_buffer(Gtk::TextBuffer::create(source->get_tag_table()));
  get_buffer()->set_enable_undo(false);
  set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
  set_hexpand(true);
  set_size_request(-1, 32); // Readable before the first GTK text-layout pass.
  add_css_class("inde-native-text");
  refresh();
  auto local = get_buffer();
  connections_.push_back(local->signal_insert().connect(
      [this](const Gtk::TextIter &at, const Glib::ustring &value, int) {
        if (reflecting_)
          return;
        if (cross_edit_ || begin_cross_edit())
          return;
        if (!ready()) {
          g_signal_stop_emission_by_name(get_buffer()->gobj(), "insert-text");
          schedule();
          return;
        }
        const int before = source_->get_char_count();
        auto position = source_->get_iter_at_offset(start() + at.get_offset());
        source_->insert(position, value);
        if (source_->get_char_count() !=
            before + static_cast<int>(value.size()))
          g_signal_stop_emission_by_name(get_buffer()->gobj(), "insert-text");
      },
      false));
  connections_.push_back(local->signal_erase().connect(
      [this](const Gtk::TextIter &a, const Gtk::TextIter &b) {
        if (reflecting_)
          return;
        if (cross_edit_ || begin_cross_edit())
          return;
        if (!ready()) {
          g_signal_stop_emission_by_name(get_buffer()->gobj(), "delete-range");
          schedule();
          return;
        }
        const int before = source_->get_char_count();
        auto first = source_->get_iter_at_offset(start() + a.get_offset());
        auto last = source_->get_iter_at_offset(start() + b.get_offset());
        source_->erase(first, last);
        if (source_->get_char_count() !=
            before - (b.get_offset() - a.get_offset()))
          g_signal_stop_emission_by_name(get_buffer()->gobj(), "delete-range");
      },
      false));
  connections_.push_back(local->signal_mark_set().connect(
      [this](const Gtk::TextIter &, const Glib::RefPtr<Gtk::TextMark> &) {
        if (!reflecting_ && !cross_edit_ && is_focus())
          select_source();
      }));
  connections_.push_back(
      source_->signal_changed().connect([this] { schedule(); }));
  connections_.push_back(source_->signal_mark_set().connect(
      [this](const Gtk::TextIter &, const Glib::RefPtr<Gtk::TextMark> &mark) {
        if (mark == source_->get_insert() ||
            mark == source_->get_selection_bound())
          schedule();
      }));
  connections_.push_back(source_->signal_apply_tag().connect(
      [this](const Glib::RefPtr<Gtk::TextTag> &, const Gtk::TextIter &,
             const Gtk::TextIter &) { schedule(); }));
  connections_.push_back(source_->signal_remove_tag().connect(
      [this](const Glib::RefPtr<Gtk::TextTag> &, const Gtk::TextIter &,
             const Gtk::TextIter &) { schedule(); }));
  auto focus = Gtk::EventControllerFocus::create();
  focus->signal_enter().connect([this] {
    if (!reflecting_ && !cross_edit_)
      select_source();
  });
  add_controller(focus);
  auto keys = Gtk::EventControllerKey::create();
  keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  keys->signal_key_pressed().connect(
      [this](guint key, guint, Gdk::ModifierType mods) {
        if (shortcut && shortcut(key, mods)) {
          refresh();
          return true;
        }
        const auto local = get_buffer();
        const int cursor =
            local->get_iter_at_mark(local->get_insert()).get_offset();
        const bool shift =
            (mods & Gdk::ModifierType::SHIFT_MASK) != Gdk::ModifierType{};
        if ((mods & (Gdk::ModifierType::CONTROL_MASK |
                     Gdk::ModifierType::ALT_MASK)) != Gdk::ModifierType{})
          return false;
        if (!local->get_has_selection() &&
            ((key == GDK_KEY_BackSpace && cursor == 0 && start() > 0) ||
             (key == GDK_KEY_Delete && cursor == local->get_char_count() &&
              end() < source_->get_char_count()))) {
          if (!ready())
            return true;
          const int at = key == GDK_KEY_BackSpace ? start() - 1 : end();
          auto first = source_->get_iter_at_offset(at),
               last = source_->get_iter_at_offset(at + 1);
          source_->erase(first, last);
          source_->place_cursor(source_->get_iter_at_offset(at));
          if (navigate)
            navigate(at, false);
          return true;
        }
        if (((key == GDK_KEY_Left || key == GDK_KEY_Up) && cursor == 0 &&
             start() > 0) ||
            ((key == GDK_KEY_Right || key == GDK_KEY_Down) &&
             cursor == local->get_char_count() &&
             end() < source_->get_char_count())) {
          const int at = key == GDK_KEY_Left || key == GDK_KEY_Up ? start() - 1
                                                                  : end() + 1;
          if (navigate)
            navigate(at, shift);
          return true;
        }
        return false;
      },
      false);
  add_controller(keys);
  preedit_handler_ = g_signal_connect(gobj(), "preedit-changed",
                   G_CALLBACK(+[](GtkTextView *, gchar *text, gpointer data) {
                     auto *view = static_cast<ObjectTextProjection *>(data);
                     view->preedit_active_ = text && *text;
                     if (view->preedit)
                       view->preedit(text);
                     if (!view->preedit_active_)
                       view->schedule();
                   }),
                   this);
}
ObjectTextProjection::~ObjectTextProjection() {
  if (preedit_handler_ && gobj() && g_signal_handler_is_connected(gobj(), preedit_handler_))
    g_signal_handler_disconnect(gobj(), preedit_handler_);
  pending_.disconnect();
  cross_pending_.disconnect();
  for (auto &connection : connections_)
    connection.disconnect();
  restore_primary_registration();
  source_->delete_mark(start_);
  source_->delete_mark(end_);
}
void ObjectTextProjection::on_realize() {
  Gtk::TextView::on_realize();
  // The complete selection belongs to Writing's source buffer. Independent
  // PRIMARY owners for paragraph buffers would clear each other's selections.
  get_buffer()->remove_selection_clipboard(get_primary_clipboard());
  primary_detached_ = true;
  refresh();
}
void ObjectTextProjection::restore_primary_registration() {
  if (!primary_detached_)
    return;
  const bool previous = std::exchange(reflecting_, true);
  get_buffer()->place_cursor(get_buffer()->begin());
  get_buffer()->add_selection_clipboard(get_primary_clipboard());
  primary_detached_ = false;
  reflecting_ = previous;
}
void ObjectTextProjection::on_unrealize() {
  restore_primary_registration();
  Gtk::TextView::on_unrealize();
}
void ObjectTextProjection::size_allocate_vfunc(int width, int height,
                                               int baseline) {
  const bool changed = width != layout_width_ || height != layout_height_;
  layout_width_ = width;
  layout_height_ = height;
  Gtk::TextView::size_allocate_vfunc(width, height, baseline);
  if (changed && layout_changed)
    layout_changed();
}
int ObjectTextProjection::start() const {
  return source_->get_iter_at_mark(start_).get_offset();
}
int ObjectTextProjection::end() const {
  return source_->get_iter_at_mark(end_).get_offset();
}
bool ObjectTextProjection::contains(int offset) const {
  if (offset < 0)
    offset = source_->get_iter_at_mark(source_->get_insert()).get_offset();
  return start() <= offset && offset <= end();
}
bool ObjectTextProjection::ready() const {
  return (!editable || editable()) &&
         source_->get_text(source_->get_iter_at_mark(start_),
                           source_->get_iter_at_mark(end_)) ==
             get_buffer()->get_text();
}
void ObjectTextProjection::schedule() {
  if (pending_.connected())
    return;
  pending_ = Glib::signal_idle().connect([this] {
    pending_.disconnect();
    refresh();
    return false;
  }, Glib::PRIORITY_HIGH_IDLE);
}
void ObjectTextProjection::select_source() {
  const auto local = get_buffer();
  const int a =
      start() + local->get_iter_at_mark(local->get_insert()).get_offset();
  const int b =
      start() +
      local->get_iter_at_mark(local->get_selection_bound()).get_offset();
  if (source_->get_iter_at_mark(source_->get_insert()).get_offset() != a ||
      source_->get_iter_at_mark(source_->get_selection_bound()).get_offset() != b)
    source_->select_range(source_->get_iter_at_offset(a),
                          source_->get_iter_at_offset(b));
}
void ObjectTextProjection::refresh() {
  // Resetting selection during preedit makes GtkTextView reset its IM
  // context. Keep provisional text owned by GTK until it commits/cancels.
  if (reflecting_ || cross_edit_ || preedit_active_)
    return;
  reflecting_ = true;
  const auto local = get_buffer();
  const int a = start(), b = end();
  const auto text = source_->get_text(source_->get_iter_at_offset(a),
                                      source_->get_iter_at_offset(b));
  if (text != local->get_text())
    local->set_text(text);
  local->remove_all_tags(local->begin(), local->end());
  // Reuse the actual formatting, semantic, search and visibility tags. Unicode
  // character offsets stay identical to Writing's source within this viewport.
  for (int i = a; i < b;) {
    auto at = source_->get_iter_at_offset(i);
    const auto tags = at.get_tags();
    auto next = at;
    if (!next.forward_to_tag_toggle({}))
      next = source_->end();
    const int stop = std::min(b, std::max(i + 1, next.get_offset()));
    for (const auto &tag : tags)
      local->apply_tag(tag, local->get_iter_at_offset(i - a),
                       local->get_iter_at_offset(stop - a));
    i = stop;
  }
  const int cursor =
      source_->get_iter_at_mark(source_->get_insert()).get_offset();
  const int bound =
      source_->get_iter_at_mark(source_->get_selection_bound()).get_offset();
  const int local_cursor = std::clamp(cursor - a, 0, b - a);
  const int local_bound = std::clamp(bound - a, 0, b - a);
  if (local->get_iter_at_mark(local->get_insert()).get_offset() != local_cursor ||
      local->get_iter_at_mark(local->get_selection_bound()).get_offset() != local_bound)
    local->select_range(local->get_iter_at_offset(local_cursor),
                        local->get_iter_at_offset(local_bound));
  reflecting_ = false;
}
bool ObjectTextProjection::begin_cross_edit() {
  Gtk::TextIter a, b;
  if (!replace_selection || !ready() || !source_->get_selection_bounds(a, b) ||
      (a.get_offset() >= start() && b.get_offset() <= end()))
    return false;
  cross_source_ = source_->get_text();
  cross_local_ = get_buffer()->get_text();
  cross_cursor_ = source_->get_iter_at_mark(source_->get_insert()).get_offset();
  cross_bound_ =
      source_->get_iter_at_mark(source_->get_selection_bound()).get_offset();
  cross_first_ = std::clamp(a.get_offset() - start(), 0, end() - start());
  cross_suffix_ = (end() - start()) -
                  std::clamp(b.get_offset() - start(), 0, end() - start());
  cross_edit_ = true;
  // GTK may deliver deletion and insertion separately for one accepted input.
  // Keep the authoritative selection intact until the complete dispatch ends.
  // Reconcile before GTK's layout/redraw idles, even during sustained painting.
  cross_pending_ = Glib::signal_idle().connect([this] {
    cross_pending_.disconnect();
    finish_cross_edit();
    return false;
  }, Glib::PRIORITY_HIGH_IDLE);
  return true;
}
void ObjectTextProjection::finish_cross_edit() {
  const auto local = get_buffer()->get_text();
  const int size = static_cast<int>(local.size());
  const bool unchanged =
      source_->get_text() == cross_source_ &&
      source_->get_iter_at_mark(source_->get_insert()).get_offset() ==
          cross_cursor_ &&
      source_->get_iter_at_mark(source_->get_selection_bound()).get_offset() ==
          cross_bound_;
  const bool valid =
      size >= cross_first_ + cross_suffix_ &&
      local.substr(0, static_cast<std::size_t>(cross_first_)) ==
          cross_local_.substr(0, static_cast<std::size_t>(cross_first_)) &&
      local.substr(static_cast<std::size_t>(size - cross_suffix_)) ==
          cross_local_.substr(cross_local_.size() -
                              static_cast<std::size_t>(cross_suffix_));
  cross_edit_ = false;
  if (unchanged && valid && (!editable || editable()))
    replace_selection(local
                          .substr(static_cast<std::size_t>(cross_first_),
                                  static_cast<std::size_t>(size - cross_first_ -
                                                           cross_suffix_))
                          .raw());
  refresh();
}
void ObjectTextProjection::focus_offset(int offset) {
  if (offset < 0)
    offset = source_->get_iter_at_mark(source_->get_insert()).get_offset();
  refresh();
  if (!contains(offset))
    return;
  reflecting_ = true;
  grab_focus();
  reflecting_ = false;
  // The paragraph has its complete natural height. Its containing sheet owns
  // scrolling; starting a second TextView animation here is unnecessary and
  // can outlive the viewport when a structural edit replaces it.
}
} // namespace inde::ui
