#include "inde/ui/graphs_workspace.hpp"
#include "inde/ui/accessibility.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace inde::ui {
namespace {

void configure_label(Gtk::Label &label) {
  label.set_wrap(true);
  label.set_xalign(0.0F);
  label.set_halign(Gtk::Align::FILL);
}

void clear_list(Gtk::ListBox &list) {
  while (auto *child = list.get_first_child())
    list.remove(*child);
}

void append_row(Gtk::ListBox &list, const Glib::ustring &text,
                std::function<void()> action = {}) {
  auto *button = Gtk::make_managed<Gtk::Button>(text);
  button->set_halign(Gtk::Align::FILL);
  button->set_hexpand(true);
  if (auto *label = dynamic_cast<Gtk::Label *>(button->get_child())) {
    label->set_wrap(true);
    label->set_xalign(0.0F);
  }
  button->set_can_focus(static_cast<bool>(action));
  if (action)
    button->signal_clicked().connect(std::move(action));
  list.append(*button);
}

std::string point_caption(const project::FictionalTimePoint &point) {
  return std::to_string(point.ordinal) + " — " + point.label;
}

} // namespace

GraphsWorkspace::GraphsWorkspace(application::ProjectService &service)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8), service_(service) {
  build_ui();
}

void GraphsWorkspace::build_ui() {
  set_hexpand(true);
  set_vexpand(true);
  set_margin(16);
  add_css_class("workspace-page");
  title_.set_text("Visualizações temporais");
  title_.add_css_class("title-1");
  title_.add_css_class("page-title");
  title_.set_hexpand(true);
  title_.set_halign(Gtk::Align::START);
  header_.append(title_);
  header_.append(axis_label_);
  axis_combo_.set_size_request(230, -1);
  set_accessible_label(axis_combo_, "Eixo ficcional");
  header_.append(axis_combo_);
  header_.append(refresh_button_);
  header_.add_css_class("timeline-toolbar");
  append(header_);

  work_combo_.set_size_request(190, -1);
  window_combo_.set_size_request(145, -1);
  start_point_combo_.set_size_request(180, -1);
  start_point_combo_.set_tooltip_text(
      "Ponto único ou início do período ficcional");
  set_accessible_label(start_point_combo_, "Início do período ficcional");
  end_point_combo_.set_size_request(180, -1);
  end_point_combo_.set_tooltip_text("Fim do período ficcional");
  set_accessible_label(end_point_combo_, "Fim do período ficcional");
  lens_combo_.set_size_request(130, -1);
  set_accessible_label(work_combo_, "Obra no contexto");
  set_accessible_label(window_combo_, "Janela temporal");
  set_accessible_label(lens_combo_, "Lente de visualização");
  context_controls_.append(work_label_);
  context_controls_.append(work_combo_);
  context_controls_.append(window_label_);
  context_controls_.append(window_combo_);
  context_controls_.append(start_point_combo_);
  context_controls_.append(end_point_combo_);
  context_controls_.append(clear_window_button_);
  context_controls_.append(clear_context_button_);
  context_controls_.append(lens_label_);
  context_controls_.append(lens_combo_);
  context_controls_scroll_.set_child(context_controls_);
  context_controls_scroll_.set_policy(Gtk::PolicyType::AUTOMATIC,
                                      Gtk::PolicyType::NEVER);
  context_controls_scroll_.set_propagate_natural_width(false);
  context_controls_scroll_.set_min_content_height(50);
  context_controls_scroll_.add_css_class("timeline-toolbar");
  append(context_controls_scroll_);

  search_.set_placeholder_text("Filtrar entidade, local ou descrição");
  set_accessible_label(search_, "Filtrar linha do tempo");
  set_accessible_description(
      search_, "Filtra entidade, local ou descrição na visualização temporal.");
  search_.set_hexpand(true);
  events_filter_.set_active(true);
  presences_filter_.set_active(true);
  events_filter_.add_css_class("filter-toggle");
  presences_filter_.add_css_class("filter-toggle");
  events_filter_.add_css_class("filter-enabled");
  presences_filter_.add_css_class("filter-enabled");
  controls_.append(search_);
  controls_.append(events_filter_);
  controls_.append(presences_filter_);
  controls_.append(zoom_title_);
  controls_.append(zoom_out_button_);
  zoom_label_.set_width_chars(5);
  zoom_label_.set_xalign(0.5F);
  controls_.append(zoom_label_);
  controls_.append(zoom_in_button_);
  controls_.append(zoom_reset_button_);
  paint_metric_.set_width_chars(16);
  paint_metric_.set_xalign(1.0F);
  controls_.append(paint_metric_);
  controls_scroll_.set_child(controls_);
  controls_scroll_.set_policy(Gtk::PolicyType::AUTOMATIC,
                              Gtk::PolicyType::NEVER);
  controls_scroll_.set_propagate_natural_width(false);
  controls_scroll_.set_min_content_height(50);
  controls_scroll_.add_css_class("timeline-toolbar");
  append(controls_scroll_);

  split_.set_hexpand(true);
  split_.set_vexpand(true);
  split_.set_wide_handle(false);
  split_.set_resize_start_child(true);
  split_.set_shrink_start_child(true);
  split_.set_resize_end_child(false);
  split_.set_shrink_end_child(false);
  split_.set_start_child(lens_stack_);
  split_.set_end_child(inspector_scroll_);
  lens_stack_.set_hexpand(true);
  lens_stack_.set_vexpand(true);
  lens_stack_.set_transition_type(Gtk::StackTransitionType::CROSSFADE);
  timeline_scroll_.set_child(timeline_);
  timeline_scroll_.set_overlay_scrolling(false);
  timeline_scroll_.set_policy(Gtk::PolicyType::AUTOMATIC,
                              Gtk::PolicyType::AUTOMATIC);
  lens_stack_.add(timeline_scroll_, "timeline");

  situation_scroll_.set_child(situation_);
  situation_scroll_.set_policy(Gtk::PolicyType::NEVER,
                               Gtk::PolicyType::AUTOMATIC);
  situation_.set_margin(20);
  situation_.add_css_class("content-card");
  situation_title_.add_css_class("title-1");
  situation_title_.set_halign(Gtk::Align::START);
  situation_context_.add_css_class("dim-label");
  configure_label(situation_context_);
  configure_label(situation_state_);
  situation_list_.add_css_class("boxed-list");
  situation_.append(situation_title_);
  situation_.append(situation_context_);
  situation_.append(situation_state_);
  situation_.append(situation_list_);
  lens_stack_.add(situation_scroll_, "situation");

  agenda_scroll_.set_child(agenda_);
  agenda_scroll_.set_policy(Gtk::PolicyType::NEVER,
                            Gtk::PolicyType::AUTOMATIC);
  agenda_.set_margin(20);
  agenda_.add_css_class("content-card");
  agenda_title_.add_css_class("title-1");
  agenda_title_.set_halign(Gtk::Align::START);
  agenda_context_.add_css_class("dim-label");
  configure_label(agenda_context_);
  agenda_list_.add_css_class("boxed-list");
  agenda_.append(agenda_title_);
  agenda_.append(agenda_context_);
  agenda_.append(agenda_list_);
  lens_stack_.add(agenda_scroll_, "agenda");

  inspector_scroll_.set_child(inspector_);
  inspector_scroll_.set_policy(Gtk::PolicyType::NEVER,
                               Gtk::PolicyType::AUTOMATIC);
  inspector_scroll_.set_size_request(320, -1);
  inspector_scroll_.add_css_class("inspector-surface");
  inspector_scroll_.set_visible(false);
  inspector_.set_margin(16);
  inspector_title_.add_css_class("title-2");
  inspector_title_.set_halign(Gtk::Align::START);
  for (auto *label : {&inspector_kind_, &inspector_name_, &inspector_position_,
                      &inspector_description_, &inspector_reason_, &state_})
    configure_label(*label);
  inspector_kind_.add_css_class("dim-label");
  inspector_reason_.add_css_class("dim-label");
  open_source_button_.set_sensitive(false);
  inspector_.append(inspector_title_);
  inspector_.append(inspector_kind_);
  inspector_.append(inspector_name_);
  inspector_.append(inspector_position_);
  inspector_.append(inspector_description_);
  inspector_.append(inspector_reason_);
  inspector_.append(open_source_button_);
  inspector_.append(state_);
  append(split_);

  axis_combo_.signal_changed().connect([this] {
    if (!refreshing_axis_) {
      store_shared_context_from_controls();
      refresh_axis_points();
      refresh_snapshot();
    }
  });
  work_combo_.signal_changed().connect([this] {
    if (!refreshing_context_) {
      store_shared_context_from_controls();
      refresh_snapshot();
    }
  });
  window_combo_.signal_changed().connect([this] {
    if (!refreshing_context_) {
      update_window_controls();
      store_shared_context_from_controls();
      refresh_snapshot();
    }
  });
  start_point_combo_.signal_changed().connect([this] {
    if (!refreshing_context_) {
      store_shared_context_from_controls();
      refresh_snapshot();
    }
  });
  end_point_combo_.signal_changed().connect([this] {
    if (!refreshing_context_) {
      store_shared_context_from_controls();
      refresh_snapshot();
    }
  });
  lens_combo_.signal_changed().connect([this] {
    if (!refreshing_context_)
      update_lens();
  });
  refresh_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &GraphsWorkspace::refresh));
  clear_window_button_.signal_clicked().connect([this] {
    refreshing_context_ = true;
    window_combo_.set_active_id("all");
    refreshing_context_ = false;
    update_window_controls();
    store_shared_context_from_controls();
    refresh_snapshot();
  });
  clear_context_button_.signal_clicked().connect([this] {
    refreshing_context_ = true;
    work_combo_.set_active_id("all");
    window_combo_.set_active_id("all");
    search_.set_text("");
    events_filter_.set_active(true);
    presences_filter_.set_active(true);
    refreshing_context_ = false;
    update_window_controls();
    store_shared_context_from_controls();
    refresh_snapshot();
    signal_status_message_.emit("Recorte compartilhado e filtros locais limpos");
  });
  timeline_.signal_item_selected().connect(
      sigc::mem_fun(*this, &GraphsWorkspace::show_selection));
  timeline_.signal_painted().connect(
      sigc::mem_fun(*this, &GraphsWorkspace::on_timeline_painted));
  search_.signal_changed().connect(
      sigc::mem_fun(*this, &GraphsWorkspace::apply_filters));
  events_filter_.signal_toggled().connect([this] {
    events_filter_.remove_css_class(events_filter_.get_active()
                                        ? "filter-disabled"
                                        : "filter-enabled");
    events_filter_.add_css_class(events_filter_.get_active()
                                     ? "filter-enabled"
                                     : "filter-disabled");
    events_filter_.set_label(events_filter_.get_active()
                                 ? "Acontecimentos visíveis"
                                 : "Acontecimentos ocultos");
    apply_filters();
  });
  presences_filter_.signal_toggled().connect([this] {
    presences_filter_.remove_css_class(presences_filter_.get_active()
                                           ? "filter-disabled"
                                           : "filter-enabled");
    presences_filter_.add_css_class(presences_filter_.get_active()
                                        ? "filter-enabled"
                                        : "filter-disabled");
    presences_filter_.set_label(presences_filter_.get_active()
                                    ? "Presenças visíveis"
                                    : "Presenças ocultas");
    apply_filters();
  });
  zoom_out_button_.signal_clicked().connect(
      [this] { update_zoom(timeline_.zoom() / 1.25); });
  zoom_in_button_.signal_clicked().connect(
      [this] { update_zoom(timeline_.zoom() * 1.25); });
  zoom_reset_button_.signal_clicked().connect([this] { update_zoom(1.0); });
  open_source_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &GraphsWorkspace::open_selection_source));
  reset();
}

