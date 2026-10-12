#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/author_vocabulary.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::refresh_types() {
  types_ = service_.narrative().entity_types();
  refresh_entity_type_controls();
}

void PlanningWorkspace::refresh_entity_type_controls() {
  const auto selected = entity_type_combo_.get_active_id().raw();
  entity_type_combo_.remove_all();
  for (const auto &type : types_)
    entity_type_combo_.append(
        type.id,
        author::category_label(type) + (type.is_builtin ? " — do INDE" : " — pessoal"));
  if (selected.empty() || !entity_type_combo_.set_active_id(selected)) {
    const auto custom =
        std::find_if(types_.begin(), types_.end(),
                     [](const auto &type) { return !type.is_builtin; });
    if (custom != types_.end())
      entity_type_combo_.set_active_id(custom->id);
    else if (!types_.empty())
      entity_type_combo_.set_active(0);
  }
  const auto active = entity_type_combo_.get_active_id().raw();
  const auto found =
      std::find_if(types_.begin(), types_.end(),
                   [&](const auto &type) { return type.id == active; });
  entity_type_actions_button_.set_sensitive(found != types_.end() &&
                                            !found->is_builtin);
}

std::optional<std::string>
PlanningWorkspace::type_id_for_key(const std::string &key) const {
  const auto found =
      std::find_if(types_.begin(), types_.end(),
                   [&](const auto &type) { return type.key == key; });
  if (found == types_.end())
    return std::nullopt;
  return found->id;
}

std::string PlanningWorkspace::type_name(const std::string &id) const {
  const auto found =
      std::find_if(types_.begin(), types_.end(),
                   [&](const auto &type) { return type.id == id; });
  return found == types_.end() ? "Categoria desconhecida" : author::category_label(*found);
}

void PlanningWorkspace::create_entity_type() {
  auto *owner = owner_window();
  if (!owner || !service_.current())
    return;
  auto *dialog = new OverlayDialog("Nova categoria de elemento", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *key = Gtk::make_managed<Gtk::Entry>();
  key->set_text(author::new_category_key());
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome exibido");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição e uso esperado");
  form->set_margin(16);
  auto *advanced = Gtk::make_managed<Gtk::Expander>("Identificador técnico (opcional)");
  auto *advanced_body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
  append_labeled_form_field(*advanced_body, "Identificador técnico", *key,
      "Gerado automaticamente. Alterá-lo não dá à categoria pessoal as funções das categorias do INDE.");
  advanced->set_child(*advanced_body);
  append_labeled_form_field(*form, "Nome da categoria", *name, "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  form->append(*advanced);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, key, name, description](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            const auto created = service_.narrative().create_entity_type(
                key->get_text(), name->get_text(), description->get_text());
            refresh_types();
            refresh_context_filters();
            refresh_entities();
            entity_type_combo_.set_active_id(created.id);
            signal_status_message_.emit("Categoria de elemento criada");
          } catch (const std::exception &error) {
            dialog->set_secondary_text(author::diagnostic(error.what()));
            dialog->keep_open_on_response();
            return;
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::edit_entity_type() {
  const auto id = entity_type_combo_.get_active_id().raw();
  const auto found =
      std::find_if(types_.begin(), types_.end(),
                   [&](const auto &type) { return type.id == id; });
  auto *owner = owner_window();
  if (!owner || found == types_.end())
    return;
  if (found->is_builtin) {
    show_error("Tipo interno protegido",
               std::runtime_error(
                   "Tipos internos garantem integrações do sistema e não "
                   "podem ser alterados. Crie um tipo do usuário para outro "
                   "vocabulário."));
    return;
  }
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar categoria de elemento", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *key = Gtk::make_managed<Gtk::Label>("Chave imutável: " + original.key);
  key->set_halign(Gtk::Align::START);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text(original.name);
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(original.description);
  form->set_margin(16);
  auto *technical = Gtk::make_managed<Gtk::Expander>("Identificador técnico (somente leitura)");
  technical->set_child(*key);
  form->append(*technical);
  append_labeled_form_field(*form, "Nome da categoria", *name, "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, name,
                                     description](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        original.name = name->get_text();
        original.description = description->get_text();
        const auto updated = service_.narrative().update_entity_type(original);
        refresh_types();
        refresh_context_filters();
        refresh_entities();
        entity_type_combo_.set_active_id(updated.id);
        signal_status_message_.emit("Categoria de elemento atualizada");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar a categoria de elemento", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_entity_type() {
  const auto id = entity_type_combo_.get_active_id().raw();
  const auto found =
      std::find_if(types_.begin(), types_.end(),
                   [&](const auto &type) { return type.id == id; });
  auto *owner = owner_window();
  if (!owner || found == types_.end() || found->is_builtin)
    return;
  confirm_project_removal(*owner, service_, project::RemovalKind::EntityType, id, [this, id] {
        selected_type_ids_.erase(std::remove(selected_type_ids_.begin(),
                                             selected_type_ids_.end(), id),
                                 selected_type_ids_.end());
        refresh_types();
        refresh_context_filters();
        refresh_entities();
        signal_status_message_.emit("Categoria de elemento excluída");
  });
}

} // namespace inde::ui
