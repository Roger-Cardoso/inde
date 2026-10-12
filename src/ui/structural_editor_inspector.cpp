#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/structural_editor.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include "inde/ui/incremental_selector.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace inde::ui {

namespace {
std::optional<std::filesystem::path>
editorial_cover_path(const std::filesystem::path &project_path,
                     const std::string &stored) {
  if (stored.empty())
    return std::nullopt;
  std::filesystem::path candidate{stored};
  if (candidate.is_absolute() && std::filesystem::is_regular_file(candidate))
    return candidate;
  candidate = project_path / stored;
  if (std::filesystem::is_regular_file(candidate))
    return candidate;
  candidate = project_path.parent_path() / stored;
  if (std::filesystem::is_regular_file(candidate))
    return candidate;
  return std::nullopt;
}
} // namespace

void StructuralEditor::update_inspector() {
  if (!selected_work_id_ || !selected_structural_node_id_) {
    inspector_type_.set_text("Nenhum elemento selecionado");
    inspector_name_.set_text("");
    inspector_status_.set_text("");
    inspector_synopsis_.set_text("");
    inspector_cover_.set_visible(false);
    inspector_cover_overlay_.set_visible(false);
    inspector_cover_section_.set_visible(false);
    inspector_edit_button_.set_sensitive(false);
    inspector_actions_button_.set_sensitive(false);
    refresh_editorial_references();
    refresh_documents();
    show_structure_page();
    return;
  }
  const auto nodes = service_.structural_nodes_for_work(*selected_work_id_);
  const auto found =
      std::find_if(nodes.begin(), nodes.end(), [&](const auto &node) {
        return node.id == *selected_structural_node_id_;
      });
  if (found == nodes.end()) {
    selected_structural_node_id_.reset();
    update_inspector();
    return;
  }
  inspector_type_.set_text(project::structural_node_position_label(*found));
  inspector_name_.set_text(found->title);
  inspector_status_.set_text("Status: " + found->status);
  inspector_synopsis_.set_text(found->synopsis.empty() ? "Sem sinopse."
                                                       : found->synopsis);
  inspector_synopsis_.set_visible(!found->synopsis.empty());
  inspector_cover_.set_visible(false);
  inspector_cover_overlay_.set_visible(false);
  inspector_cover_section_.set_visible(false);
  const auto &catalog = service_.catalog();
  const auto work = std::find_if(
      catalog.works.begin(), catalog.works.end(),
      [&](const auto &value) { return value.id == *selected_work_id_; });
  if (work != catalog.works.end()) {
    const auto ip = std::find_if(
        catalog.intellectual_properties.begin(),
        catalog.intellectual_properties.end(), [&](const auto &value) {
          return value.id == work->intellectual_property_id;
        });
    if (ip != catalog.intellectual_properties.end()) {
      const auto cover =
          editorial_cover_path(service_.current()->path(), ip->cover_path);
      if (cover) {
        inspector_cover_.set_filename(cover->string());
        inspector_cover_.set_visible(true);
        inspector_cover_overlay_.set_visible(true);
        inspector_cover_section_.set_visible(true);
      }
    }
  }
  inspector_edit_button_.set_sensitive(true);
  inspector_actions_button_.set_sensitive(true);
  refresh_editorial_references();
  refresh_documents();
}

