#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/author_vocabulary.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::refresh_relation_type_controls() {
  const std::string selected = relation_type_combo_.get_active_id().raw();
  relation_types_ = service_.narrative().relation_types();
  relation_type_combo_.remove_all();
  for (const auto &type : relation_types_)
    relation_type_combo_.append(type.id, type.name);
  if (!selected.empty() && relation_type_combo_.set_active_id(selected)) {
  } else if (!relation_types_.empty()) {
    relation_type_combo_.set_active(0);
  }
  const bool has_type = !relation_type_combo_.get_active_id().empty();
  relation_type_edit_button_.set_sensitive(has_type);
  relation_type_remove_button_.set_sensitive(has_type);
  relation_type_actions_button_.set_sensitive(has_type);
  relation_add_button_.set_sensitive(has_type);
}

void PlanningWorkspace::create_relation_type() {
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Novo tipo de relação", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *key = Gtk::make_managed<Gtk::Entry>();
  key->set_text(author::new_category_key());
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome no sentido origem → destino");
  auto *inverse = Gtk::make_managed<Gtk::Entry>();
  inverse->set_placeholder_text("Como se lê no sentido contrário; dispensável se simétrica");
  auto *direction = Gtk::make_managed<Gtk::ComboBoxText>();
  direction->append("directed", "Tem um sentido");
  direction->append("symmetric", "Vale nos dois sentidos");
  direction->set_active_id("directed");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição");
  form->set_margin(16);
  auto *advanced = Gtk::make_managed<Gtk::Expander>("Identificador técnico (opcional)");
  auto *advanced_body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
  append_labeled_form_field(*advanced_body, "Identificador técnico", *key,
                            "Gerado automaticamente; não muda quando você altera o nome da relação.");
  advanced->set_child(*advanced_body);
  append_labeled_form_field(*form, "Como se lê de um elemento para o outro", *name);
  append_labeled_form_field(*form, "Como se lê no sentido contrário", *inverse,
                            "Pode ficar vazio para relações simétricas.");
  append_labeled_form_field(*form, "Como a relação funciona", *direction);
  append_labeled_form_field(*form, "Descrição", *description);
  form->append(*advanced);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, key, name, inverse, direction, description](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            const auto type = service_.narrative().create_relation_type(
                key->get_text(), name->get_text(), inverse->get_text(),
                project::relation_directionality_from_string(
                    direction->get_active_id().raw()),
                description->get_text());
            refresh_relation_type_controls();
            refresh_relation_filter_options();
            refresh_relation_explorer();
            refresh_context_filters();
            refresh_entities();
            relation_type_combo_.set_active_id(type.id);
            signal_status_message_.emit("Tipo de relação criado");
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

void PlanningWorkspace::edit_relation_type() {
  const std::string id = relation_type_combo_.get_active_id().raw();
  const auto found =
      std::find_if(relation_types_.begin(), relation_types_.end(),
                   [&](const auto &value) { return value.id == id; });
  auto *owner = owner_window();
  if (!owner || found == relation_types_.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar tipo de relação", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text(original.name);
  auto *inverse = Gtk::make_managed<Gtk::Entry>();
  inverse->set_text(original.inverse_name);
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(original.description);
  form->set_margin(16);
  form->append(
      *Gtk::make_managed<Gtk::Label>("Chave imutável: " + original.key));
  append_labeled_form_field(*form, "Como se lê de um elemento para o outro", *name);
  append_labeled_form_field(*form, "Como se lê no sentido contrário", *inverse);
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, name, inverse,
                                     description](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        original.name = name->get_text();
        original.inverse_name = inverse->get_text();
        original.description = description->get_text();
        const auto type = service_.narrative().update_relation_type(original);
        refresh_relation_type_controls();
        refresh_relation_filter_options();
        refresh_relation_explorer();
        refresh_context_filters();
        refresh_entities();
        relation_type_combo_.set_active_id(type.id);
        signal_status_message_.emit("Tipo de relação atualizado");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar o tipo de relação", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_relation_type() {
  const std::string id = relation_type_combo_.get_active_id().raw();
  auto *owner = owner_window();
  if (!owner || id.empty())
    return;
  confirm_project_removal(*owner, service_, project::RemovalKind::RelationType, id, [this] {
        refresh_relation_type_controls();
        refresh_relation_filter_options();
        refresh_relation_explorer();
        refresh_context_filters();
        refresh_entities();
        signal_status_message_.emit("Tipo de relação removido");
  });
}

} // namespace inde::ui
