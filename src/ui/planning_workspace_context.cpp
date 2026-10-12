#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::refresh_editorial_context() {
  const auto *entity = selected_entity();
  const bool has_entity = entity != nullptr;
  work_scope_section_.set_visible(has_entity);
  editorial_reference_section_.set_visible(has_entity);
  for (auto *widget :
       {static_cast<Gtk::Widget *>(&work_scope_separator_),
        static_cast<Gtk::Widget *>(&work_scope_title_),
        static_cast<Gtk::Widget *>(&work_scope_toolbar_),
        static_cast<Gtk::Widget *>(&work_scope_list_),
        static_cast<Gtk::Widget *>(&editorial_reference_separator_),
        static_cast<Gtk::Widget *>(&editorial_reference_title_),
        static_cast<Gtk::Widget *>(&editorial_reference_toolbar_),
        static_cast<Gtk::Widget *>(&editorial_reference_list_)})
    widget->set_visible(has_entity);

  work_scopes_.clear();
  editorial_references_.clear();
  selected_work_scope_id_.reset();
  selected_editorial_reference_id_.reset();
  while (auto *child = work_scope_list_.get_first_child())
    work_scope_list_.remove(*child);
  while (auto *child = editorial_reference_list_.get_first_child())
    editorial_reference_list_.remove(*child);
  work_scope_edit_button_.set_sensitive(false);
  work_scope_remove_button_.set_sensitive(false);
  work_scope_actions_button_.set_sensitive(false);
  editorial_reference_edit_button_.set_sensitive(false);
  editorial_reference_remove_button_.set_sensitive(false);
  editorial_reference_actions_button_.set_sensitive(false);
  work_scope_add_button_.set_sensitive(has_entity &&
                                       !service_.catalog().works.empty());
  editorial_reference_add_button_.set_sensitive(false);
  if (!entity)
    return;

  persistence::EntityWorkScopeQuery scope_query;
  scope_query.entity_id = entity->id;
  scope_query.limit = 100;
  work_scopes_ = service_.narrative().work_scopes(scope_query);
  for (const auto &scope : work_scopes_) {
    const auto work = std::find_if(
        service_.catalog().works.begin(), service_.catalog().works.end(),
        [&](const auto &value) { return value.id == scope.work_id; });
    const auto label = (work == service_.catalog().works.end() ? "Obra ausente"
                                                               : work->title) +
                       (scope.notes.empty() ? "" : " — " + scope.notes);
    auto *button = Gtk::make_managed<Gtk::Button>(label);
    button->set_halign(Gtk::Align::FILL);
    button->signal_clicked().connect(
        [this, id = scope.id] { select_work_scope(id); });
    attach_context_menu(*button, {{"Editar notas",
                                   [this, id = scope.id] {
                                     select_work_scope(id);
                                     edit_work_scope();
                                   },
                                   "document-edit-symbolic"},
                                  {"Desvincular",
                                   [this, id = scope.id] {
                                     select_work_scope(id);
                                     delete_work_scope();
                                   },
                                   "user-trash-symbolic", true, true, true}});
    work_scope_list_.append(*button);
  }

  persistence::EditorialReferenceQuery reference_query;
  reference_query.entity_id = entity->id;
  reference_query.limit = 100;
  editorial_references_ =
      service_.narrative().editorial_references(reference_query);
  for (const auto &reference : editorial_references_) {
    const auto nodes = service_.structural_nodes_for_work(reference.work_id);
    const auto node =
        std::find_if(nodes.begin(), nodes.end(), [&](const auto &value) {
          return value.id == reference.editorial_node_id;
        });
    const auto node_name =
        node == nodes.end() ? "Unidade ausente" : node->title;
    auto *button =
        Gtk::make_managed<Gtk::Button>(node_name + " — " + reference.purpose);
    button->set_halign(Gtk::Align::FILL);
    button->signal_clicked().connect(
        [this, id = reference.id] { select_editorial_reference(id); });
    attach_context_menu(*button, {{"Editar referência",
                                   [this, id = reference.id] {
                                     select_editorial_reference(id);
                                     edit_editorial_reference();
                                   },
                                   "document-edit-symbolic"},
                                  {"Remover referência",
                                   [this, id = reference.id] {
                                     select_editorial_reference(id);
                                     delete_editorial_reference();
                                   },
                                   "user-trash-symbolic", true, true, true}});
    editorial_reference_list_.append(*button);
  }
  editorial_reference_add_button_.set_sensitive(!work_scopes_.empty());
}