void GraphsWorkspace::refresh() {
  if (!service_.current()) {
    reset();
    return;
  }
  try {
    const std::string previous_axis = axis_combo_.get_active_id().raw();
    const auto shared_axis = service_.planning_context().fictional_axis_id;
    axes_ = service_.planning().time_axes();
    refreshing_axis_ = true;
    axis_combo_.remove_all();
    for (const auto &axis : axes_)
      axis_combo_.append(axis.id,
                         axis.name + (axis.is_default ? " (padrão)" : ""));
    const bool restored =
        (shared_axis && axis_combo_.set_active_id(*shared_axis)) ||
        (!shared_axis && !previous_axis.empty() &&
         axis_combo_.set_active_id(previous_axis));
    if (!restored) {
      const auto preferred =
          std::find_if(axes_.begin(), axes_.end(),
                       [](const auto &axis) { return axis.is_default; });
      if (preferred != axes_.end())
        axis_combo_.set_active_id(preferred->id);
      else if (!axes_.empty())
        axis_combo_.set_active(0);
    }
    refreshing_axis_ = false;
    axis_combo_.set_sensitive(!axes_.empty());
    refresh_context_controls();
    refresh_axis_points();
    refresh_snapshot();
  } catch (const std::exception &error) {
    refreshing_axis_ = false;
    refreshing_context_ = false;
    show_failure(error);
  }
}

