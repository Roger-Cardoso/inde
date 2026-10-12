#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::refresh_details() {
  const auto found =
      selected_entity_id_
          ? std::find_if(page_.begin(), page_.end(),
                         [&](const auto &entity) {
                           return entity.id == *selected_entity_id_;
                         })
          : page_.end();
  const bool selected = found != page_.end();
  empty_title_.set_visible(!selected);
  empty_message_.set_visible(!selected);
  edit_button_.set_sensitive(selected);
  remove_button_.set_sensitive(selected);
  entity_actions_button_.set_sensitive(selected);
  entity_dates_button_.set_visible(selected);
  cartography_button_.set_visible(selected &&
                                  found->entity_type_id ==
                                      "00000000-0000-4000-9000-000000000002");
  if (!selected) {
    entity_name_.set_text("");
    entity_type_.set_text("");
    entity_summary_.set_text("");
    entity_alias_summary_.set_text("");
    inspector_identity_.set_text("Nenhum elemento selecionado");
    inspector_dates_.set_text("");
    inspector_relations_.set_text("");
    inspector_presences_.set_text("");
    refresh_temporal_context();
    refresh_editorial_context();
    refresh_writing_context();
    refresh_relations();
    return;
  }
  entity_name_.set_text(found->name);
  try {
    auto target = service_.worldbuilding().temporal_target("entity", found->id);
    auto dates = service_.worldbuilding().target_dates(target);
    entity_dates_button_.set_label("Datas e horários (" + std::to_string(dates.size()) + ")…");
  } catch (const std::exception &) { entity_dates_button_.set_label("Datas e horários…"); }
  entity_type_.set_text(type_name(found->entity_type_id));
  entity_summary_.set_text(found->summary.empty() ? "Sem resumo."
                                                  : found->summary);
  try {
    const auto aliases=service_.worldbuilding().aliases(found->id);
    std::string compact="Nomes alternativos: ",full;
    for(std::size_t i=0;i<aliases.size();++i) {
      if(i<3)compact+=(i?" · ":"")+aliases[i].name;
      full+=aliases[i].name+(aliases[i].note.empty()?"":" — "+aliases[i].note)+"\n";
    }
    if(aliases.empty())compact+="nenhum";
    if(aliases.size()>3)compact+=" · +"+std::to_string(aliases.size()-3);
    entity_alias_summary_.set_text(compact);
    entity_alias_summary_.set_tooltip_text(full+"Gerencie em Ações do elemento da história → Nomes alternativos.");
  } catch(const std::exception &e){entity_alias_summary_.set_text("Nomes alternativos indisponíveis: "+std::string(author::diagnostic(e.what())));}
  inspector_identity_.set_text("UUID\n" + found->id);
  inspector_dates_.set_text("Criada em " + found->created_at +
                            "\nAtualizada em " + found->updated_at);
  persistence::RelationQuery relations;
  relations.entity_id = found->id;
  relations.limit = 200;
  inspector_relations_.set_text(
      "Relações (até 200): " +
      std::to_string(service_.narrative().relations(relations).size()));
  persistence::PresenceQuery presences;
  presences.entity_id = found->id;
  presences.limit = 100;
  inspector_presences_.set_text(
      "Presenças registradas (até 100): " +
      std::to_string(service_.planning().presences(presences).size()));
  refresh_temporal_context();
  refresh_editorial_context();
  refresh_writing_context();
  refresh_relations();
}

const project::NarrativeEntity *PlanningWorkspace::selected_entity() const {
  if (!selected_entity_id_)
    return nullptr;
  const auto found =
      std::find_if(page_.begin(), page_.end(), [&](const auto &value) {
        return value.id == *selected_entity_id_;
      });
  return found == page_.end() ? nullptr : &*found;
}

