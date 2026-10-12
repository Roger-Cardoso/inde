#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/temporal_context.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::configure_action_menus() {
  configure_overflow_button(
      entity_actions_button_,
      {{"Editar elemento da história", [this] { edit_selected_entity(); },
        "document-edit-symbolic"},
       {"Datas e horários…", [this] {
          if (auto *e = selected_entity(); e && owner_window()) show_temporal_context(*owner_window(), service_, "entity", e->id);
        }, "x-office-calendar-symbolic"},
       {"Nomes alternativos…", [this] { show_aliases(); }, "contact-new-symbolic"},
       {"Excluir elemento da história", [this] { delete_selected_entity(); },
        "user-trash-symbolic", true, true, true}},
      "Ações do elemento da história");
  configure_overflow_button(entity_type_actions_button_,
                            {{"Editar categoria", [this] { edit_entity_type(); },
                              "document-edit-symbolic"},
                             {"Excluir categoria", [this] { delete_entity_type(); },
                              "user-trash-symbolic", true, true, true}},
                            "Ações da categoria de elemento selecionada");
  configure_overflow_button(
      work_scope_actions_button_,
      {{"Editar notas", [this] { edit_work_scope(); },
        "document-edit-symbolic"},
       {"Desvincular", [this] { delete_work_scope(); },
        "user-trash-symbolic", true, true, true}},
      "Ações do vínculo selecionado");
  configure_overflow_button(
      relation_type_actions_button_,
      {{"Editar tipo de relação", [this] { edit_relation_type(); },
        "document-edit-symbolic"},
       {"Excluir tipo de relação", [this] { delete_relation_type(); },
        "user-trash-symbolic", true, true, true}},
      "Ações do tipo de relação");
  configure_overflow_button(
      relation_actions_button_,
      {{"Editar relação", [this] { edit_relation(); },
        "document-edit-symbolic"},
       {"Datas e horários…", [this] {
          if (selected_relation_id_ && owner_window()) show_temporal_context(*owner_window(), service_, "relation", *selected_relation_id_);
        }, "x-office-calendar-symbolic"},
       {"Abrir outro elemento da relação", [this] { open_relation_counterpart(); },
        "go-next-symbolic"},
       {"Filtrar por outro elemento da relação", [this] { filter_by_relation_counterpart(); },
        "edit-find-symbolic"},
       {"Excluir relação", [this] { delete_relation(); }, "user-trash-symbolic",
        true, true, true}},
      "Ações da relação selecionada");
  configure_overflow_button(
      event_actions_button_,
      {{"Editar ocorrência", [this] { edit_event_occurrence(); },
        "document-edit-symbolic"},
       {"Remover ocorrência", [this] { delete_event_occurrence(); },
        "user-trash-symbolic", true, true, true}},
      "Ações do acontecimento");
  configure_overflow_button(
      participant_actions_button_,
      {{"Editar participante", [this] { edit_participant(); },
        "document-edit-symbolic"},
       {"Remover participante", [this] { delete_participant(); },
        "user-trash-symbolic", true, true, true}},
      "Ações do participante selecionado");
  configure_overflow_button(presence_actions_button_,
                            {{"Editar presença", [this] { edit_presence(); },
                              "document-edit-symbolic"},
                             {"Remover presença", [this] { delete_presence(); },
                              "user-trash-symbolic", true, true, true}},
                            "Ações da presença selecionada");
  configure_overflow_button(
      editorial_reference_actions_button_,
      {{"Editar referência", [this] { edit_editorial_reference(); },
        "document-edit-symbolic"},
       {"Remover referência", [this] { delete_editorial_reference(); },
        "user-trash-symbolic", true, true, true}},
      "Ações da referência selecionada");
  configure_overflow_button(
      axis_actions_button_,
      {{"Editar linha do tempo", [this] { edit_time_axis(); }, "document-edit-symbolic"},
       {"Calendário da linha do tempo…", [this] { show_calendar(); }, "x-office-calendar-symbolic"},
       {"Ver mês do calendário…", [this] { show_calendar_month(); }, "x-office-calendar-symbolic"},
       {"Excluir linha do tempo", [this] { delete_time_axis(); }, "user-trash-symbolic",
        true, true, true}},
      "Ações da linha do tempo atual");
  configure_overflow_button(point_actions_button_,
                            {{"Editar ponto", [this] { edit_time_point(); },
                              "document-edit-symbolic"},
                             {"Novo ponto por data…", [this] { create_calendar_point(); }, "list-add-symbolic"},
                             {"Remover ponto", [this] { delete_time_point(); },
                              "user-trash-symbolic", true, true, true}},
                            "Ações do ponto selecionado");
}