void GraphsWorkspace::refresh_context_controls() {
  const auto &context = service_.planning_context();
  const std::string previous_lens = lens_combo_.get_active_id().raw();
  refreshing_context_ = true;
  works_ = service_.catalog().works;
  work_combo_.remove_all();
  work_combo_.append("all", "Todo o Projeto");
  for (const auto &work : works_)
    work_combo_.append(work.id, work.title);
  if (!context.work_id || !work_combo_.set_active_id(*context.work_id))
    work_combo_.set_active_id("all");

  window_combo_.remove_all();
  window_combo_.append("all", "Eixo inteiro");
  window_combo_.append("point", "Um ponto");
  window_combo_.append("range", "Período fechado");
  if (context.fictional_time_point_id)
    window_combo_.set_active_id("point");
  else if (context.fictional_window_start_time_point_id)
    window_combo_.set_active_id("range");
  else
    window_combo_.set_active_id("all");

  lens_combo_.remove_all();
  lens_combo_.append("timeline", "Timeline");
  lens_combo_.append("situation", "Situação");
  lens_combo_.append("agenda", "Agenda");
  if (previous_lens.empty() || !lens_combo_.set_active_id(previous_lens))
    lens_combo_.set_active_id("timeline");
  refreshing_context_ = false;
  update_window_controls();
}