void PlanningWorkspace::refresh_writing_context() {
  while (auto *child = writing_reference_list_.get_first_child())
    writing_reference_list_.remove(*child);
  const auto *entity = selected_entity();
  writing_reference_section_.set_visible(entity != nullptr);
  writing_library_button_.set_sensitive(entity != nullptr);
  if (!entity) {
    writing_reference_hint_.set_text("Selecione um elemento da história.");
    return;
  }
  persistence::DocumentQuery query;
  query.entity_id = entity->id;
  query.limit = 200;
  const auto documents = service_.writing().document_summaries(query);
  writing_reference_hint_.set_text(
      documents.empty()
          ? "Este elemento da história ainda não foi associado explicitamente a um Documento."
          : std::to_string(documents.size()) +
                (documents.size() == 1 ? " Documento usa este elemento da história"
                                       : " Documentos usam este elemento da história"));
  for (const auto &document : documents) {
    auto *button = Gtk::make_managed<Gtk::Button>(document.title);
    button->set_halign(Gtk::Align::FILL);
    button->set_tooltip_text("Abrir Documento; o vínculo pode apontar ao texto inteiro ou a um trecho marcado");
    button->signal_clicked().connect(
        [this, id = document.id] { signal_open_document_requested_.emit(id); });
    writing_reference_list_.append(*button);
  }
}

void PlanningWorkspace::select_work_scope(std::string id) {
  selected_work_scope_id_ = std::move(id);
  work_scope_edit_button_.set_sensitive(true);
  work_scope_remove_button_.set_sensitive(true);
  work_scope_actions_button_.set_sensitive(true);
}

