#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::select_presence(std::string id) {
  selected_presence_id_ = std::move(id);
  presence_edit_button_.set_sensitive(true);
  presence_remove_button_.set_sensitive(true);
  presence_actions_button_.set_sensitive(true);
}

void PlanningWorkspace::add_presence() {
  const auto *current = selected_entity();
  const auto location_type = type_id_for_key("location");
  auto *owner = owner_window();
  if (!owner || !current || !location_type)
    return;
  auto *dialog = new OverlayDialog("Nova presença", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *location = make_entity_selector(location_type, false);
  auto *start = make_time_point_selector();
  auto *end = make_time_point_selector();
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição da presença");
  form->set_margin(16);
  append_labeled_form_field(*form, "Local", *location,
                            "Pesquise e escolha um elemento da categoria Lugar.");
  append_labeled_form_field(*form, "Início", *start,
                            "Marco na linha do tempo em que a presença começa.");
  append_labeled_form_field(
      *form, "Fim opcional", *end,
      "Deixe sem seleção se a presença ainda estiver ativa.");
  append_labeled_form_field(*form, "Descrição da presença", *description);
  dialog->get_content_area()->append(*form);
  const auto entity_id = current->id;
  dialog->signal_response().connect([this, dialog, entity_id, location, start,
                                     end, description](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (!location->selected_id())
          throw std::runtime_error("Crie e selecione um elemento da categoria Lugar");
        if (!start->selected_id())
          throw std::runtime_error("Crie e selecione um ponto inicial");
        const auto optional_end = end->selected_id();
        static_cast<void>(service_.planning().create_presence(
            entity_id, *location->selected_id(), *start->selected_id(),
            optional_end, description->get_text()));
        refresh_details();
        signal_status_message_.emit("Presença narrativa criada");
      } catch (const std::exception &error) {
        show_error("Não foi possível criar a presença", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::edit_presence() {
  if (!selected_presence_id_)
    return;
  const auto found = std::find_if(
      presences_.begin(), presences_.end(),
      [&](const auto &value) { return value.id == *selected_presence_id_; });
  const auto location_type = type_id_for_key("location");
  auto *owner = owner_window();
  if (!owner || found == presences_.end() || !location_type)
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar presença", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *location = make_entity_selector(location_type, false);
  location->set_selected(original.location_entity_id, "Local atual");
  auto *start = make_time_point_selector();
  start->set_selected(original.start_time_point_id, "Início atual");
  auto *end = make_time_point_selector();
  if (original.end_time_point_id)
    end->set_selected(*original.end_time_point_id, "Fim atual");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(original.description);
  form->set_margin(16);
  append_labeled_form_field(*form, "Local", *location,
                            "Pesquise e escolha um elemento da categoria Lugar.");
  append_labeled_form_field(*form, "Início", *start,
                            "Marco na linha do tempo em que a presença começa.");
  append_labeled_form_field(
      *form, "Fim opcional", *end,
      "Deixe sem seleção se a presença ainda estiver ativa.");
  append_labeled_form_field(*form, "Descrição da presença", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, location, start,
                                     end, description](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (!location->selected_id())
          throw std::runtime_error("Crie e selecione um elemento da categoria Lugar");
        if (!start->selected_id())
          throw std::runtime_error("Crie e selecione um ponto inicial");
        original.location_entity_id = *location->selected_id();
        original.start_time_point_id = *start->selected_id();
        original.end_time_point_id = end->selected_id();
        original.description = description->get_text();
        static_cast<void>(service_.planning().update_presence(original));
        refresh_details();
        signal_status_message_.emit("Presença narrativa atualizada");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar a presença", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_presence() {
  if (!selected_presence_id_)
    return;
  const auto id = *selected_presence_id_;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog(*owner, "Remover esta presença narrativa?",
                                   false, Gtk::MessageType::QUESTION,
                                   Gtk::ButtonsType::YES_NO, true);
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.planning().remove_presence(id);
        refresh_details();
        signal_status_message_.emit("Presença narrativa removida");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover a presença", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

} // namespace inde::ui
