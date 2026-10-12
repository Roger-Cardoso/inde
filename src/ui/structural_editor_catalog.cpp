#include "inde/ui/structural_editor.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include "inde/ui/incremental_selector.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace inde::ui {

namespace {
void constrain_combo_text(Gtk::ComboBoxText &combo, int characters) {
  for (auto *cell : combo.get_cells()) {
    auto *text = dynamic_cast<Gtk::CellRendererText *>(cell);
    if (!text)
      continue;
    text->property_ellipsize() = Pango::EllipsizeMode::MIDDLE;
    text->property_width_chars() = characters;
    text->property_max_width_chars() = characters;
  }
}
} // namespace

void StructuralEditor::refresh_structural_types() {
  if (!service_.current()) {
    structural_types_.clear();
    return;
  }
  const auto filter = structural_type_filter_.get_active_id().raw();
  const auto managed = structural_type_manager_.get_active_id().raw();
  structural_types_ = service_.structural_element_types();
  refreshing_filters_ = true;
  structural_type_filter_.remove_all();
  structural_type_filter_.append("all", "Todos os tipos de unidade");
  structural_type_manager_.remove_all();
  for (const auto &type : structural_types_) {
    structural_type_filter_.append(type.id, type.name);
    structural_type_manager_.append(
        type.id,
        type.name + (type.is_builtin ? " — sistema" : " — do usuário"));
  }
  if (filter.empty() || !structural_type_filter_.set_active_id(filter))
    structural_type_filter_.set_active_id("all");
  if (managed.empty() || !structural_type_manager_.set_active_id(managed)) {
    const auto custom =
        std::find_if(structural_types_.begin(), structural_types_.end(),
                     [](const auto &type) { return !type.is_builtin; });
    if (custom != structural_types_.end())
      structural_type_manager_.set_active_id(custom->id);
    else if (!structural_types_.empty())
      structural_type_manager_.set_active(0);
  }
  refreshing_filters_ = false;
  const auto selected = structural_type_manager_.get_active_id().raw();
  const auto found =
      std::find_if(structural_types_.begin(), structural_types_.end(),
                   [&](const auto &type) { return type.id == selected; });
  const bool editable = found != structural_types_.end() && !found->is_builtin;
  structural_type_edit_button_.set_sensitive(editable);
  structural_type_remove_button_.set_sensitive(editable);
}