void PlanningWorkspace::add_work_scope() {
  const auto *entity = selected_entity();
  auto *owner = owner_window();
  if (!owner || !entity)
    return;
  auto *dialog = new OverlayDialog("Vincular elemento da história à Obra", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Vincular", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *work = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : service_.catalog().works) {
    const bool linked = std::any_of(
        work_scopes_.begin(), work_scopes_.end(),
        [&](const auto &scope) { return scope.work_id == value.id; });
    if (!linked)
      work->append(value.id, value.title);
  }
  work->set_active(0);
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_placeholder_text("Notas sobre o área da consulta nesta Obra");
  form->set_margin(16);
  append_labeled_form_field(
      *form, "Obra", *work,
      "Escolha a obra que passa a incluir este elemento da história.");
  append_labeled_form_field(*form, "Notas sobre o área da consulta", *notes);
  dialog->get_content_area()->append(*form);
  const auto entity_id = entity->id;
  dialog->signal_response().connect(
      [this, dialog, entity_id, work, notes](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            if (work->get_active_id().empty())
              throw std::runtime_error(
                  "O elemento da história já está vinculada a todas as Obras disponíveis");
            static_cast<void>(service_.narrative().add_entity_to_work(
                entity_id, work->get_active_id().raw(), notes->get_text()));
            refresh_entities();
            signal_status_message_.emit("Elemento da história associado à Obra");
          } catch (const std::exception &error) {
            show_error("Não foi possível vincular o elemento da história", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::edit_work_scope() {
  if (!selected_work_scope_id_)
    return;
  const auto found = std::find_if(
      work_scopes_.begin(), work_scopes_.end(),
      [&](const auto &value) { return value.id == *selected_work_scope_id_; });
  auto *owner = owner_window();
  if (!owner || found == work_scopes_.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar vínculo com a Obra", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_text(original.notes);
  notes->set_placeholder_text("Notas sobre o área da consulta nesta Obra");
  form->set_margin(16);
  append_labeled_form_field(*form, "Notas sobre o área da consulta", *notes);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, original, notes](int response) mutable {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            original.notes = notes->get_text();
            static_cast<void>(service_.narrative().update_work_scope(original));
            refresh_details();
            signal_status_message_.emit("Vínculo com a Obra atualizado");
          } catch (const std::exception &error) {
            show_error("Não foi possível editar o vínculo", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::delete_work_scope() {
  if (!selected_work_scope_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  const auto id = *selected_work_scope_id_;
  confirm_project_removal(*owner, service_, project::RemovalKind::EntityWorkScope, id, [this] {
        refresh_entities();
        signal_status_message_.emit("Vínculo com a Obra removido");
  });
}

void PlanningWorkspace::select_editorial_reference(std::string id) {
  selected_editorial_reference_id_ = std::move(id);
  editorial_reference_edit_button_.set_sensitive(true);
  editorial_reference_remove_button_.set_sensitive(true);
  editorial_reference_actions_button_.set_sensitive(true);
}

void PlanningWorkspace::add_editorial_reference() {
  const auto *entity = selected_entity();
  auto *owner = owner_window();
  if (!owner || !entity)
    return;
  auto *dialog = new OverlayDialog("Nova referência editorial", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *node = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &scope : work_scopes_) {
    const auto work = std::find_if(
        service_.catalog().works.begin(), service_.catalog().works.end(),
        [&](const auto &value) { return value.id == scope.work_id; });
    const auto work_name =
        work == service_.catalog().works.end() ? "Obra" : work->title;
    for (const auto &value : service_.structural_nodes_for_work(scope.work_id))
      node->append(value.id, work_name + " — " + value.title);
  }
  node->set_active(0);
  auto *purpose = Gtk::make_managed<Gtk::Entry>();
  purpose->set_placeholder_text(
      "Finalidade, por exemplo: aparece, mencionada, revelada");
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_placeholder_text("Notas editoriais");
  form->set_margin(16);
  append_labeled_form_field(*form, "Elemento do manuscrito", *node,
                            "Escolha onde o elemento da história será apresentado.");
  append_labeled_form_field(*form, "Finalidade da apresentação", *purpose);
  append_labeled_form_field(*form, "Notas editoriais", *notes);
  dialog->get_content_area()->append(*form);
  const auto entity_id = entity->id;
  dialog->signal_response().connect(
      [this, dialog, entity_id, node, purpose, notes](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            if (node->get_active_id().empty())
              throw std::runtime_error(
                  "Crie um elemento do manuscrito em uma Obra vinculada");
            static_cast<void>(service_.narrative().add_editorial_reference(
                entity_id, node->get_active_id().raw(), purpose->get_text(),
                notes->get_text()));
            refresh_details();
            signal_status_message_.emit("Referência editorial criada");
          } catch (const std::exception &error) {
            show_error("Não foi possível criar a referência", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::edit_editorial_reference() {
  if (!selected_editorial_reference_id_)
    return;
  const auto found =
      std::find_if(editorial_references_.begin(), editorial_references_.end(),
                   [&](const auto &value) {
                     return value.id == *selected_editorial_reference_id_;
                   });
  auto *owner = owner_window();
  if (!owner || found == editorial_references_.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar referência editorial", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *purpose = Gtk::make_managed<Gtk::Entry>();
  purpose->set_text(original.purpose);
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_text(original.notes);
  form->set_margin(16);
  append_labeled_form_field(*form, "Finalidade da apresentação", *purpose);
  append_labeled_form_field(*form, "Notas editoriais", *notes);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, original, purpose, notes](int response) mutable {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            original.purpose = purpose->get_text();
            original.notes = notes->get_text();
            static_cast<void>(
                service_.narrative().update_editorial_reference(original));
            refresh_details();
            signal_status_message_.emit("Referência editorial atualizada");
          } catch (const std::exception &error) {
            show_error("Não foi possível editar a referência", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::delete_editorial_reference() {
  if (!selected_editorial_reference_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  const auto id = *selected_editorial_reference_id_;
  auto *dialog = new OverlayDialog(*owner, "Remover esta referência editorial?",
                                   false, Gtk::MessageType::QUESTION,
                                   Gtk::ButtonsType::YES_NO, true);
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.narrative().remove_editorial_reference(id);
        refresh_details();
        signal_status_message_.emit("Referência editorial removida");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover a referência", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

} // namespace inde::ui
