#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/project_operations.hpp"

#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include "inde/project/manifest.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "writing_workspace_util.hpp"

namespace inde::ui {

persistence::DocumentQuery WritingWorkspace::library_query() const {
  persistence::DocumentQuery query;
  query.search = search_.get_text();
  query.limit = 200;
  const auto placement = placement_filter_.get_active_id().raw();
  if (!placement.empty() && placement != "all")
    query.editorial_node_id = placement;
  if (entity_filter_)
    query.entity_id = entity_filter_->selected_id();
  if (current_group_id_)
    query.group_id = current_group_id_;
  else {
    const auto group = group_filter_.get_active_id().raw();
    if (!group.empty() && group != "all")
      query.group_id = group;
  }
  const auto purpose = purpose_filter_.get_active_id().raw();
  if (!purpose.empty() && purpose != "all")
    query.purpose = project::document_purpose_from_string(purpose);
  query.perspective = perspective_filter_.get_text();
  query.revisions_only = revisions_filter_.get_active();
  // Na raiz, grupos substituem visualmente seus filhos. Em pesquisas e
  // filtros, porém, todos os Documentos voltam a ser encontráveis.
  query.ungrouped_only = !current_group_id_ && query.search.empty() &&
                         !query.editorial_node_id && !query.entity_id &&
                         !query.group_id && !query.purpose &&
                         query.perspective.empty() && !query.revisions_only;
  return query;
}

void WritingWorkspace::refresh_library() {
  if (!service_.current())
    return;
  refresh_recovery_notice();
  const auto query = library_query();
  document_groups_ = service_.writing().document_groups();
  const auto active_group =
      current_group_id_
          ? std::find_if(document_groups_.begin(), document_groups_.end(),
                         [&](const auto &value) {
                           return value.id == *current_group_id_;
                         })
          : document_groups_.end();
  if (current_group_id_ && active_group == document_groups_.end())
    current_group_id_.reset();
  library_title_.set_text(current_group_id_ &&
                                  active_group != document_groups_.end()
                              ? active_group->name
                              : "Documentos");
  leave_group_button_.set_visible(current_group_id_.has_value());
  group_actions_button_.set_visible(current_group_id_.has_value());
  group_actions_button_.set_sensitive(current_group_id_.has_value());
  new_group_button_.set_visible(!current_group_id_.has_value());
  documents_ = service_.writing().document_summaries(query);
  auto total = service_.writing().document_count(query);
  if (query.ungrouped_only) {
    persistence::DocumentQuery all_documents;
    all_documents.limit = 1;
    total = service_.writing().document_count(all_documents);
  }
  auto summary = std::to_string(total) +
                 (total == 1 ? " Documento" : " Documentos") +
                 (query.search.empty() ? " no contexto" : " encontrados");
  if (!query.ungrouped_only && total > documents_.size())
    summary +=
        " — mostrando os " + std::to_string(documents_.size()) + " primeiros";
  library_summary_.set_text(summary);
  std::vector<std::string> context;
  if (!query.search.empty())
    context.push_back("texto “" + query.search + "”");
  if (query.editorial_node_id)
    context.push_back(placement_label(query.editorial_node_id));
  if (query.entity_id)
    context.push_back("elemento da história " + entity_label(*query.entity_id));
  if (query.group_id) {
    const auto group = std::find_if(
        document_groups_.begin(), document_groups_.end(),
        [&](const auto &value) { return value.id == *query.group_id; });
    context.push_back("grupo " + (group == document_groups_.end()
                                      ? std::string{"indisponível"}
                                      : group->name));
  }
  if (query.purpose)
    context.push_back(project::display_name(*query.purpose));
  if (!query.perspective.empty())
    context.push_back("perspectiva “" + query.perspective + "”");
  if (query.revisions_only)
    context.push_back("somente versões de revisão dos documentos");
  std::string context_text = context.empty() ? "Todo o Projeto" : "Filtros: ";
  for (std::size_t index = 0; index < context.size(); ++index) {
    if (index != 0)
      context_text += "  •  ";
    context_text += context[index];
  }
  library_context_.set_text(context_text);

  while (auto *child = document_list_.get_first_child())
    document_list_.remove(*child);
  const bool show_groups = !current_group_id_ && query.search.empty() &&
                           !query.editorial_node_id && !query.entity_id &&
                           !query.purpose && query.perspective.empty() &&
                           !query.revisions_only;
  if (show_groups) {
    for (const auto &group : document_groups_) {
      persistence::DocumentQuery count_query;
      count_query.group_id = group.id;
      const auto count = service_.writing().document_count(count_query);
      auto *button = Gtk::make_managed<Gtk::Button>();
      auto *content =
          Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 7);
      content->set_margin(14);
      auto *kind = Gtk::make_managed<Gtk::Label>("GRUPO DE DOCUMENTOS");
      kind->set_xalign(0.0F);
      kind->add_css_class("accent-label");
      auto *title = Gtk::make_managed<Gtk::Label>(group.name);
      title->set_xalign(0.0F);
      title->set_ellipsize(Pango::EllipsizeMode::END);
      title->add_css_class("title-3");
      auto *description = Gtk::make_managed<Gtk::Label>(
          group.description.empty() ? "Abra para ver somente este conjunto."
                                    : group.description);
      description->set_xalign(0.0F);
      description->set_wrap(true);
      description->set_lines(2);
      description->set_ellipsize(Pango::EllipsizeMode::END);
      description->add_css_class("dim-label");
      auto *details = Gtk::make_managed<Gtk::Label>(
          std::to_string(count) + (count == 1 ? " Documento" : " Documentos"));
      details->set_xalign(0.0F);
      details->add_css_class("dim-label");
      content->append(*kind);
      content->append(*title);
      content->append(*description);
      content->append(*details);
      button->set_child(*content);
      button->set_size_request(270, 154);
      button->set_hexpand(true);
      button->set_halign(Gtk::Align::FILL);
      button->set_valign(Gtk::Align::START);
      button->add_css_class("content-card");
      button->add_css_class("document-group-card");
      button->signal_clicked().connect(
          [this, id = group.id] { open_document_group(id); });
      attach_context_menu(
          *button,
          {{"Abrir grupo", [this, id = group.id] { open_document_group(id); },
            "folder-open-symbolic"},
           {"Editar grupo",
            [this, id = group.id] {
              open_document_group(id);
              edit_current_group();
            },
            "document-edit-symbolic"},
           {"Remover grupo",
            [this, id = group.id] {
              open_document_group(id);
              delete_current_group();
            },
            "user-trash-symbolic", true, true, true}});
      document_list_.append(*button);
    }
  }
  for (const auto &document : documents_) {
    auto *button = Gtk::make_managed<Gtk::Button>();
    auto *content = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    content->set_margin(12);
    auto *title = Gtk::make_managed<Gtk::Label>(document.title);
    title->set_xalign(0.0F);
    title->set_ellipsize(Pango::EllipsizeMode::END);
    title->set_max_width_chars(34);
    title->add_css_class("heading");
    auto *placement = Gtk::make_managed<Gtk::Label>(
        placement_label(document.editorial_node_id));
    placement->set_xalign(0.0F);
    placement->set_ellipsize(Pango::EllipsizeMode::END);
    placement->set_max_width_chars(34);
    placement->add_css_class("dim-label");
    auto *details = Gtk::make_managed<Gtk::Label>(
        std::to_string(document.character_count) +
        (document.character_count == 1 ? " caractere" : " caracteres") +
        "  •  " + std::to_string(document.entity_reference_count) +
        (document.entity_reference_count == 1 ? " elemento da história" : " elementos da história"));
    details->set_xalign(0.0F);
    details->add_css_class("dim-label");
    content->append(*title);
    auto *purpose = Gtk::make_managed<Gtk::Label>(
        project::display_name(document.purpose) +
        (document.perspective.empty() ? "" : "  •  " + document.perspective));
    purpose->set_xalign(0.0F);
    purpose->set_ellipsize(Pango::EllipsizeMode::END);
    purpose->add_css_class("accent-label");
    content->append(*purpose);
    if (document.search_match != project::DocumentSearchMatch::None) {
      auto *reason = Gtk::make_managed<Gtk::Label>(
          "Encontrado por: " + search_match_label(document.search_match));
      reason->set_xalign(0.0F);
      reason->add_css_class("accent-label");
      content->append(*reason);
    }
    content->append(*placement);
    content->append(*details);
    button->set_child(*content);
    button->set_size_request(270, 154);
    button->set_halign(Gtk::Align::FILL);
    button->set_valign(Gtk::Align::START);
    button->set_hexpand(true);
    button->set_vexpand(false);
    button->add_css_class("catalog-card");
    auto tooltip =
        document.title + "\n" + placement_label(document.editorial_node_id);
    if (document.search_match != project::DocumentSearchMatch::None)
      tooltip += "\nIncluído porque a busca corresponde ao " +
                 search_match_label(document.search_match) + ".";
    button->set_tooltip_text(tooltip);
    button->signal_clicked().connect(
        [this, id = document.id] { open_document(id); });
    attach_context_menu(
        *button,
        {{"Abrir Documento", [this, id = document.id] { open_document(id); },
          "go-next-symbolic"},
         {"Remover Documento",
          [this, id = document.id] {
            open_document(id);
            if (current_document_ && current_document_->id == id)
              delete_document();
          },
          "user-trash-symbolic", true, true, true}});
    document_list_.append(*button);
  }
  const bool empty =
      documents_.empty() && (!show_groups || document_groups_.empty());
  library_results_stack_.set_visible_child(empty ? "empty" : "results");
  if (documents_.empty() &&
      (!query.search.empty() || query.editorial_node_id || query.entity_id)) {
    empty_title_.set_text("Nenhum Documento encontrado");
    empty_message_.set_text(
        "Ajuste a pesquisa ou limpe os filtros para voltar à Biblioteca.");
  } else {
    empty_title_.set_text("Comece a escrever");
    empty_message_.set_text(
        "Crie um Documento livre agora. Você pode situá-lo na organização do manuscrito quando isso for útil.");
  }
}