void StructuralEditor::refresh_documents() {
  while (auto *child = documents_list_.get_first_child())
    documents_list_.remove(*child);
  const bool selected = selected_structural_node_id_.has_value();
  create_document_button_.set_sensitive(selected);
  filter_documents_button_.set_sensitive(selected);
  if (!selected) {
    documents_hint_.set_text("Selecione um elemento do manuscrito.");
    return;
  }
  persistence::DocumentQuery query;
  query.editorial_node_id = *selected_structural_node_id_;
  query.limit = 200;
  const auto documents = service_.writing().document_summaries(query);
  documents_hint_.set_text(
      documents.empty()
          ? "Nenhum Documento foi colocado diretamente nesta unidade."
          : std::to_string(documents.size()) +
                (documents.size() == 1 ? " Documento relacionado"
                                       : " Documentos relacionados"));
  for (const auto purpose :
       {project::DocumentPurpose::MainText, project::DocumentPurpose::Revision,
        project::DocumentPurpose::Annotation, project::DocumentPurpose::Outline,
        project::DocumentPurpose::Research, project::DocumentPurpose::Reference,
        project::DocumentPurpose::Other}) {
    const auto count = std::count_if(
        documents.begin(), documents.end(),
        [&](const auto &value) { return value.purpose == purpose; });
    if (count == 0)
      continue;
    auto *category = Gtk::make_managed<Gtk::Label>(
        project::display_name(purpose) + "  ·  " + std::to_string(count));
    category->set_xalign(0.0F);
    category->set_margin_top(8);
    category->set_margin_start(8);
    category->add_css_class("heading");
    documents_list_.append(*category);
    for (const auto &document : documents) {
      if (document.purpose != purpose)
        continue;
      auto *button = Gtk::make_managed<Gtk::Button>();
      auto *content =
          Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 3);
      auto *title = Gtk::make_managed<Gtk::Label>(document.title);
      title->set_xalign(0.0F);
      title->add_css_class("heading");
      auto metadata =
          std::to_string(document.character_count) +
          (document.character_count == 1 ? " caractere" : " caracteres");
      if (!document.revision_label.empty())
        metadata += "  •  " + document.revision_label;
      if (!document.perspective.empty())
        metadata += "  •  " + document.perspective;
      auto *detail = Gtk::make_managed<Gtk::Label>(metadata);
      detail->set_xalign(0.0F);
      detail->set_wrap(true);
      detail->add_css_class("dim-label");
      content->set_margin(8);
      content->append(*title);
      content->append(*detail);
      button->set_child(*content);
      button->set_halign(Gtk::Align::FILL);
      button->set_tooltip_text(
          "Abrir na Escrita\nFinalidade: " +
          project::display_name(document.purpose) +
          (document.perspective.empty()
               ? ""
               : "\nPerspectiva: " + document.perspective));
      button->signal_clicked().connect([this, id = document.id] {
        signal_open_document_requested_.emit(id);
      });
      documents_list_.append(*button);
    }
  }
}

void StructuralEditor::refresh_editorial_references() {
  editorial_references_.clear();
  selected_editorial_reference_id_.reset();
  while (auto *child = reference_list_.get_first_child())
    reference_list_.remove(*child);
  while (auto *child = inherited_references_list_.get_first_child())
    inherited_references_list_.remove(*child);
  const bool selected = selected_work_id_ && selected_structural_node_id_;
  reference_add_button_.set_sensitive(selected);
  reference_edit_button_.set_sensitive(false);
  reference_remove_button_.set_sensitive(false);
  reference_actions_button_.set_sensitive(false);
  if (!selected)
    return;
  persistence::EditorialReferenceQuery query;
  query.editorial_node_id = *selected_structural_node_id_;
  query.limit = 500;
  editorial_references_ = service_.narrative().editorial_references(query);
  for (const auto &reference : editorial_references_) {
    const auto entity = service_.narrative().entity(reference.entity_id);
    auto *button = Gtk::make_managed<Gtk::Button>(
        (entity ? entity->name : "Elemento da história ausente") + " — " +
        reference.purpose);
    button->set_halign(Gtk::Align::FILL);
    button->signal_clicked().connect(
        [this, id = reference.id] { select_editorial_reference(id); });
    attach_context_menu(*button, {{"Editar apresentação",
                                   [this, id = reference.id] {
                                     select_editorial_reference(id);
                                     edit_editorial_reference();
                                   },
                                   "document-edit-symbolic"},
                                  {"Remover apresentação",
                                   [this, id = reference.id] {
                                     select_editorial_reference(id);
                                     delete_editorial_reference();
                                   },
                                   "user-trash-symbolic", true, true, true}});
    reference_list_.append(*button);
  }

  // A referência continua pertencendo ao filho que a declarou. Aqui fazemos
  // apenas uma projeção de leitura para o pai, preservando a origem para que
  // ninguém edite acidentalmente um vínculo em outro elemento.
  const auto nodes = service_.structural_nodes_for_work(*selected_work_id_);
  std::unordered_set<std::string> descendant_ids;
  std::vector<std::string> frontier{*selected_structural_node_id_};
  while (!frontier.empty()) {
    const auto parent_id = std::move(frontier.back());
    frontier.pop_back();
    for (const auto &node : nodes) {
      if (node.parent_id && *node.parent_id == parent_id &&
          descendant_ids.insert(node.id).second)
        frontier.push_back(node.id);
    }
  }
  if (descendant_ids.empty()) {
    inherited_references_hint_.set_text(
        "Este elemento ainda não possui partes internas com elementos da história apresentados.");
    return;
  }
  persistence::EditorialReferenceQuery all_references_query;
  all_references_query.work_id = *selected_work_id_;
  all_references_query.limit = 500;
  std::vector<project::EditorialEntityReference> inherited;
  for (std::size_t offset = 0;; offset += all_references_query.limit) {
    all_references_query.offset = offset;
    const auto page =
        service_.narrative().editorial_references(all_references_query);
    for (const auto &reference : page)
      if (descendant_ids.contains(reference.editorial_node_id))
        inherited.push_back(reference);
    if (page.size() < all_references_query.limit)
      break;
  }
  if (inherited.empty()) {
    inherited_references_hint_.set_text(
        "Nenhum elemento foi apresentado nos elementos internos.");
    return;
  }
  inherited_references_hint_.set_text("Projeção somente para leitura; edite a "
                                      "referência no elemento de origem.");
  for (const auto &reference : inherited) {
    const auto entity = service_.narrative().entity(reference.entity_id);
    const auto origin = std::find_if(
        nodes.begin(), nodes.end(), [&reference](const auto &node) {
          return node.id == reference.editorial_node_id;
        });
    const auto origin_name =
        origin == nodes.end()
            ? "Elemento de origem indisponível"
            : project::structural_node_position_label(*origin) + " — " +
                  origin->title;
    auto *button = Gtk::make_managed<Gtk::Button>(
        (entity ? entity->name : "Elemento da história ausente") + " — " +
        reference.purpose + "\nApresentada em: " + origin_name);
    button->set_halign(Gtk::Align::FILL);
    button->set_tooltip_text("Abrir elemento de origem: " + origin_name);
    button->signal_clicked().connect([this, id = reference.editorial_node_id] {
      selected_structural_node_id_ = id;
      refresh();
      show_detail_page();
    });
    inherited_references_list_.append(*button);
  }
}