void GraphsWorkspace::refresh_axis_points() {
  const auto &context = service_.planning_context();
  refreshing_context_ = true;
  axis_points_.clear();
  start_point_combo_.remove_all();
  end_point_combo_.remove_all();
  const std::string axis_id = axis_combo_.get_active_id().raw();
  if (service_.current() && !axis_id.empty()) {
    persistence::PlanningQuery query;
    query.limit = query_limit_;
    axis_points_ = service_.planning().time_points(axis_id, query);
    for (const auto &point : axis_points_) {
      start_point_combo_.append(point.id, point_caption(point));
      end_point_combo_.append(point.id, point_caption(point));
    }
    const auto preferred_start = context.fictional_time_point_id
                                     ? context.fictional_time_point_id
                                     : context.fictional_window_start_time_point_id;
    if (!preferred_start ||
        !start_point_combo_.set_active_id(*preferred_start)) {
      if (!axis_points_.empty())
        start_point_combo_.set_active(0);
    }
    if (!context.fictional_window_end_time_point_id ||
        !end_point_combo_.set_active_id(
            *context.fictional_window_end_time_point_id)) {
      if (!axis_points_.empty())
        end_point_combo_.set_active(
            static_cast<int>(axis_points_.size() - 1));
    }
  }
  refreshing_context_ = false;
  update_window_controls();
}

void GraphsWorkspace::update_window_controls() {
  const std::string mode = window_combo_.get_active_id().raw();
  const bool point = mode == "point";
  const bool range = mode == "range";
  start_point_combo_.set_visible(point || range);
  end_point_combo_.set_visible(range);
  start_point_combo_.set_sensitive(!axis_points_.empty());
  end_point_combo_.set_sensitive(!axis_points_.empty());
  clear_window_button_.set_sensitive(point || range);
}