void PlanningWorkspace::refresh_temporal_context() {
  const auto *entity = selected_entity();
  const auto event_type = type_id_for_key("event");
  const bool is_event =
      entity && event_type && entity->entity_type_id == *event_type;
  event_section_.set_visible(is_event);
  for (auto *widget : {static_cast<Gtk::Widget *>(&event_separator_),
                       static_cast<Gtk::Widget *>(&event_title_),
                       static_cast<Gtk::Widget *>(&event_status_),
                       static_cast<Gtk::Widget *>(&event_toolbar_),
                       static_cast<Gtk::Widget *>(&participants_title_),
                       static_cast<Gtk::Widget *>(&participant_toolbar_),
                       static_cast<Gtk::Widget *>(&participant_list_)})
    widget->set_visible(is_event);
  const bool has_entity = entity != nullptr;
  presence_section_.set_visible(has_entity);
  for (auto *widget : {static_cast<Gtk::Widget *>(&presence_separator_),
                       static_cast<Gtk::Widget *>(&presence_title_),
                       static_cast<Gtk::Widget *>(&presence_toolbar_),
                       static_cast<Gtk::Widget *>(&presence_list_)})
    widget->set_visible(has_entity);

  participants_.clear();
  presences_.clear();
  occurrence_.reset();
  selected_participant_id_.reset();
  selected_presence_id_.reset();
  while (auto *child = participant_list_.get_first_child())
    participant_list_.remove(*child);
  while (auto *child = presence_list_.get_first_child())
    presence_list_.remove(*child);
  participant_edit_button_.set_sensitive(false);
  participant_remove_button_.set_sensitive(false);
  participant_actions_button_.set_sensitive(false);
  presence_edit_button_.set_sensitive(false);
  presence_remove_button_.set_sensitive(false);
  presence_actions_button_.set_sensitive(false);
  presence_add_button_.set_sensitive(has_entity && !axes_.empty());
  if (!entity)
    return;

  if (is_event) {
    occurrence_ = service_.planning().event_occurrence_for_entity(entity->id);
    event_edit_button_.set_label(occurrence_ ? "Editar ocorrência" : "Situar");
    event_edit_button_.set_sensitive(!axes_.empty());
    event_remove_button_.set_sensitive(occurrence_.has_value());
    event_actions_button_.set_sensitive(occurrence_.has_value());
    participant_add_button_.set_sensitive(occurrence_.has_value());
    if (occurrence_) {
      const auto point =
          service_.planning().time_point(occurrence_->time_point_id);
      const auto point_description =
          point
              ? point->label + " (ordem " + std::to_string(point->ordinal) + ")"
              : occurrence_->time_point_id;
      event_status_.set_text("Situado em " + point_description +
                             (occurrence_->description.empty()
                                  ? ""
                                  : "\n" + occurrence_->description));
      persistence::PlanningQuery query;
      query.limit = 100;
      participants_ =
          service_.planning().event_participations(occurrence_->id, query);
      for (const auto &participant : participants_) {
        const auto referenced =
            service_.narrative().entity(participant.participant_entity_id);
        const auto name = referenced ? referenced->name : "Elemento da história ausente";
        auto *button =
            Gtk::make_managed<Gtk::Button>(name + " — " + participant.role);
        button->set_halign(Gtk::Align::FILL);
        button->signal_clicked().connect(
            [this, id = participant.id] { select_participant(id); });
        attach_context_menu(*button,
                            {{"Editar participante",
                              [this, id = participant.id] {
                                select_participant(id);
                                edit_participant();
                              },
                              "document-edit-symbolic"},
                             {"Remover participante",
                              [this, id = participant.id] {
                                select_participant(id);
                                delete_participant();
                              },
                              "user-trash-symbolic", true, true, true}});
        participant_list_.append(*button);
      }
    } else {
      event_status_.set_text(
          "Este acontecimento ainda não foi situado no tempo da história.");
    }
  }

  persistence::PresenceQuery presence_query;
  presence_query.entity_id = entity->id;
  presence_query.limit = 100;
  presences_ = service_.planning().presences(presence_query);
  for (const auto &presence : presences_) {
    const auto location =
        service_.narrative().entity(presence.location_entity_id);
    const auto name = location ? location->name : "Local ausente";
    auto *button = Gtk::make_managed<Gtk::Button>(
        name + (presence.end_time_point_id ? " — intervalo fechado"
                                           : " — intervalo aberto"));
    button->set_halign(Gtk::Align::FILL);
    button->signal_clicked().connect(
        [this, id = presence.id] { select_presence(id); });
    attach_context_menu(*button, {{"Editar presença",
                                   [this, id = presence.id] {
                                     select_presence(id);
                                     edit_presence();
                                   },
                                   "document-edit-symbolic"},
                                  {"Remover presença",
                                   [this, id = presence.id] {
                                     select_presence(id);
                                     delete_presence();
                                   },
                                   "user-trash-symbolic", true, true, true}});
    presence_list_.append(*button);
  }
}

