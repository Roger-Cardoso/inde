#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/planning_workspace.hpp"

#include <algorithm>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::refresh_axes() {
  const std::string previous = axis_combo_.get_active_id().raw();
  const auto shared_axis = service_.planning_context().fictional_axis_id;
  axes_ = service_.planning().time_axes();
  refreshing_axis_ = true;
  axis_combo_.remove_all();
  for (const auto &axis : axes_)
    axis_combo_.append(axis.id,
                       axis.name + (axis.is_default ? " — principal" : ""));
  if ((!shared_axis || !axis_combo_.set_active_id(*shared_axis)) &&
      (previous.empty() || !axis_combo_.set_active_id(previous))) {
    const auto preferred =
        std::find_if(axes_.begin(), axes_.end(),
                     [](const auto &axis) { return axis.is_default; });
    if (preferred != axes_.end())
      axis_combo_.set_active_id(preferred->id);
    else if (!axes_.empty())
      axis_combo_.set_active_id(axes_.front().id);
  }
  refreshing_axis_ = false;
  axis_edit_button_.set_sensitive(!axes_.empty());
  axis_actions_button_.set_sensitive(!axes_.empty());
  const std::string active = axis_combo_.get_active_id().raw();
  const auto selected_axis =
      std::find_if(axes_.begin(), axes_.end(),
                   [&](const auto &axis) { return axis.id == active; });
  axis_remove_button_.set_sensitive(selected_axis != axes_.end() &&
                                    !selected_axis->is_default);
  point_add_button_.set_sensitive(!active.empty());
  refresh_time_points();
}

void PlanningWorkspace::refresh_time_points() {
  refresh_axis_calendar();
  selected_time_point_id_.reset();
  time_points_.clear();
  while (auto *child = point_list_.get_first_child())
    point_list_.remove(*child);
  const std::string axis_id = axis_combo_.get_active_id().raw();
  if (!service_.current() || axis_id.empty()) {
    point_edit_button_.set_sensitive(false);
    point_remove_button_.set_sensitive(false);
    point_actions_button_.set_sensitive(false);
    return;
  }
  persistence::PlanningQuery query;
  query.limit = 500;
  time_points_ = service_.planning().time_points(axis_id, query);
  const auto dates = service_.worldbuilding().point_dates(time_points_);
  for (const auto &point : time_points_) {
    auto *button = Gtk::make_managed<Gtk::Button>(dates.at(point.id) + " — " +
                                                  point.label);
    button->set_halign(Gtk::Align::FILL);
    button->signal_clicked().connect(
        [this, id = point.id] { select_time_point(id); });
    attach_context_menu(*button,
                        {{"Usar como período mostrado",
                          [this, id = point.id] { select_time_point(id); },
                          "object-select-symbolic"},
                         {"Editar ponto",
                          [this, id = point.id] {
                            select_time_point(id);
                            edit_time_point();
                          },
                          "document-edit-symbolic"},
                         {"Remover ponto",
                          [this, id = point.id] {
                            select_time_point(id);
                            delete_time_point();
                          },
                          "user-trash-symbolic", true, true, true}});
    point_list_.append(*button);
  }
  const auto selected_axis =
      std::find_if(axes_.begin(), axes_.end(),
                   [&](const auto &axis) { return axis.id == axis_id; });
  axis_remove_button_.set_sensitive(selected_axis != axes_.end() &&
                                    !selected_axis->is_default);
  point_edit_button_.set_sensitive(false);
  point_remove_button_.set_sensitive(false);
  point_actions_button_.set_sensitive(false);
  refresh_time_overview();
}
void PlanningWorkspace::refresh_axis_calendar() {
  const auto axis = axis_combo_.get_active_id().raw();
  axis_calendar_button_.set_sensitive(service_.current() && !axis.empty());
  if (!service_.current() || axis.empty()) {
    axis_calendar_summary_.set_text("Selecione um linha do tempo.");
    return;
  }
  try {
    const auto b = service_.worldbuilding().binding(axis);
    if (!b) {
      axis_calendar_summary_.set_text(
          "Sem calendário · pontos em ordem relativa");
      return;
    }
    const auto h = service_.worldbuilding().adoptions(axis);
    auto text = "Calendário de origem: " +
                service_.worldbuilding().calendar(b->calendar_id).name;
    if (!h.empty())
      text += "\nÚltima adoção: " +
              service_.worldbuilding().calendar(h.back().calendar_id).name +
              " · " + std::to_string(h.size()) + " mudanças";
    axis_calendar_summary_.set_text(text);
  } catch (const std::exception &e) {
    axis_calendar_summary_.set_text(author::diagnostic(e.what()));
  }
}

