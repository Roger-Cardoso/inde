#include "object_text_projection.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void need(bool ok, const char *why) {
  if (!ok)
    throw std::runtime_error(why);
}
void drain() {
  const auto context = Glib::MainContext::get_default();
  while (context->pending())
    context->iteration(false);
}
} // namespace
int main() try {
  const auto app =
      Gtk::Application::create("io.github.inde.obj01.projection.tests",
                               Gio::Application::Flags::NON_UNIQUE);
  app->register_application();
  auto source = Gtk::TextBuffer::create();
  source->set_text("Olá\nMundo");
  inde::ui::ObjectTextProjection first(source, 0, 3), second(source, 4, 9);
  first.get_buffer()->insert(first.get_buffer()->get_iter_at_offset(1), "ç");
  need(source->get_text() == "Oçlá\nMundo",
       "Unicode input did not reach source");
  drain();
  need(first.get_buffer()->get_text() == "Oçlá", "First viewport lost input");
  need(second.get_buffer()->get_text() == "Mundo",
       "Neighbor viewport moved incorrectly");
  second.get_buffer()->insert(second.get_buffer()->end(), "!");
  need(source->get_text() == "Oçlá\nMundo!",
       "Shifted neighbor edited wrong source range");
  drain();
  auto guard = source->signal_insert().connect(
      [&](const Gtk::TextIter &, const Glib::ustring &text, int) {
        if (text == "?")
          g_signal_stop_emission_by_name(source->gobj(), "insert-text");
      },
      false);
  first.get_buffer()->insert(first.get_buffer()->begin(), "?");
  need(first.get_buffer()->get_text() == "Oçlá" &&
           source->get_text() == "Oçlá\nMundo!",
       "Rejected input diverged from authoritative source");
  source->insert(source->begin(), "Já ");
  first.get_buffer()->insert(first.get_buffer()->begin(), "STALE");
  need(source->get_text() == "Já Oçlá\nMundo!",
       "Stale viewport overwrote newer text");
  drain();
  need(first.get_buffer()->get_text() == "Já Oçlá",
       "External edit did not refresh viewport");
  auto bold = source->create_tag("bold");
  bold->property_weight() = Pango::Weight::BOLD;
  source->apply_tag(bold, source->begin(), source->get_iter_at_offset(2));
  drain();
  need(first.get_buffer()->begin().has_tag(bold),
       "Original formatting tag not shared");
  source->remove_tag(bold, source->begin(), source->end());
  drain();
  need(!first.get_buffer()->begin().has_tag(bold),
       "Formatting removal left a stale tag");
  auto hidden = source->create_tag("hidden");
  hidden->property_invisible() = true;
  source->apply_tag(hidden, source->begin(), source->get_iter_at_offset(3));
  drain();
  need(first.get_buffer()->get_text(false) == "Oçlá",
       "Visibility changed source offset mapping");
  need(source->get_text() == "Já Oçlá\nMundo!", "Visibility deleted source");
  first.editable = [] { return false; };
  first.get_buffer()->erase(first.get_buffer()->begin(),
                            first.get_buffer()->end());
  need(source->get_text() == "Já Oçlá\nMundo!",
       "Busy state allowed destructive input");
  first.editable = [] { return true; };
  second.get_buffer()->erase(second.get_buffer()->get_iter_at_offset(5),
                             second.get_buffer()->end());
  need(source->get_text() == "Já Oçlá\nMundo",
       "Viewport erase used wrong character offsets");
  guard.disconnect();
  drain();
  {
    auto selected = Gtk::TextBuffer::create();
    selected->set_text("abc\ndef");
    inde::ui::ObjectTextProjection view(selected, 0, 3);
    int replacements = 0;
    view.replace_selection = [&](const std::string &value) {
      ++replacements;
      Gtk::TextIter a, b;
      need(selected->get_selection_bounds(a, b),
           "Lost cross-paragraph selection");
      const auto at = selected->erase(a, b);
      selected->insert(at, value);
      return true;
    };
    selected->select_range(selected->end(), selected->begin());
    view.prepare_context(0, 0);
    need(selected->get_iter_at_mark(selected->get_insert()).get_offset() == 7 &&
             selected->get_iter_at_mark(selected->get_selection_bound()).get_offset() == 0,
         "Context menu truncated the cross-paragraph selection");
    need(selected->get_text() == "abc\ndef", "Context menu changed the source text");
    view.refresh();
    view.get_buffer()->erase(view.get_buffer()->begin(),
                             view.get_buffer()->end());
    view.get_buffer()->insert(view.get_buffer()->begin(), "a");
    need(view.pending_input(), "Cross-paragraph input was not protected");
    need(selected->get_text() == "abc\ndef",
         "Partial deletion reached source before accepted input completed");
    drain();
    need(selected->get_text() == "a" && replacements == 1 &&
             !view.pending_input(),
         "Cross-paragraph replacement was not one complete edit");
  }
  std::cout << "shared text projection: Unicode, guarded edits, stale refusal, "
               "formatting, visibility, busy and neighbor offsets OK\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