void GraphsWorkspace::store_shared_context_from_controls() {
  if (!service_.current())
    return;
  auto context = service_.planning_context();
  const std::string axis_id = axis_combo_.get_active_id().raw();
  if (axis_id.empty())
    context.fictional_axis_id.reset();
  else
    context.fictional_axis_id = axis_id;
  const std::string work_id = work_combo_.get_active_id().raw();
  if (work_id.empty() || work_id == "all") {
    context.work_id.reset();
    // A apresentação editorial pertence a uma Obra. Se Gráficos volta ao
    // projeto inteiro, ela não pode continuar impondo uma Obra invisível.
    context.editorial_node_id.reset();
  } else
    context.work_id = work_id;
  context.fictional_time_point_id.reset();
  context.fictional_window_start_time_point_id.reset();
  context.fictional_window_end_time_point_id.reset();
  const std::string mode = window_combo_.get_active_id().raw();
  const std::string start = start_point_combo_.get_active_id().raw();
  const std::string end = end_point_combo_.get_active_id().raw();
  if (mode == "point" && !start.empty())
    context.fictional_time_point_id = start;
  else if (mode == "range" && !start.empty() && !end.empty()) {
    context.fictional_window_start_time_point_id = start;
    context.fictional_window_end_time_point_id = end;
  }
  context.offset = 0;
  service_.set_planning_context(std::move(context));
}

void GraphsWorkspace::refresh_snapshot() {
  if (!service_.current())
    return;
  const std::string axis_id = axis_combo_.get_active_id().raw();
  if (axis_id.empty()) {
    view_snapshot_.reset();
    snapshot_.reset();
    timeline_.clear();
    state_.set_text("Crie um eixo e pontos temporais no Planejamento para "
                    "iniciar esta projeção.");
    update_lens();
    signal_status_message_.emit("Linha do tempo sem eixo temporal");
    return;
  }
  try {
    store_shared_context_from_controls();
    auto context =
        service_.planning().temporal_view_context(service_.planning_context());
    context.limit = query_limit_;
    view_snapshot_ = service_.planning().temporal_view(context);
    apply_filters();
  } catch (const std::exception &error) {
    show_failure(error);
  }
}

void GraphsWorkspace::apply_filters() {
  if (refreshing_context_ || !view_snapshot_)
    return;
  application::TimelineFilter filter;
  filter.search = search_.get_text().raw();
  filter.show_events = events_filter_.get_active();
  filter.show_presences = presences_filter_.get_active();
  snapshot_ =
      application::filter_timeline_snapshot(view_snapshot_->timeline, filter);
  timeline_.set_snapshot(*snapshot_);
  timeline_.set_zoom(timeline_.zoom(), timeline_scroll_.get_width());
  reset_inspector();
  update_lens();

  if (snapshot_->points.empty())
    state_.set_text("Este recorte não possui pontos temporais.");
  else if (view_snapshot_->matching.events == 0 &&
           view_snapshot_->matching.presences == 0)
    state_.set_text("Nenhum fato temporal corresponde ao recorte atual.");
  else if (snapshot_->events.empty() && snapshot_->presences.empty())
    state_.set_text("Nenhum fato temporal corresponde aos filtros locais.");
  else if (view_snapshot_->truncation.any())
    state_.set_text(
        "Exibição possivelmente parcial: um limite seguro foi alcançado.");
  else
    state_.set_text("");

  signal_status_message_.emit(
      "Recorte temporal: " + std::to_string(snapshot_->points.size()) +
      " pontos, " + std::to_string(snapshot_->events.size()) + " de " +
      std::to_string(view_snapshot_->matching.events) + " acontecimentos e " +
      std::to_string(snapshot_->presences.size()) + " de " +
      std::to_string(view_snapshot_->matching.presences) + " presenças" +
      " (acontecimentos " +
      (filter.show_events ? "visíveis" : "ocultos") + ", presenças " +
      (filter.show_presences ? "visíveis" : "ocultas") + ")");
}

void GraphsWorkspace::update_lens() {
  const std::string lens = lens_combo_.get_active_id().raw();
  if (lens == "situation") {
    update_situation();
    lens_stack_.set_visible_child("situation");
  } else if (lens == "agenda") {
    update_agenda();
    lens_stack_.set_visible_child("agenda");
  } else {
    lens_stack_.set_visible_child("timeline");
  }
}

