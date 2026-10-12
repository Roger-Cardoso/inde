#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::build_time_page() {
  time_page_.set_vexpand(true);
  time_page_.set_hexpand(true);
  time_page_title_.add_css_class("title-1");
  time_page_title_.add_css_class("page-title");
  time_page_title_.set_hexpand(true);
  time_page_title_.set_halign(Gtk::Align::START);
  time_header_.append(time_page_title_);
  time_page_.append(time_header_);
  time_page_description_.set_wrap(true);
  time_page_description_.set_xalign(0.0F);
  time_page_description_.add_css_class("dim-label");
  time_page_description_.set_text(
      "Organize linhas do tempo, pontos e fatos. Configure o calendário de cada linha do tempo aqui; "
      "a biblioteca reúne sistemas, meses, relógios e datas. A Timeline permanece em Gráficos.");
  time_page_.append(time_page_description_);
  auto *library = Gtk::make_managed<Gtk::Button>("Sistemas e datas…");
  library->set_halign(Gtk::Align::START);
  library->signal_clicked().connect([this] { show_temporal_library(); });
  time_page_.append(*library);
  inspector_scroll_.set_child(inspector_panel_);
  inspector_scroll_.set_policy(Gtk::PolicyType::NEVER,
                               Gtk::PolicyType::AUTOMATIC);
  inspector_scroll_.set_vexpand(true);
  inspector_scroll_.set_hexpand(true);
  time_facts_scroll_.set_child(time_facts_panel_);
  time_facts_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                Gtk::PolicyType::AUTOMATIC);
  time_facts_scroll_.set_vexpand(true);
  time_facts_scroll_.set_hexpand(true);
  time_columns_.append(inspector_scroll_);
  time_columns_.append(time_facts_scroll_);
  time_page_.append(time_columns_);
  inspector_panel_.set_margin(12);
  inspector_panel_.add_css_class("content-card");
  time_facts_panel_.set_margin(12);
  time_facts_panel_.add_css_class("content-card");
  time_title_.add_css_class("title-2");
  time_title_.set_halign(Gtk::Align::START);
  axis_toolbar_.append(axis_add_button_);
  axis_toolbar_.append(axis_actions_button_);
  axis_remove_button_.add_css_class("destructive-action");
  points_title_.add_css_class("heading");
  points_title_.set_halign(Gtk::Align::START);
  point_toolbar_.append(point_add_button_);
  point_toolbar_.append(point_actions_button_);
  point_remove_button_.add_css_class("destructive-action");
  point_list_.add_css_class("boxed-list");
  inspector_panel_.append(time_separator_);
  inspector_panel_.append(time_title_);
  inspector_panel_.append(axis_combo_);
  inspector_panel_.append(axis_toolbar_);
  axis_calendar_summary_.set_wrap(true);
  axis_calendar_summary_.set_xalign(0);
  axis_calendar_summary_.add_css_class("secondary-text");
  inspector_panel_.append(axis_calendar_summary_);
  axis_calendar_button_.set_halign(Gtk::Align::START);
  axis_calendar_button_.signal_clicked().connect([this] { show_calendar(); });
  inspector_panel_.append(axis_calendar_button_);
  inspector_panel_.append(points_title_);
  inspector_panel_.append(point_toolbar_);
  inspector_panel_.append(point_list_);
  time_events_title_.add_css_class("heading");
  time_events_title_.set_halign(Gtk::Align::START);
  time_events_list_.add_css_class("boxed-list");
  time_presences_title_.add_css_class("heading");
  time_presences_title_.set_halign(Gtk::Align::START);
  time_presences_list_.add_css_class("boxed-list");
  time_facts_panel_.append(time_events_title_);
  time_facts_panel_.append(time_events_list_);
  time_facts_panel_.append(time_presences_title_);
  time_facts_panel_.append(time_presences_list_);
}

} // namespace inde::ui
