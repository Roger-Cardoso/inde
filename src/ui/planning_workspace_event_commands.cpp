#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::edit_event_occurrence() {
  const auto *entity = selected_entity();
  auto *owner = owner_window();
  if (!owner || !entity)
    return;
  auto original = occurrence_;
  auto *dialog = new OverlayDialog(
      original ? "Editar ocorrência" : "Situar acontecimento", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *point = make_time_point_selector();
  if (original)
    point->set_selected(original->time_point_id, "Marco na linha do tempo atual");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição da ocorrência");
  if (original)
    description->set_text(original->description);
  form->set_margin(16);
  append_labeled_form_field(*form, "Ponto de tempo da história", *point,
                            "Pesquise ou selecione o ponto da ocorrência.");
  append_labeled_form_field(*form, "Descrição da ocorrência", *description);
  dialog->get_content_area()->append(*form);
  const auto entity_id = entity->id;
  dialog->signal_response().connect([this, dialog, original, entity_id, point,
                                     description](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (!point->selected_id())
          throw std::runtime_error("Crie e selecione um marco na linha do tempo");
        if (original) {
          original->time_point_id = *point->selected_id();
          original->description = description->get_text();
          static_cast<void>(
              service_.planning().update_event_occurrence(*original));
        } else {
          static_cast<void>(service_.planning().place_event(
              entity_id, *point->selected_id(), description->get_text()));
        }
        refresh_details();
        signal_status_message_.emit("Ocorrência do acontecimento salva");
      } catch (const std::exception &error) {
        show_error("Não foi possível salvar a ocorrência", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_event_occurrence() {
  if (!occurrence_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  const auto id = occurrence_->id;
  auto *dialog = new OverlayDialog(
      *owner, "Remover a ocorrência deste acontecimento?", false,
      Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);
  dialog->set_secondary_text(
      "As participações desta ocorrência também serão removidas.");
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.planning().remove_event_occurrence(id);
        refresh_details();
        signal_status_message_.emit("Ocorrência removida");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover a ocorrência", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::select_participant(std::string id) {
  selected_participant_id_ = std::move(id);
  participant_edit_button_.set_sensitive(true);
  participant_remove_button_.set_sensitive(true);
  participant_actions_button_.set_sensitive(true);
}

void PlanningWorkspace::add_participant() {
  auto *owner = owner_window();
  if (!owner || !occurrence_)
    return;
  auto *dialog = new OverlayDialog("Adicionar participante", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Adicionar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *entity = make_entity_selector(std::nullopt, true);
  auto *role = Gtk::make_managed<Gtk::Entry>();
  role->set_placeholder_text("Papel no acontecimento (obrigatório)");
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_placeholder_text("Notas");
  form->set_margin(16);
  append_labeled_form_field(*form, "Elemento da história participante", *entity,
                            "Pesquise e escolha o elemento da história participante.");
  append_labeled_form_field(*form, "Papel no acontecimento", *role,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Notas", *notes);
  dialog->get_content_area()->append(*form);
  const auto occurrence_id = occurrence_->id;
  dialog->signal_response().connect(
      [this, dialog, occurrence_id, entity, role, notes](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            if (!entity->selected_id())
              throw std::runtime_error("Selecione um elemento da história participante");
            static_cast<void>(service_.planning().add_participant(
                occurrence_id, *entity->selected_id(), role->get_text(),
                notes->get_text()));
            refresh_details();
            signal_status_message_.emit("Participante adicionado");
          } catch (const std::exception &error) {
            show_error("Não foi possível adicionar o participante", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::edit_participant() {
  if (!selected_participant_id_)
    return;
  const auto found = std::find_if(
      participants_.begin(), participants_.end(),
      [&](const auto &value) { return value.id == *selected_participant_id_; });
  auto *owner = owner_window();
  if (!owner || found == participants_.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar participante", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *entity = make_entity_selector(std::nullopt, true);
  entity->set_selected(original.participant_entity_id, "Participante atual");
  auto *role = Gtk::make_managed<Gtk::Entry>();
  role->set_text(original.role);
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_text(original.notes);
  form->set_margin(16);
  append_labeled_form_field(*form, "Elemento da história participante", *entity,
                            "Pesquise e escolha o elemento da história participante.");
  append_labeled_form_field(*form, "Papel no acontecimento", *role,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Notas", *notes);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, entity, role,
                                     notes](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (!entity->selected_id())
          throw std::runtime_error("Selecione um elemento da história participante");
        original.participant_entity_id = *entity->selected_id();
        original.role = role->get_text();
        original.notes = notes->get_text();
        static_cast<void>(service_.planning().update_participation(original));
        refresh_details();
        signal_status_message_.emit("Participação atualizada");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar a participação", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_participant() {
  if (!selected_participant_id_)
    return;
  const auto id = *selected_participant_id_;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog(
      *owner, "Remover esta participação do acontecimento?", false,
      Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.planning().remove_participation(id);
        refresh_details();
        signal_status_message_.emit("Participante removido");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover o participante", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

} // namespace inde::ui