void GraphsWorkspace::update_situation() {
  clear_list(situation_list_);
  if (!snapshot_ || !view_snapshot_) {
    situation_context_.set_text("Escolha um eixo ficcional para ver a situação.");
    situation_state_.set_text("");
    return;
  }
  std::optional<application::TimelinePoint> point;
  if (selected_source_point_id_) {
    const auto found = std::find_if(
        snapshot_->points.begin(), snapshot_->points.end(), [&](const auto &v) {
          return v.id == *selected_source_point_id_;
        });
    if (found != snapshot_->points.end())
      point = *found;
  }
  if (!point && view_snapshot_->context.window.start_point_id) {
    const auto found = std::find_if(
        snapshot_->points.begin(), snapshot_->points.end(), [&](const auto &v) {
          return v.id == *view_snapshot_->context.window.start_point_id;
        });
    if (found != snapshot_->points.end())
      point = *found;
  }
  if (!point && !snapshot_->points.empty())
    point = snapshot_->points.front();
  if (!point) {
    situation_context_.set_text("Este recorte não contém ponto temporal.");
    situation_state_.set_text("");
    return;
  }

  situation_context_.set_text("Eixo: " + snapshot_->axis.name + " — " +
                              point->label + " (ordem " +
                              std::to_string(point->ordinal) + ")");
  situation_state_.set_text(point->description.empty()
                                ? "Sem descrição para este ponto."
                                : point->description);
  append_row(situation_list_, "Acontecimentos neste ponto");
  std::size_t event_count{};
  for (const auto &event : snapshot_->events) {
    if (event.point_id != point->id)
      continue;
    ++event_count;
    append_row(situation_list_, event.entity_name + " — " +
                                    (event.description.empty()
                                         ? event.inclusion_reason
                                         : event.description),
               [this, id = event.occurrence_id] {
                 show_selection(application::TimelineVisualKind::Event, id);
               });
  }
  if (event_count == 0)
    append_row(situation_list_, "Nenhum acontecimento situado neste ponto.");

  append_row(situation_list_, "Presenças ativas neste ponto");
  std::size_t presence_count{};
  for (const auto &presence : snapshot_->presences) {
    if (presence.start_ordinal > point->ordinal ||
        (presence.end_ordinal && *presence.end_ordinal < point->ordinal))
      continue;
    ++presence_count;
    append_row(situation_list_, presence.entity_name + " em " +
                                    presence.location_name + " — " +
                                    presence.inclusion_reason,
               [this, id = presence.presence_id] {
                 show_selection(application::TimelineVisualKind::Presence, id);
               });
  }
  if (presence_count == 0)
    append_row(situation_list_, "Nenhuma presença ativa neste ponto.");
}

void GraphsWorkspace::update_agenda() {
  clear_list(agenda_list_);
  if (!snapshot_) {
    agenda_context_.set_text("Escolha um eixo ficcional para ver a agenda.");
    return;
  }
  if (snapshot_->points.empty()) {
    agenda_context_.set_text("Itens ordenados pelo tempo ficcional; a agenda "
                             "não afirma uma ordem editorial de publicação.");
    append_row(agenda_list_, "Este recorte não possui pontos temporais.");
    return;
  }
  const auto entries = application::build_temporal_agenda(*snapshot_);
  const auto visible_count = std::min(entries.size(), agenda_row_limit_);
  std::string context =
      "Itens ordenados pelo tempo ficcional; a agenda não afirma uma ordem "
      "editorial de publicação.";
  if (entries.size() > visible_count)
    context += " Mostrando os primeiros " + std::to_string(visible_count) +
               " de " + std::to_string(entries.size()) +
               " itens; reduza o recorte para ver o restante.";
  agenda_context_.set_text(context);
  for (std::size_t index = 0; index < visible_count; ++index) {
    const auto &entry = entries[index];
    const std::string text =
        (entry.kind == application::TemporalAgendaEntryKind::Point ? "" : "  ") +
        entry.title + (entry.detail.empty() ? "" : " — " + entry.detail);
    if (entry.kind == application::TemporalAgendaEntryKind::Point) {
      append_row(agenda_list_, text, [this, id = entry.source_id] {
        show_selection(application::TimelineVisualKind::Point, id);
      });
    } else if (entry.kind == application::TemporalAgendaEntryKind::Event) {
      append_row(agenda_list_, text, [this, id = entry.source_id] {
        show_selection(application::TimelineVisualKind::Event, id);
      });
    } else {
      append_row(agenda_list_, text, [this, id = entry.source_id] {
        show_selection(application::TimelineVisualKind::Presence, id);
      });
    }
  }
  if (entries.size() > visible_count)
    append_row(agenda_list_, std::to_string(entries.size() - visible_count) +
                                 " itens não renderizados neste recorte.");
}