void PlanningWorkspace::select_entity(std::string id) {
  // A única semântica do card é navegação. O id fica ativo somente enquanto a
  // página de detalhe estiver aberta, onde Editar e Remover são inequívocos.
  remember_navigation();
  selected_entity_id_ = std::move(id);
  refresh_details();
  show_entity_detail_page();
  signal_status_message_.emit("Elemento da história narrativa selecionada");
}

void PlanningWorkspace::reveal_entity(const std::string &id) {
  if (!service_.current())
    return;
  try {
    const auto entity = service_.narrative().entity(id);
    if (!entity)
      throw std::runtime_error("O elemento da história de origem não existe mais");
    remember_navigation();
    auto context = service_.planning_context();
    // Abrir uma origem conserva Obra, apresentação editorial e recorte
    // temporal. Só os filtros que competiriam diretamente com a entidade são
    // limpos, permitindo voltar à leitura sem perder o caminho contextual.
    context.search = entity->name;
    context.entity_type_ids.clear();
    context.relation_type_ids.clear();
    context.related_entity_id.reset();
    context.offset = 0;
    // The next explorer query is rebuilt from controls. Keep the explicit
    // source navigation in sync, including origins outside the first page.
    const bool was_refreshing = refreshing_context_;
    const bool was_refreshing_facets = refreshing_facets_;
    refreshing_context_ = true;
    refreshing_facets_ = true;
    search_.set_text(entity->name);
    selected_type_ids_.clear();
    selected_relation_type_ids_.clear();
    task_mode_filter_.set_active_id("explore");
    refreshing_facets_ = was_refreshing_facets;
    refreshing_context_ = was_refreshing;
    service_.set_planning_context(std::move(context));
    offset_ = 0;
    // Source navigation must refresh the entity explorer even when the
    // previously visible Planning page was narrative, relations or time.
    show_explorer_page();
    refresh();
    const auto found =
        std::find_if(page_.begin(), page_.end(),
                     [&](const auto &value) { return value.id == entity->id; });
    if (found == page_.end())
      throw std::runtime_error(
          "O elemento da história de origem não foi encontrado na lista atual");
    restoring_navigation_ = true;
    select_entity(entity->id);
    restoring_navigation_ = false;
    signal_status_message_.emit("Origem aberta no Planejamento");
  } catch (const std::exception &error) {
    show_error("Não foi possível abrir a origem", error);
  }
}

Gtk::Window *PlanningWorkspace::owner_window() {
  return dynamic_cast<Gtk::Window *>(get_root());
}

void PlanningWorkspace::show_error(const Glib::ustring &title,
                                   const std::exception &error) {
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog(*owner, author::diagnostic(error.what()), false, Gtk::MessageType::ERROR,
                        Gtk::ButtonsType::CLOSE, true);
  dialog->set_title(title);
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->present();
}

} // namespace inde::ui