void StructuralEditor::create_structural_type() {
  auto *owner = owner_window();
  if (!owner || !service_.current())
    return;
  auto *dialog = new OverlayDialog("Novo tipo de elemento", *owner, true);
  dialog->set_secondary_text(
      "Tipos do usuário ficam disponíveis em todas as Obras deste Projeto.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Ex.: Interlúdio");
  dialog->get_content_area()->append(*name);
  dialog->signal_response().connect([this, dialog, name](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        const auto created =
            service_.create_structural_element_type(name->get_text().raw());
        refresh_structural_types();
        structural_type_manager_.set_active_id(created.id);
        refresh();
        signal_status_message_.emit("Tipo estrutural criado");
      } catch (const std::exception &error) {
        show_error("Não foi possível criar o tipo", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void StructuralEditor::edit_structural_type() {
  const auto id = structural_type_manager_.get_active_id().raw();
  const auto found =
      std::find_if(structural_types_.begin(), structural_types_.end(),
                   [&](const auto &type) { return type.id == id; });
  auto *owner = owner_window();
  if (!owner || found == structural_types_.end() || found->is_builtin)
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar tipo de elemento", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text(original.name);
  dialog->get_content_area()->append(*name);
  dialog->signal_response().connect([this, dialog, name,
                                     original](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        original.name = name->get_text().raw();
        const auto updated = service_.update_structural_element_type(original);
        refresh_structural_types();
        structural_type_manager_.set_active_id(updated.id);
        refresh();
        signal_status_message_.emit("Tipo estrutural atualizado");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar o tipo", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void StructuralEditor::delete_structural_type() {
  const auto id = structural_type_manager_.get_active_id().raw();
  const auto found =
      std::find_if(structural_types_.begin(), structural_types_.end(),
                   [&](const auto &type) { return type.id == id; });
  auto *owner = owner_window();
  if (!owner || found == structural_types_.end() || found->is_builtin)
    return;
  confirm_project_removal(*owner, service_, project::RemovalKind::StructuralType, id, [this] {
        refresh_structural_types();
        refresh();
        signal_status_message_.emit("Tipo estrutural removido");
  });
}

void StructuralEditor::create_structural_node(
    std::optional<std::string> parent_id) {
  if (!selected_work_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Novo elemento estrutural", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *type = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : structural_types_)
    type->append(value.id, value.name + (value.is_builtin ? " — sistema"
                                                          : " — do usuário"));
  type->set_active_id(project::builtin_structural_type_id(
      project::StructuralNodeType::Chapter));
  if (type->get_active_row_number() < 0 && !structural_types_.empty())
    type->set_active(0);
  auto *designator = Gtk::make_managed<Gtk::Entry>();
  designator->set_placeholder_text("Opcional: 1, 0, I, IV, A...");
  designator->set_max_length(64);
  auto *title = Gtk::make_managed<Gtk::Entry>();
  title->set_placeholder_text("Título (obrigatório)");
  form->set_margin(16);
  append_labeled_form_field(*form, "Tipo de elemento", *type);
  append_labeled_form_field(*form, "Número ou designador", *designator,
                            "Opcional e livre: aceita números arábicos, "
                            "romanos ou convenções da Obra.");
  append_labeled_form_field(*form, "Título do elemento", *title,
                            "Campo obrigatório.");
  dialog->get_content_area()->append(*form);
  const auto work_id = *selected_work_id_;
  dialog->signal_response().connect(
      [this, dialog, type, designator, title, work_id,
       parent_id = std::move(parent_id)](int response) {
        if (response == Gtk::ResponseType::ACCEPT)
          try {
            service_.create_structural_node(
                work_id, parent_id, type->get_active_id().raw(),
                designator->get_text().raw(), title->get_text().raw());
            refresh();
          } catch (const std::exception &error) {
            show_error("Não foi possível criar o elemento estrutural", error);
          }
        dialog->hide();
      });
  dialog->present();
}

void StructuralEditor::edit_structural_node(std::string id) {
  if (!selected_work_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  const auto nodes = service_.structural_nodes_for_work(*selected_work_id_);
  const auto found =
      std::find_if(nodes.begin(), nodes.end(),
                   [&](const auto &value) { return value.id == id; });
  if (found == nodes.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar elemento estrutural", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *type = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : structural_types_)
    type->append(value.id, value.name + (value.is_builtin ? " — sistema"
                                                          : " — do usuário"));
  type->set_active_id(original.structural_type_id);
  auto *parent = Gtk::make_managed<Gtk::ComboBoxText>();
  constrain_combo_text(*parent, 46);
  parent->append("", "Raiz da obra");
  const auto descendants = project::StructuralTree(nodes).descendants_of(id);
  const auto path_labels = project::structural_node_path_labels(nodes);
  for (const auto &node : nodes) {
    if (node.id != id && std::find(descendants.begin(), descendants.end(),
                                   node.id) == descendants.end())
      parent->append(node.id, path_labels.at(node.id));
  }
  parent->set_tooltip_text(
      "O caminho mostra tipo, ordem entre irmãos e ancestrais do destino.");
  parent->set_active_id(original.parent_id ? *original.parent_id : "");
  auto *title = Gtk::make_managed<Gtk::Entry>();
  title->set_text(original.title);
  auto *designator = Gtk::make_managed<Gtk::Entry>();
  designator->set_text(original.designator);
  designator->set_max_length(64);
  auto *subtitle = Gtk::make_managed<Gtk::Entry>();
  subtitle->set_text(original.subtitle);
  auto *synopsis = Gtk::make_managed<Gtk::Entry>();
  synopsis->set_text(original.synopsis);
  auto *status = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto *value :
       {"Planejamento", "Em desenvolvimento", "Revisão", "Concluído"})
    status->append(value);
  status->set_active_text(original.status);
  form->set_margin(16);
  append_labeled_form_field(*form, "Tipo de elemento", *type);
  append_labeled_form_field(
      *form, "Número ou designador", *designator,
      "Opcional e livre: aceita 1, 0, I, IV ou convenções próprias.");
  append_labeled_form_field(*form, "Elemento pai", *parent,
                            "Escolha Raiz da obra para remover a hierarquia.");
  append_labeled_form_field(*form, "Título do elemento", *title,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Subtítulo", *subtitle);
  append_labeled_form_field(*form, "Sinopse", *synopsis);
  append_labeled_form_field(*form, "Situação editorial", *status);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, type, parent,
                                     designator, title, subtitle, synopsis,
                                     status](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT)
      try {
        const auto old_parent = original.parent_id;
        original.structural_type_id = type->get_active_id().raw();
        original.designator = designator->get_text().raw();
        original.title = title->get_text().raw();
        original.subtitle = subtitle->get_text().raw();
        original.synopsis = synopsis->get_text().raw();
        original.status = status->get_active_text().raw();
        service_.update_structural_node(original);
        const auto parent_id =
            parent->get_active_id().empty()
                ? std::nullopt
                : std::optional<std::string>(parent->get_active_id().raw());
        if (parent_id != old_parent)
          service_.move_structural_node(original.id, parent_id);
        refresh();
      } catch (const std::exception &error) {
        show_error("Não foi possível editar o elemento estrutural", error);
      }
    dialog->hide();
  });
  dialog->present();
}

void StructuralEditor::delete_structural_node(std::string id) {
  if(auto *owner=owner_window())
    confirm_project_removal(*owner,service_,project::RemovalKind::EditorialBranch,id,[this]{refresh();});
}

} // namespace inde::ui
