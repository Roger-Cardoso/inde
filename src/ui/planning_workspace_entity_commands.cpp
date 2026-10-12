#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>

namespace inde::ui {

void PlanningWorkspace::create_entity() {
  auto *owner = owner_window();
  if (types_.empty() && service_.current())
    refresh_types();
  if (!owner || types_.empty())
    return;
  auto *dialog = new OverlayDialog("Novo elemento da história", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  dialog->allow_primary_shortcut();
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *type = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : types_)
    type->append(value.id, value.name);
  type->set_active(0);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome (obrigatório)");
  auto *summary = Gtk::make_managed<Gtk::Entry>();
  summary->set_placeholder_text("Resumo");
  form->set_margin(16);
  append_labeled_form_field(*form, "Categoria do elemento", *type,
                            "Escolha a categoria narrativa do elemento da história.");
  append_labeled_form_field(*form, "Nome do elemento da história", *name,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Resumo", *summary,
                            "Breve descrição do elemento da história.");
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, type, name,
                                     summary](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        const auto created = service_.narrative().create_entity(
            type->get_active_id().raw(), name->get_text(), summary->get_text());
        selected_entity_id_ = created.id;
        offset_ = 0;
        refresh_entities();
        if (std::none_of(page_.begin(), page_.end(), [&](const auto &value) {
              return value.id == created.id;
            }))
          page_.push_back(created);
        refresh_details();
        show_entity_detail_page();
        signal_status_message_.emit("Elemento da história narrativa criada e salva");
      } catch (const std::exception &error) {
        show_error("Não foi possível criar o elemento da história", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::edit_selected_entity() {
  if (!selected_entity_id_)
    return;
  const auto found =
      std::find_if(page_.begin(), page_.end(), [&](const auto &value) {
        return value.id == *selected_entity_id_;
      });
  auto *owner = owner_window();
  if (!owner || found == page_.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar elemento da história", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  dialog->allow_primary_shortcut();
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *type = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : types_)
    type->append(value.id, value.name);
  type->set_active_id(original.entity_type_id);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text(original.name);
  auto *summary = Gtk::make_managed<Gtk::Entry>();
  summary->set_text(original.summary);
  form->set_margin(16);
  append_labeled_form_field(*form, "Categoria do elemento", *type,
                            "Escolha a categoria narrativa do elemento da história.");
  append_labeled_form_field(*form, "Nome do elemento da história", *name,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Resumo", *summary,
                            "Breve descrição do elemento da história.");
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, type, name,
                                     summary](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        original.entity_type_id = type->get_active_id().raw();
        original.name = name->get_text();
        original.summary = summary->get_text();
        static_cast<void>(service_.narrative().update_entity(original));
        refresh_entities();
        signal_status_message_.emit("Elemento da história narrativa atualizada e salva");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar o elemento da história", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_selected_entity() {
  if(!selected_entity_id_ || !owner_window())return;
  confirm_project_removal(*owner_window(),service_,project::RemovalKind::Entity,*selected_entity_id_,[this]{
    selected_entity_id_.reset();refresh_entities();show_explorer_page();
    signal_status_message_.emit("Elemento da história removida; operação registrada no histórico do Projeto");
  });
}

} // namespace inde::ui