void PlanningWorkspace::refresh_time_overview() {
  while (auto *child = time_events_list_.get_first_child())
    time_events_list_.remove(*child);
  while (auto *child = time_presences_list_.get_first_child())
    time_presences_list_.remove(*child);
  if (!service_.current()) {
    time_events_title_.set_text("Acontecimentos no recorte");
    time_presences_title_.set_text("Presenças no recorte");
    return;
  }
  try {
    const auto view =
        service_.planning().temporal_view(service_.planning_context());
    const auto &events = view.timeline.events;
    const auto &presences = view.timeline.presences;
    std::vector<project::FictionalTimePoint> event_points;
    for (const auto &e : events)
      event_points.push_back(project::FictionalTimePoint{
          e.point_id, view.timeline.axis.id, e.ordinal, {}, {}, {}, {}});
    const auto dates = service_.worldbuilding().point_dates(event_points);
    time_events_title_.set_text("Acontecimentos no recorte (" +
                                std::to_string(view.matching.events) + ")");
    for (const auto &event : events) {
      auto *button = Gtk::make_managed<Gtk::Button>(event.entity_name + " — " +
                                                    dates.at(event.point_id));
      button->set_halign(Gtk::Align::FILL);
      button->set_tooltip_text(event.description.empty()
                                   ? event.inclusion_reason
                                   : event.description);
      button->signal_clicked().connect(
          [this, id = event.entity_id] { reveal_entity(id); });
      time_events_list_.append(*button);
    }
    if (events.empty()) {
      auto *empty = Gtk::make_managed<Gtk::Label>(
          "Nenhum Acontecimento corresponde ao recorte compartilhado.");
      empty->set_xalign(0.0F);
      empty->add_css_class("dim-label");
      time_events_list_.append(*empty);
    }

    time_presences_title_.set_text("Presenças no recorte (" +
                                   std::to_string(view.matching.presences) +
                                   ")");
    for (const auto &presence : presences) {
      auto *button =
          Gtk::make_managed<Gtk::Button>("Sujeito: " + presence.entity_name +
                                         " • Local: " + presence.location_name);
      button->set_halign(Gtk::Align::FILL);
      button->set_tooltip_text(presence.description.empty()
                                   ? presence.inclusion_reason
                                   : presence.description);
      button->signal_clicked().connect(
          [this, id = presence.entity_id] { reveal_entity(id); });
      time_presences_list_.append(*button);
    }
    if (presences.empty()) {
      auto *empty = Gtk::make_managed<Gtk::Label>(
          "Nenhuma presença corresponde ao recorte compartilhado.");
      empty->set_xalign(0.0F);
      empty->add_css_class("dim-label");
      time_presences_list_.append(*empty);
    }
  } catch (const std::exception &error) {
    time_events_title_.set_text("Acontecimentos no recorte");
    time_presences_title_.set_text("Presenças no recorte");
    auto *failure = Gtk::make_managed<Gtk::Label>(
        "Não foi possível projetar o recorte: " + std::string(author::diagnostic(error.what())));
    failure->set_xalign(0.0F);
    failure->add_css_class("dim-label");
    time_events_list_.append(*failure);
  }
}

void PlanningWorkspace::reveal_time_point(const std::string &id) {
  if (!service_.current())
    return;
  try {
    const auto point = service_.planning().time_point(id);
    if (!point)
      throw std::runtime_error("O marco na linha do tempo de origem não existe mais");
    remember_navigation();
    auto context = service_.planning_context();
    context.fictional_axis_id = point->axis_id;
    context.fictional_time_point_id = point->id;
    context.fictional_window_start_time_point_id.reset();
    context.fictional_window_end_time_point_id.reset();
    service_.set_planning_context(std::move(context));
    refresh();
    axis_combo_.set_active_id(point->axis_id);
    refresh_time_points();
    show_time_page();
    select_time_point(point->id);
    signal_status_message_.emit("Marco na linha do tempo aberto no Planejamento");
  } catch (const std::exception &error) {
    show_error("Não foi possível abrir a origem", error);
  }
}
void PlanningWorkspace::reveal_removal_use(const project::RemovalUse &use) {
  if (use.location_table == "entities") {
    if (!service_.narrative().entity(use.location_id))
      throw std::runtime_error(
          "De removida; atualize a prévia.");
    reveal_entity(use.location_id);
    if (use.table == "entity_work_scopes")
      select_work_scope(use.record_id);
    else if (use.table == "editorial_entity_references")
      select_editorial_reference(use.record_id);
    else if (use.table == "entity_presences")
      select_presence(use.record_id);
    else if (use.table == "event_participations")
      select_participant(use.record_id);
    return;
  }
  if (use.location_table == "fictional_time_points") {
    if (!service_.planning().time_point(use.location_id))
      throw std::runtime_error("Ponto de origem removido; atualize a prévia.");
    reveal_time_point(use.location_id);
    return;
  }
  if (use.location_table == "fictional_time_axes") {
    auto context = service_.planning_context();
    context.fictional_axis_id = use.location_id;
    context.fictional_time_point_id.reset();
    context.fictional_window_start_time_point_id.reset();
    context.fictional_window_end_time_point_id.reset();
    service_.set_planning_context(context);
    refresh_axes();
    show_calendar(use.location_id);
    return;
  }
  if (use.location_table == "fictional_calendars") {
    show_temporal_library();
    temporal_library_.edit_calendar(
        service_.worldbuilding().calendar(use.location_id));
    return;
  }
  if (use.location_table == "temporal_references") {
    const auto ref =
        service_.worldbuilding().temporal_reference(use.location_id);
    show_temporal_library();
    if (use.table == "temporal_attachments")
      temporal_library_.date_links(ref);
    else
      temporal_library_.open_reference(ref.axis_id, ref.id);
    return;
  }
  if (use.location_table == "relations") {
    show_relations_page();
    select_relation(use.location_id);
    edit_relation();
    return;
  }
  if (use.location_table.starts_with("narrative_") ||
      use.location_table == "structure_models") {
    show_narrative_page();
    narrative_structure_workspace_.reveal_removal_use(use);
    return;
  }
  throw std::runtime_error("Este uso tem orientação textual, mas ainda não tem "
                           "atalho de abertura nesta tela: " +
                           use.path);
}

} // namespace inde::ui