void GraphsWorkspace::reset_inspector() {
  selected_source_entity_id_.reset();
  selected_source_point_id_.reset();
  inspector_kind_.set_text("Selecione um ponto, acontecimento ou presença.");
  inspector_name_.set_text("");
  inspector_position_.set_text("");
  inspector_description_.set_text("");
  inspector_reason_.set_text("");
  open_source_button_.set_label("Abrir origem no Planejamento");
  open_source_button_.set_sensitive(false);
}

void GraphsWorkspace::update_zoom(double value) {
  timeline_.set_zoom(value, timeline_scroll_.get_width());
  const auto percent = static_cast<int>(std::lround(timeline_.zoom() * 100.0));
  zoom_label_.set_text(std::to_string(percent) + "%");
  zoom_out_button_.set_sensitive(timeline_.zoom() > 1.0);
  zoom_in_button_.set_sensitive(timeline_.zoom() < 8.0);
  signal_status_message_.emit("Zoom temporal: " + std::to_string(percent) +
                              "% (somente visual)");
}

void GraphsWorkspace::show_selection(application::TimelineVisualKind kind,
                                     const std::string &id) {
  if (!snapshot_)
    return;
  inspector_visible_ = true;
  inspector_scroll_.set_visible(true);
  state_.set_text("");
  timeline_.select_item(kind, id);
  selected_source_entity_id_.reset();
  selected_source_point_id_.reset();
  if (kind == application::TimelineVisualKind::Point) {
    const auto found =
        std::find_if(snapshot_->points.begin(), snapshot_->points.end(),
                     [&](const auto &value) { return value.id == id; });
    if (found == snapshot_->points.end())
      return;
    selected_source_point_id_ = found->id;
    inspector_kind_.set_text("Ponto temporal");
    inspector_name_.set_text(found->label);
    inspector_position_.set_text("Ordinal: " + std::to_string(found->ordinal));
    inspector_description_.set_text(found->description);
    inspector_reason_.set_text("Ponto de contexto no eixo ficcional.");
    open_source_button_.set_label("Abrir ponto no Planejamento");
    open_source_button_.set_sensitive(true);
  } else if (kind == application::TimelineVisualKind::Event) {
    const auto found = std::find_if(
        snapshot_->events.begin(), snapshot_->events.end(),
        [&](const auto &value) { return value.occurrence_id == id; });
    if (found == snapshot_->events.end())
      return;
    selected_source_entity_id_ = found->entity_id;
    selected_source_point_id_ = found->point_id;
    inspector_kind_.set_text("Acontecimento");
    inspector_name_.set_text(found->entity_name);
    inspector_position_.set_text("Ordinal: " + std::to_string(found->ordinal));
    inspector_description_.set_text(found->description);
    std::string reason = "Acontecimento. Incluído porque: " +
                         found->inclusion_reason;
    persistence::PlanningQuery participant_query;
    participant_query.limit = 100;
    const auto participants = service_.planning().event_participations(
        found->occurrence_id, participant_query);
    if (!participants.empty()) {
      reason += "\nParticipantes: ";
      for (std::size_t index = 0; index < participants.size(); ++index) {
        if (index != 0)
          reason += ", ";
        const auto participant = service_.narrative().entity(
            participants[index].participant_entity_id);
        reason += (participant ? participant->name : "Entidade ausente") +
                  (participants[index].role.empty()
                       ? ""
                       : " (" + participants[index].role + ")");
      }
    }
    inspector_reason_.set_text(reason);
    open_source_button_.set_label("Abrir acontecimento no Planejamento");
    open_source_button_.set_sensitive(true);
  } else {
    const auto found = std::find_if(
        snapshot_->presences.begin(), snapshot_->presences.end(),
        [&](const auto &value) { return value.presence_id == id; });
    if (found == snapshot_->presences.end())
      return;
    selected_source_entity_id_ = found->entity_id;
    inspector_kind_.set_text("Presença — sujeito e Local");
    inspector_name_.set_text(found->entity_name + " em " +
                             found->location_name);
    inspector_position_.set_text(
        "Do ordinal " + std::to_string(found->start_ordinal) +
        (found->end_ordinal ? " ao " + std::to_string(*found->end_ordinal)
                            : " em diante"));
    inspector_description_.set_text(found->description);
    inspector_reason_.set_text(
        "Sujeito de presença: " + found->entity_name +
        "\nLocal de presença: " + found->location_name +
        "\nIncluída porque: " + found->inclusion_reason);
    open_source_button_.set_label("Abrir entidade no Planejamento");
    open_source_button_.set_sensitive(true);
  }
  update_situation();
  update_agenda();
}