void PlanningWorkspace::connect_ui_signals() {
  temporal_library_button_.signal_clicked().connect([this] {
    if(page_stack_.get_visible_child_name()!="temporal-library")remember_navigation();
    show_temporal_library();
  });
  search_.signal_changed().connect([this] {
    if (refreshing_context_)
      return;
    offset_ = 0;
    refresh_entities();
  });
  search_.signal_activate().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::dismiss_filters));
  task_mode_filter_.signal_changed().connect([this] {
    if (refreshing_facets_)
      return;
    selected_type_ids_.clear();
    const auto mode = task_mode_filter_.get_active_id().raw();
    if (mode == "characters")
      selected_type_ids_ = {*type_id_for_key("character")};
    else if (mode == "events")
      selected_type_ids_ = {*type_id_for_key("event")};
    else if (mode == "locations")
      selected_type_ids_ = {*type_id_for_key("location")};
    offset_ = 0;
    refresh_entities();
  });
  work_filter_.signal_changed().connect([this] {
    if (refreshing_context_)
      return;
    refresh_context_editorial_nodes();
    offset_ = 0;
    refresh_entities();
  });
  temporal_axis_filter_.signal_changed().connect([this] {
    if (refreshing_context_ || refreshing_temporal_filter_)
      return;
    refresh_context_time_points();
    offset_ = 0;
    refresh_entities();
  });
  temporal_window_filter_.signal_changed().connect([this] {
    if (refreshing_context_ || refreshing_temporal_filter_)
      return;
    refresh_context_time_points();
    offset_ = 0;
    refresh_entities();
  });
  temporal_point_filter_->signal_selection_changed().connect([this] {
    if (refreshing_context_ || refreshing_temporal_filter_)
      return;
    offset_ = 0;
    refresh_entities();
  });
  temporal_window_end_filter_->signal_selection_changed().connect([this] {
    if (refreshing_context_ || refreshing_temporal_filter_)
      return;
    offset_ = 0;
    refresh_entities();
  });
  editorial_node_filter_->signal_selection_changed().connect([this] {
    if (refreshing_context_)
      return;
    offset_ = 0;
    refresh_entities();
  });
  writing_usage_filter_.signal_changed().connect([this] {
    if (refreshing_context_)
      return;
    const bool specific = writing_usage_filter_.get_active_id() == "document";
    writing_document_filter_->set_sensitive(specific);
    if (!specific)
      writing_document_filter_->clear_selection();
    writing_document_filter_->refresh();
    offset_ = 0;
    refresh_entities();
  });
  writing_document_filter_->signal_selection_changed().connect([this] {
    if (refreshing_context_)
      return;
    offset_ = 0;
    refresh_entities();
  });
  writing_library_button_.signal_clicked().connect([this] {
    if (const auto *entity = selected_entity())
      signal_filter_documents_requested_.emit(entity->id);
  });
  clear_context_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::clear_planning_context));
  apply_filters_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::dismiss_filters));
  previous_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::previous_page));
  next_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::next_page));
  add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_entity));
  entity_type_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_entity_type));
  entity_type_combo_.signal_changed().connect([this] {
    const auto id = entity_type_combo_.get_active_id().raw();
    const auto found =
        std::find_if(types_.begin(), types_.end(),
                     [&](const auto &type) { return type.id == id; });
    entity_type_actions_button_.set_sensitive(found != types_.end() &&
                                              !found->is_builtin);
  });
  edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_selected_entity));
  remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_selected_entity));
  axis_combo_.signal_changed().connect([this] {
    if (!refreshing_axis_) {
      auto context = service_.planning_context();
      const auto axis_id = axis_combo_.get_active_id().raw();
      context.fictional_axis_id =
          axis_id.empty() ? std::nullopt : std::optional<std::string>{axis_id};
      context.fictional_time_point_id.reset();
      context.fictional_window_start_time_point_id.reset();
      context.fictional_window_end_time_point_id.reset();
      service_.set_planning_context(std::move(context));
      refresh_time_points();
    }
  });
  axis_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_time_axis));
  axis_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_time_axis));
  axis_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_time_axis));
  point_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_time_point));
  point_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_time_point));
  point_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_time_point));
  event_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_event_occurrence));
  event_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_event_occurrence));
  participant_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::add_participant));
  participant_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_participant));
  participant_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_participant));
  presence_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::add_presence));
  presence_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_presence));
  presence_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_presence));
  work_scope_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::add_work_scope));
  work_scope_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_work_scope));
  work_scope_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_work_scope));
  editorial_reference_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::add_editorial_reference));
  editorial_reference_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_editorial_reference));
  editorial_reference_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_editorial_reference));
  relation_type_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_relation_type));
  relation_type_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_relation_type));
  relation_type_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_relation_type));
  relation_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_relation));
  relation_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_relation));
  relation_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_relation));
  relation_open_counterpart_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::open_relation_counterpart));
  relation_filter_counterpart_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::filter_by_relation_counterpart));
  context_relation_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_relation));
  narrative_structure_workspace_.signal_status_message().connect(
      [this](const Glib::ustring &message) {
        signal_status_message_.emit(message);
      });
  narrative_structure_workspace_.signal_write_requested().connect([this](const std::string &id) { signal_write_unit_requested_.emit(id); });
  narrative_structure_workspace_.signal_realization_requested().connect([this](const application::BuilderRealization &r) { signal_realization_requested_.emit(r); });
  narrative_page_button_.signal_clicked().connect([this] {
    if (page_stack_.get_visible_child_name() != "narrative")
      remember_navigation();
    show_narrative_page();
  });
  explorer_page_button_.signal_clicked().connect([this] {
    if (page_stack_.get_visible_child_name() != "explorer")
      remember_navigation();
    show_explorer_page();
  });
  relations_page_button_.signal_clicked().connect([this] {
    if (page_stack_.get_visible_child_name() != "relations")
      remember_navigation();
    show_relations_page();
  });
  time_page_button_.signal_clicked().connect([this] {
    if (page_stack_.get_visible_child_name() != "time")
      remember_navigation();
    show_time_page();
  });
  filters_button_.signal_clicked().connect([this] {
    filters_visible_ = !filters_visible_;
    set_overlay_revealer_open(filters_revealer_, filters_visible_);
    filters_button_.set_label(filters_visible_ ? "Ocultar filtros" : "Filtros");
    if (filters_visible_)
      search_.grab_focus();
  });
  relation_filters_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::toggle_relation_filters));
  relation_search_.signal_search_changed().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::refresh_relation_explorer));
  relation_type_filter_.signal_changed().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::refresh_relation_explorer));
  for (auto *selector : {relation_entity_filter_, relation_time_point_filter_,
                         relation_location_filter_, relation_cause_filter_})
    selector->signal_selection_changed().connect(
        sigc::mem_fun(*this, &PlanningWorkspace::refresh_relation_explorer));
  clear_relation_filters_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::clear_relation_filters));
  apply_relation_filters_button_.signal_clicked().connect([this] {
    refresh_relation_explorer();
    if (relation_filters_visible_)
      toggle_relation_filters();
  });
  back_to_explorer_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::go_back_in_navigation));
  history_back_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::go_back_in_navigation));
  history_forward_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::go_forward_in_navigation));
}

} // namespace inde::ui
