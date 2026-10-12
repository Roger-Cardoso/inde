#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/temporal_context.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::create_relation() {
  const auto *current = selected_entity();
  auto *owner = owner_window();
  const bool contextual =
      current != nullptr && page_stack_.get_visible_child_name() == "detail";
  if (!owner || relation_types_.empty())
    return;
  auto *dialog = new OverlayDialog("Nova relação narrativa", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *type = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : relation_types_)
    type->append(value.id, value.name);
  type->set_active_id(relation_type_combo_.get_active_id());
  auto *source =
      contextual ? nullptr : make_entity_selector(std::nullopt, false);
  auto *target =
      contextual ? nullptr : make_entity_selector(std::nullopt, false);
  auto *other = contextual
                    ? make_entity_selector(std::nullopt, false, current->id)
                    : nullptr;
  auto *direction =
      contextual ? Gtk::make_managed<Gtk::ComboBoxText>() : nullptr;
  if (direction) {
    direction->append("outgoing", "Elemento da história atual → outro elemento");
    direction->append("incoming", "Outro elemento → elemento atual");
    direction->set_active_id("outgoing");
  }
  auto *when = make_time_point_selector();
  auto *where = make_entity_selector(type_id_for_key("location"), false);
  auto *cause = make_entity_selector(std::nullopt, false);
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição da relação");
  form->set_margin(16);
  append_labeled_form_field(*form, "Tipo de relação", *type);
  if (contextual) {
    append_labeled_form_field(*form, "Outro elemento", *other,
                              "Pesquise e escolha o elemento da história relacionada.");
    append_labeled_form_field(*form, "Sentido da relação", *direction);
  } else {
    append_labeled_form_field(*form, "De", *source);
    append_labeled_form_field(*form, "Para", *target);
  }
  append_labeled_form_field(*form, "Quando (opcional)", *when);
  append_labeled_form_field(*form, "Onde (opcional)", *where,
                            "Apenas elementos da história da categoria Lugar.");
  append_labeled_form_field(*form, "Por causa de (opcional)", *cause);
  append_labeled_form_field(*form, "Notas de contexto", *description);
  dialog->get_content_area()->append(*form);
  const auto current_id =
      current ? std::optional<std::string>{current->id} : std::nullopt;
  dialog->signal_response().connect([this, dialog, contextual, current_id, type,
                                     source, target, other, direction, when,
                                     where, cause, description](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (type->get_active_id().empty())
          throw std::runtime_error("Selecione o tipo de relação");
        std::string source_id;
        std::string target_id;
        if (contextual) {
          if (!current_id || !other->selected_id())
            throw std::runtime_error("Selecione o outro elemento");
          const bool outgoing = direction->get_active_id().raw() == "outgoing";
          const auto other_id = *other->selected_id();
          source_id = outgoing ? *current_id : other_id;
          target_id = outgoing ? other_id : *current_id;
        } else {
          if (!source->selected_id() || !target->selected_id())
            throw std::runtime_error("Selecione origem e destino");
          source_id = *source->selected_id();
          target_id = *target->selected_id();
        }
        static_cast<void>(service_.narrative().create_relation(
            type->get_active_id().raw(), std::move(source_id),
            std::move(target_id), description->get_text(), when->selected_id(),
            where->selected_id(), cause->selected_id()));
        refresh_entities();
        refresh_relation_explorer();
        if (contextual)
          refresh_details();
        signal_status_message_.emit("Relação narrativa criada");
      } catch (const std::exception &error) {
        show_error("Não foi possível criar a relação", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::edit_relation() {
  if (!selected_relation_id_)
    return;
  const auto found = service_.narrative().relation(*selected_relation_id_);
  auto *owner = owner_window();
  if (!owner || !found)
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar relação narrativa", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *type = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : relation_types_)
    type->append(value.id, value.name);
  type->set_active_id(original.relation_type_id);
  auto *source = make_entity_selector(std::nullopt, false);
  source->set_selected(original.source_entity_id,
                       relation_entity_label(original.source_entity_id));
  auto *target = make_entity_selector(std::nullopt, false);
  target->set_selected(original.target_entity_id,
                       relation_entity_label(original.target_entity_id));
  auto *when = make_time_point_selector();
  if (original.fictional_time_point_id)
    when->set_selected(*original.fictional_time_point_id,
                       "Marco na linha do tempo atual");
  auto *where = make_entity_selector(type_id_for_key("location"), false);
  if (original.location_entity_id)
    where->set_selected(*original.location_entity_id,
                        relation_entity_label(*original.location_entity_id));
  auto *cause = make_entity_selector(std::nullopt, false);
  if (original.cause_entity_id)
    cause->set_selected(*original.cause_entity_id,
                        relation_entity_label(*original.cause_entity_id));
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(original.description);
  form->set_margin(16);
  append_labeled_form_field(*form, "Tipo de relação", *type);
  append_labeled_form_field(*form, "De", *source);
  append_labeled_form_field(*form, "Para", *target);
  append_labeled_form_field(*form, "Quando (opcional)", *when);
  append_labeled_form_field(*form, "Onde (opcional)", *where);
  append_labeled_form_field(*form, "Por causa de (opcional)", *cause);
  append_labeled_form_field(*form, "Notas de contexto", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, type, source,
                                     target, when, where, cause,
                                     description](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (type->get_active_id().empty() || !source->selected_id() ||
            !target->selected_id())
          throw std::runtime_error("Selecione tipo, origem e destino");
        original.relation_type_id = type->get_active_id().raw();
        original.source_entity_id = *source->selected_id();
        original.target_entity_id = *target->selected_id();
        original.fictional_time_point_id = when->selected_id();
        original.location_entity_id = where->selected_id();
        original.cause_entity_id = cause->selected_id();
        original.description = description->get_text();
        static_cast<void>(service_.narrative().update_relation(original));
        refresh_entities();
        refresh_relation_explorer();
        signal_status_message_.emit("Relação narrativa atualizada");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar a relação", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_relation() {
  if(!selected_relation_id_ || !owner_window())return;
  confirm_project_removal(*owner_window(),service_,project::RemovalKind::Relation,*selected_relation_id_,[this]{
    selected_relation_id_.reset();refresh_entities();refresh_relation_explorer();refresh_details();
    signal_status_message_.emit("Relação removida; operação registrada no histórico do Projeto");
  });
}

} // namespace inde::ui