void GraphsWorkspace::open_selection_source() {
  if (selected_source_entity_id_)
    signal_entity_source_requested_.emit(*selected_source_entity_id_);
  else if (selected_source_point_id_)
    signal_time_point_source_requested_.emit(*selected_source_point_id_);
}

void GraphsWorkspace::on_timeline_painted(double milliseconds) {
  last_paint_ms_ = milliseconds;
  ++paint_samples_;
  const auto rounded = static_cast<int>(std::lround(milliseconds * 100.0));
  paint_metric_.set_text("Cairo: " +
                         std::to_string(static_cast<double>(rounded) / 100.0) +
                         " ms");
}

void GraphsWorkspace::show_failure(const std::exception &error) {
  view_snapshot_.reset();
  snapshot_.reset();
  timeline_.clear();
  reset_inspector();
  update_lens();
  state_.set_text("Não foi possível montar a projeção: " +
                  std::string(error.what()));
  signal_status_message_.emit("Falha ao atualizar a linha do tempo");
}

void GraphsWorkspace::reset() {
  refreshing_axis_ = true;
  refreshing_context_ = true;
  axes_.clear();
  works_.clear();
  axis_points_.clear();
  axis_combo_.remove_all();
  work_combo_.remove_all();
  window_combo_.remove_all();
  start_point_combo_.remove_all();
  end_point_combo_.remove_all();
  lens_combo_.remove_all();
  work_combo_.append("all", "Todo o Projeto");
  work_combo_.set_active_id("all");
  window_combo_.append("all", "Eixo inteiro");
  window_combo_.append("point", "Um ponto");
  window_combo_.append("range", "Período fechado");
  window_combo_.set_active_id("all");
  lens_combo_.append("timeline", "Timeline");
  lens_combo_.append("situation", "Situação");
  lens_combo_.append("agenda", "Agenda");
  lens_combo_.set_active_id("timeline");
  refreshing_axis_ = false;
  refreshing_context_ = false;
  axis_combo_.set_sensitive(false);
  update_window_controls();
  view_snapshot_.reset();
  snapshot_.reset();
  timeline_.clear();
  reset_inspector();
  clear_list(situation_list_);
  clear_list(agenda_list_);
  situation_context_.set_text("Abra um projeto para consultar a situação.");
  situation_state_.set_text("");
  agenda_context_.set_text("Abra um projeto para consultar a agenda.");
  lens_stack_.set_visible_child("timeline");
  state_.set_text("Abra um projeto para visualizar sua linha do tempo.");
  search_.set_text("");
  events_filter_.set_active(true);
  presences_filter_.set_active(true);
  paint_samples_ = 0;
  last_paint_ms_ = 0.0;
  paint_metric_.set_text("Cairo: —");
  update_zoom(1.0);
}

void GraphsWorkspace::toggle_inspector() {
  inspector_visible_ = !inspector_visible_;
  inspector_scroll_.set_visible(inspector_visible_);
}

} // namespace inde::ui