void WritingWorkspace::refresh_library_filter_options() {
  refreshing_library_controls_ = true;
  const auto active = placement_filter_.get_active_id().raw();
  placement_filter_.remove_all();
  placement_filter_.append("all", "Todas as posições no manuscrito");
  for (const auto &[id, label] : placement_options_)
    placement_filter_.append(id, label);
  if (active.empty() || !placement_filter_.set_active_id(active))
    placement_filter_.set_active_id("all");
  const auto active_group = group_filter_.get_active_id().raw();
  document_groups_ = service_.writing().document_groups();
  group_filter_.remove_all();
  group_filter_.append("all", "Todos os grupos");
  for (const auto &group : document_groups_)
    group_filter_.append(group.id, group.name);
  if (active_group.empty() || !group_filter_.set_active_id(active_group))
    group_filter_.set_active_id("all");
  if (entity_filter_)
    entity_filter_->refresh();
  refreshing_library_controls_ = false;
}

void WritingWorkspace::create_document_group() {
  auto *owner = owner_window();
  if (!owner || !service_.current())
    return;
  auto *dialog = new OverlayDialog("Novo grupo de Documentos", *owner, true);
  dialog->set_secondary_text("O grupo organiza a Biblioteca dentro do INDE; "
                             "não cria uma pasta no disco.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar grupo", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  auto *description = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome do grupo");
  description->set_placeholder_text("Descrição opcional");
  form->set_margin(16);
  append_labeled_form_field(*form, "Nome", *name, "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, name, description](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            const auto group = service_.writing().create_document_group(
                name->get_text(), description->get_text());
            dialog->hide();
            refresh_library_filter_options();
            open_document_group(group.id);
            return;
          } catch (const std::exception &error) {
            show_error("Não foi possível criar o grupo", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void WritingWorkspace::edit_current_group() {
  if (!current_group_id_)
    return;
  const auto group = service_.writing().document_group(*current_group_id_);
  auto *owner = owner_window();
  if (!group || !owner)
    return;
  auto original = *group;
  auto *dialog = new OverlayDialog("Editar grupo de Documentos", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  auto *description = Gtk::make_managed<Gtk::Entry>();
  name->set_text(original.name);
  description->set_text(original.description);
  form->set_margin(16);
  append_labeled_form_field(*form, "Nome", *name, "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, name,
                                     description](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        original.name = name->get_text();
        original.description = description->get_text();
        static_cast<void>(service_.writing().update_document_group(original));
        refresh_library_filter_options();
        refresh_library();
      } catch (const std::exception &error) {
        show_error("Não foi possível editar o grupo", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void WritingWorkspace::delete_current_group() {
  if(!current_group_id_ || !owner_window())return;
  try {
    flush_changes();
    confirm_project_removal(*owner_window(),service_,project::RemovalKind::DocumentGroup,*current_group_id_,[this]{
      reset();refresh();signal_status_message_.emit("Grupo removido; textos preservados e operação registrada no histórico");
    });
  }catch(const std::exception &e){show_error("Não foi possível preparar a remoção",e);}
}

void WritingWorkspace::open_document_group(const std::string &id) {
  current_group_id_ = id;
  search_.set_text("");
  group_filter_.set_active_id("all");
  refresh_library();
  library_scroll_.get_vadjustment()->set_value(0.0);
  signal_status_message_.emit("Grupo documental aberto na Biblioteca");
}

void WritingWorkspace::leave_document_group() {
  current_group_id_.reset();
  refresh_library();
  library_scroll_.get_vadjustment()->set_value(0.0);
  new_group_button_.grab_focus();
}

void WritingWorkspace::toggle_library_filters() {
  library_filters_visible_ = !library_filters_visible_;
  set_overlay_revealer_open(library_filters_revealer_,
                            library_filters_visible_);
  library_filters_button_.set_label(
      library_filters_visible_ ? "Fechar filtros" : "Pesquisa e filtros");
  if (library_filters_visible_)
    search_.grab_focus();
}

void WritingWorkspace::dismiss_library_filters() {
  if (!library_filters_visible_)
    return;
  library_filters_visible_ = false;
  set_overlay_revealer_open(library_filters_revealer_, false);
  library_filters_button_.set_label("Pesquisa e filtros");
  library_filters_button_.grab_focus();
}

void WritingWorkspace::clear_library_filters() {
  search_.set_text("");
  placement_filter_.set_active_id("all");
  group_filter_.set_active_id("all");
  purpose_filter_.set_active_id("all");
  perspective_filter_.set_text("");
  revisions_filter_.set_active(false);
  if (entity_filter_)
    entity_filter_->clear_selection();
  refresh_library();
}

void WritingWorkspace::refresh_placement_options() {
  placement_options_.clear();
  placement_labels_.clear();
  for (const auto &work : service_.catalog().works) {
    const auto nodes = service_.structural_nodes_for_work(work.id);
    const auto path_labels = project::structural_node_path_labels(nodes);
    for (const auto &node : nodes) {
      auto label = work.title + " — " + path_labels.at(node.id);
      placement_options_.emplace_back(node.id, label);
      placement_labels_.emplace(node.id, std::move(label));
    }
  }
  const auto active = placement_.get_active_id().raw();
  const bool previous_loading = loading_editor_;
  loading_editor_ = true;
  placement_.remove_all();
  placement_.append("none", "Sem posição no manuscrito");
  for (const auto &[id, label] : placement_options_)
    placement_.append(id, label);
  placement_.set_active_id(active.empty() ? "none" : active);
  if (placement_.get_active_row_number() < 0)
    placement_.set_active_id("none");
  loading_editor_ = previous_loading;
}

} // namespace inde::ui