void StructuralEditor::select_editorial_reference(std::string id) {
  selected_editorial_reference_id_ = std::move(id);
  reference_edit_button_.set_sensitive(true);
  reference_remove_button_.set_sensitive(true);
  reference_actions_button_.set_sensitive(true);
}

void StructuralEditor::add_editorial_reference() {
  if (!selected_work_id_ || !selected_structural_node_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog("Apresentar elemento da história nesta unidade", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Adicionar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  const auto work_id = *selected_work_id_;
  auto *entity = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar elemento associado à Obra",
      [this, work_id](const std::string &search, std::size_t limit) {
        persistence::EntityQuery query;
        query.search = search;
        query.work_id = work_id;
        query.limit = std::min(limit, std::size_t{50});
        std::vector<IncrementalSelection> result;
        for (const auto &value : service_.narrative().entities(query))
          result.push_back(
              {value.id, value.name, "Elemento da história no área da consulta da Obra"});
        return result;
      },
      "Pesquise um elemento da história associado à obra");
  auto *purpose = Gtk::make_managed<Gtk::Entry>();
  purpose->set_placeholder_text(
      "Finalidade, por exemplo: aparece, mencionada, revelada");
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_placeholder_text("Notas editoriais");
  form->set_margin(16);
  append_labeled_form_field(*form, "Elemento da história no área da consulta da Obra", *entity,
                            "Pesquise e escolha o elemento da história a apresentar.");
  append_labeled_form_field(*form, "Finalidade da apresentação", *purpose);
  append_labeled_form_field(*form, "Notas editoriais", *notes);
  dialog->get_content_area()->append(*form);
  const auto node_id = *selected_structural_node_id_;
  dialog->signal_response().connect(
      [this, dialog, node_id, entity, purpose, notes](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            if (!entity->selected_id())
              throw std::runtime_error(
                  "Vincule um elemento da história à Obra no Planejamento primeiro");
            static_cast<void>(service_.narrative().add_editorial_reference(
                *entity->selected_id(), node_id, purpose->get_text(),
                notes->get_text()));
            refresh_editorial_references();
            signal_status_message_.emit("Elemento da história apresentada na unidade");
          } catch (const std::exception &error) {
            show_error("Não foi possível adicionar o elemento da história", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void StructuralEditor::edit_editorial_reference() {
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
  auto *dialog =
      new OverlayDialog("Editar apresentação editorial", *owner, true);
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
            refresh_editorial_references();
            signal_status_message_.emit("Apresentação editorial atualizada");
          } catch (const std::exception &error) {
            show_error("Não foi possível editar a apresentação", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void StructuralEditor::delete_editorial_reference() {
  if (!selected_editorial_reference_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  const auto id = *selected_editorial_reference_id_;
  auto *dialog = new OverlayDialog(
      *owner, "Remover este elemento da história do elemento do manuscrito?", false,
      Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.narrative().remove_editorial_reference(id);
        refresh_editorial_references();
        signal_status_message_.emit("Apresentação editorial removida");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover a apresentação", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

Gtk::Window *StructuralEditor::owner_window() {
  return dynamic_cast<Gtk::Window *>(get_root());
}

void StructuralEditor::show_error(const Glib::ustring &title,
                                  const std::exception &error) {
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog(*owner, author::diagnostic(error.what()), false, Gtk::MessageType::ERROR,
                        Gtk::ButtonsType::CLOSE, true);
  dialog->set_title(title);
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->show();
}

} // namespace inde::ui
