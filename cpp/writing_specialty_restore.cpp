#include "inde/ui/writing_workspace.hpp"
#include "object_native_ui.hpp"

namespace inde::ui {
void WritingWorkspace::mount_specialty_tools(bool shared) {
  if (shared) {
    for(auto *c=tools_panel_.get_first_child();c;c=c->get_next_sibling())
      g_object_set_data(G_OBJECT(c->gobj()),"inde-tools-original-visible",GINT_TO_POINTER(c->get_visible()?2:1));
    context_menu_scroll_.unset_child();
    tools_scroll_.unset_child();
    specialty_tools_.set_name("writing-specialty-tools");
    stats_title_.set_name("native-stats-heading");
    stats_words_.set_name("native-stats-words");
    specialty_tools_.append(contextual_toolbar_);
    specialty_tools_.append(tools_panel_);
    tools_panel_.set_size_request(360, -1);
    tools_panel_.set_margin(6);
    tools_button_.set_visible(false);
    // Preserve the existing controls and their application callbacks. Long
    // captions wrap in the inspector instead of widening the writing sheet.
    std::function<void(Gtk::Widget &)> wrap = [&](Gtk::Widget &w) {
      if (auto *b = dynamic_cast<Gtk::Button *>(&w))
        if (auto *l = dynamic_cast<Gtk::Label *>(b->get_child())) {
          l->set_wrap(true);
          l->set_max_width_chars(30);
        }
      for (auto *c = w.get_first_child(); c; c = c->get_next_sibling()) wrap(*c);
    };
    wrap(specialty_tools_);
  } else {
    specialty_tools_.remove(contextual_toolbar_);
    specialty_tools_.remove(tools_panel_);
    context_menu_scroll_.set_child(contextual_toolbar_);
    tools_scroll_.set_child(tools_panel_);
    tools_panel_.set_size_request(390, -1);
    tools_panel_.set_margin(16);
    tools_button_.set_visible(true);
    tools_visible_ = false;
    active_tool_section_.clear();
    for (auto *c = tools_panel_.get_first_child(); c; c = c->get_next_sibling()) {
      c->set_visible(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(c->gobj()),"inde-tools-original-visible"))==2);
      g_object_set_data(G_OBJECT(c->gobj()),"inde-tools-original-visible",nullptr);
    }
  }
}

void WritingWorkspace::refresh_specialty_tools() {
  if (!object_editor_) return;
  const auto &id = editor_specialization_;
  for(auto *c=tools_panel_.get_first_child();c;c=c->get_next_sibling())c->set_visible(false);
  auto section = [&](bool visible, std::initializer_list<Gtk::Widget *> widgets) {
    for (auto *w : widgets) w->set_visible(visible);
  };
  section(id == "text", {&stats_title_, &stats_grid_, &goal_separator_,
      &goal_title_, &goal_enabled_, &word_goal_, &goal_progress_});
  section(id == "linguistic", {&repeated_separator_, &repeated_title_,
      &repeated_words_, &linguistic_separator_, &linguistic_title_,
      &linguistic_summary_, &semantic_dictionary_button_, &linguistic_entities_button_,
      &paragraphs_separator_, &paragraphs_title_, &long_paragraphs_list_});
  section(id == "bindings", {&anchors_separator_, &anchors_title_, &anchor_actions_,
      &anchors_list_, &anchor_usages_button_, &text_references_button_,
      &references_separator_, &references_title_, &reference_actions_, &references_list_});
}
} // namespace inde::ui
