#include "inde/ui/structural_editor.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include "inde/ui/incremental_selector.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace inde::ui {

void StructuralEditor::show_structure_page() {
  page_stack_.set_visible_child("structure");
  filters_button_.grab_focus();
}

void StructuralEditor::show_structure() { show_structure_page(); }

void StructuralEditor::reveal_node(const std::string &node_id) {
  if (!selected_work_id_)
    return;
  const auto nodes = service_.structural_nodes_for_work(*selected_work_id_);
  const auto found =
      std::find_if(nodes.begin(), nodes.end(),
                   [&](const auto &node) { return node.id == node_id; });
  if (found == nodes.end())
    return;
  selected_structural_node_id_ = node_id;
  selected_editorial_reference_id_.reset();
  update_inspector();
  show_detail_page();
  signal_status_message_.emit("Elemento do manuscrito aberta a partir da Escrita");
}

void StructuralEditor::show_detail_page() {
  if (!selected_structural_node_id_)
    return;
  dismiss_filters();
  detail_hint_.set_text(structure_hint_.get_text());
  page_stack_.set_visible_child("detail");
  back_to_structure_button_.grab_focus();
}

void StructuralEditor::dismiss_filters() {
  if (!filters_visible_)
    return;
  filters_visible_ = false;
  set_overlay_revealer_open(filters_revealer_, false);
  filters_button_.set_label("Filtros");
  filters_button_.grab_focus();
}

void StructuralEditor::refresh_filter_summary() {
  std::vector<std::string> filters;
  if (!structure_filter_.empty())
    filters.push_back("Busca: “" + structure_filter_ + "”");
  const auto structural_type = structural_type_filter_.get_active_id().raw();
  if (!structural_type.empty() && structural_type != "all")
    filters.push_back("Unidade: " + structural_type_filter_.get_active_text());
  const auto status = status_filter_.get_active_id().raw();
  if (!status.empty() && status != "all")
    filters.push_back("Status: " + status_filter_.get_active_text());
  if (!selected_entity_type_ids_.empty())
    filters.push_back(std::to_string(selected_entity_type_ids_.size()) +
                      " tipo(s) de elemento da história apresentada");
  structure_filter_summary_.set_text(
      filters.empty() ? "Toda a estrutura da Obra" : "Filtros: " + [&filters] {
        std::string result;
        for (std::size_t index = 0; index < filters.size(); ++index) {
          if (index != 0)
            result += "  •  ";
          result += filters[index];
        }
        return result;
      }());
}

void StructuralEditor::refresh_editorial_entity_type_facets() {
  while (auto *child = entity_type_facets_.get_first_child())
    entity_type_facets_.remove(*child);
  matching_editorial_node_ids_.clear();
  entity_type_facets_label_.set_text("Elementos da história apresentadas");
  if (!selected_work_id_)
    return;

  persistence::EditorialReferenceQuery facet_query;
  facet_query.work_id = *selected_work_id_;
  const auto facets =
      service_.narrative().editorial_reference_entity_type_facets(facet_query);
  const auto types = service_.narrative().entity_types();
  for (const auto &facet : facets) {
    const auto type =
        std::find_if(types.begin(), types.end(),
                     [&](const auto &value) { return value.id == facet.id; });
    if (type == types.end())
      continue;
    auto *check = Gtk::make_managed<Gtk::CheckButton>();
    auto *label = Gtk::make_managed<Gtk::Label>(
        type->name + " (" + std::to_string(facet.count) + ")");
    label->set_xalign(0.0F);
    label->set_ellipsize(Pango::EllipsizeMode::END);
    label->set_max_width_chars(34);
    check->set_child(*label);
    check->set_active(std::find(selected_entity_type_ids_.begin(),
                                selected_entity_type_ids_.end(),
                                facet.id) != selected_entity_type_ids_.end());
    check->set_tooltip_text(
        "Mostrar unidades que apresentam elementos da história deste tipo");
    check->signal_toggled().connect([this, id = facet.id, check] {
      const auto found = std::find(selected_entity_type_ids_.begin(),
                                   selected_entity_type_ids_.end(), id);
      if (check->get_active() && found == selected_entity_type_ids_.end())
        selected_entity_type_ids_.push_back(id);
      else if (!check->get_active() && found != selected_entity_type_ids_.end())
        selected_entity_type_ids_.erase(found);
      refresh();
    });
    entity_type_facets_.append(*check);
  }

  if (selected_entity_type_ids_.empty())
    return;
  persistence::EditorialReferenceQuery matching_query;
  matching_query.work_id = *selected_work_id_;
  matching_query.entity_type_ids = selected_entity_type_ids_;
  matching_query.limit = 500;
  while (true) {
    const auto references =
        service_.narrative().editorial_references(matching_query);
    for (const auto &reference : references)
      matching_editorial_node_ids_.insert(reference.editorial_node_id);
    if (references.size() < matching_query.limit)
      break;
    matching_query.offset += references.size();
  }
}

void StructuralEditor::install_actions(Gtk::ApplicationWindow &window) {
  window.add_action("create-root",
                    [this] { create_structural_node(std::nullopt); });
  window.add_action("create-child", [this] {
    if (selected_structural_node_id_)
      create_structural_node(selected_structural_node_id_);
  });
  window.add_action("move-up", [this] {
    if (!selected_structural_node_id_)
      return;
    try {
      service_.move_structural_node_up(*selected_structural_node_id_);
      refresh();
    } catch (const std::exception &error) {
      show_error("Não foi possível reordenar", error);
    }
  });
  window.add_action("move-down", [this] {
    if (!selected_structural_node_id_)
      return;
    try {
      service_.move_structural_node_down(*selected_structural_node_id_);
      refresh();
    } catch (const std::exception &error) {
      show_error("Não foi possível reordenar", error);
    }
  });
  window.add_action("edit-node", [this] {
    if (selected_structural_node_id_)
      edit_structural_node(*selected_structural_node_id_);
  });
  window.add_action("remove-node", [this] {
    if (selected_structural_node_id_)
      delete_structural_node(*selected_structural_node_id_);
  });
  window.add_action(
      "duplicate-node",
      sigc::mem_fun(*this, &StructuralEditor::duplicate_selected_branch));
  window.add_action(
      "create-template",
      sigc::mem_fun(*this, &StructuralEditor::create_selected_template));
  window.add_action("restore-deletion", [&window] {
    // Use the main-window barrier and full workspace refresh. A restoration
    // must not bypass the impact/acknowledgment workflow or replay old text.
    gtk_widget_activate_action(GTK_WIDGET(window.gobj()),
                               "win.project-operations", nullptr);
  });

  add_root_button_.set_action_name("win.create-root");
  structure_add_child_button_.set_action_name("win.create-child");
  structure_up_button_.set_action_name("win.move-up");
  structure_down_button_.set_action_name("win.move-down");
  structure_edit_button_.set_action_name("win.edit-node");
  structure_remove_button_.set_action_name("win.remove-node");
  inspector_edit_button_.set_action_name("win.edit-node");
  structure_duplicate_button_.set_action_name("win.duplicate-node");
  structure_template_button_.set_action_name("win.create-template");
  structure_restore_button_.set_action_name("win.restore-deletion");
}

void StructuralEditor::set_work(std::optional<std::string> work_id) {
  if (selected_work_id_ == work_id) {
    refresh();
    return;
  }
  selected_work_id_ = std::move(work_id);
  selected_structural_node_id_.reset();
  selected_editorial_reference_id_.reset();
  selected_entity_type_ids_.clear();
  matching_editorial_node_ids_.clear();
  structural_types_.clear();
  // Uma busca de outra obra não deve esconder o manuscrito recém-aberto.
  refreshing_filters_ = true;
  structure_filter_.clear();
  structure_search_.set_text("");
  structural_type_filter_.set_active_id("all");
  status_filter_.set_active_id("all");
  refreshing_filters_ = false;
  dismiss_filters();
  show_structure_page();
  refresh();
}

void StructuralEditor::reset() {
  capture_structure_view_state();
  selected_work_id_.reset();
  selected_structural_node_id_.reset();
  structure_filter_.clear();
  selected_entity_type_ids_.clear();
  matching_editorial_node_ids_.clear();
  structure_view_states_.clear();
  rendered_work_id_.reset();
  refreshing_filters_ = true;
  structure_search_.set_text("");
  structural_type_filter_.set_active_id("all");
  structural_type_filter_.remove_all();
  structural_type_filter_.append("all", "Todos os tipos de unidade");
  structural_type_manager_.remove_all();
  status_filter_.set_active_id("all");
  refreshing_filters_ = false;
  dismiss_filters();
  show_structure_page();
  refresh();
}

void StructuralEditor::toggle_inspector() {
  if (page_stack_.get_visible_child_name() == "detail") {
    show_structure_page();
    return;
  }
  show_detail_page();
}

void StructuralEditor::duplicate_selected_branch() {
  if (!selected_structural_node_id_)
    return;
  try {
    selected_structural_node_id_ =
        service_.duplicate_structural_branch(*selected_structural_node_id_);
    refresh();
    signal_status_message_.emit("Ramificação duplicada e salva");
  } catch (const std::exception &error) {
    show_error("Não foi possível duplicar a ramificação", error);
  }
}

void StructuralEditor::create_selected_template() {
  if (!selected_work_id_)
    return;
  try {
    service_.create_structural_template(
        *selected_work_id_, structure_template_combo_.get_active_id().raw());
    refresh();
    signal_status_message_.emit("Modelo de manuscrito criado e salvo");
  } catch (const std::exception &error) {
    show_error("Não foi possível aplicar o modelo", error);
  }
}

void StructuralEditor::refresh() {
  capture_structure_view_state();
  refresh_structural_types();
  refreshing_structure_model_ = true;
  structure_model_nodes_.clear();
  if (structure_root_model_)
    structure_root_model_->remove_all();
  add_root_button_.set_sensitive(selected_work_id_.has_value());
  structural_type_add_button_.set_sensitive(service_.current() != nullptr);
  structure_template_button_.set_sensitive(selected_work_id_.has_value());
  structure_template_combo_.set_sensitive(selected_work_id_.has_value());
  if (!selected_work_id_) {
    refresh_editorial_entity_type_facets();
    refresh_filter_summary();
    structure_stack_.set_visible_child("empty");
    selected_structural_node_id_.reset();
    rendered_work_id_.reset();
    refreshing_structure_model_ = false;
    update_inspector();
    return;
  }
  structure_stack_.set_visible_child("structure");
  const auto work = std::find_if(
      service_.catalog().works.begin(), service_.catalog().works.end(),
      [&](const auto &value) { return value.id == *selected_work_id_; });
  structure_hint_.set_text(
      work == service_.catalog().works.end() ? "Obra" : work->title);
  structure_model_nodes_ =
      service_.structural_nodes_for_work(*selected_work_id_);
  refresh_editorial_entity_type_facets();
  refresh_filter_summary();
  for (const auto &node : structure_model_nodes_) {
    if (!node.parent_id && is_structure_node_visible(node))
      structure_root_model_->append(StructuralNodeItem::create(node));
  }
  rendered_work_id_ = selected_work_id_;
  restore_structure_view_state();
  if (selected_structural_node_id_ && structure_tree_model_) {
    for (guint i = 0; i < structure_tree_model_->get_n_items(); ++i) {
      auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>(
          structure_tree_model_->get_object(i));
      auto item =
          row ? std::dynamic_pointer_cast<StructuralNodeItem>(row->get_item())
              : nullptr;
      if (item && item->node().id == *selected_structural_node_id_) {
        structure_selection_->set_selected(i);
        break;
      }
    }
  }
  const bool selected = selected_structural_node_id_.has_value();
  structure_add_child_button_.set_sensitive(selected);
  structure_up_button_.set_sensitive(selected);
  structure_down_button_.set_sensitive(selected);
  structure_edit_button_.set_sensitive(selected);
  structure_remove_button_.set_sensitive(selected);
  structure_duplicate_button_.set_sensitive(selected);
  structure_actions_button_.set_sensitive(selected);
  inspector_edit_button_.set_sensitive(selected);
  refreshing_structure_model_ = false;
  update_inspector();
}

void StructuralEditor::capture_structure_view_state() {
  if (!rendered_work_id_ || !structure_tree_model_)
    return;
  auto &state = structure_view_states_[*rendered_work_id_];
  for (guint index = 0; index < structure_tree_model_->get_n_items(); ++index) {
    auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>(
        structure_tree_model_->get_object(index));
    auto item =
        row ? std::dynamic_pointer_cast<StructuralNodeItem>(row->get_item())
            : nullptr;
    if (!row || !item)
      continue;
    if (row->get_expanded())
      state.expanded_node_ids.insert(item->node().id);
    else
      state.expanded_node_ids.erase(item->node().id);
  }
  if (const auto adjustment = structure_scroll_.get_vadjustment())
    state.scroll_value = adjustment->get_value();
  state.initialized = true;
}

void StructuralEditor::restore_structure_view_state() {
  if (!selected_work_id_ || !structure_tree_model_)
    return;
  const auto found = structure_view_states_.find(*selected_work_id_);
  if (found == structure_view_states_.end() || !found->second.initialized)
    return;

  std::vector<std::pair<Glib::RefPtr<Gtk::TreeListRow>, std::string>> rows;
  rows.reserve(structure_tree_model_->get_n_items());
  for (guint index = 0; index < structure_tree_model_->get_n_items(); ++index) {
    auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>(
        structure_tree_model_->get_object(index));
    auto item =
        row ? std::dynamic_pointer_cast<StructuralNodeItem>(row->get_item())
            : nullptr;
    if (row && item)
      rows.emplace_back(row, item->node().id);
  }
  // O modelo nasce expandido para preservar o comportamento inicial. Reaplicar
  // de baixo para cima evita que recolher um ancestral invalide linhas que
  // ainda precisam receber o estado anterior.
  for (auto iterator = rows.rbegin(); iterator != rows.rend(); ++iterator)
    iterator->first->set_expanded(
        found->second.expanded_node_ids.contains(iterator->second));

  const auto work_id = *selected_work_id_;
  const auto scroll_value = found->second.scroll_value;
  Glib::signal_idle().connect_once([this, work_id, scroll_value] {
    if (!selected_work_id_ || *selected_work_id_ != work_id)
      return;
    const auto adjustment = structure_scroll_.get_vadjustment();
    if (!adjustment)
      return;
    const auto maximum =
        std::max(adjustment->get_lower(),
                 adjustment->get_upper() - adjustment->get_page_size());
    adjustment->set_value(
        std::clamp(scroll_value, adjustment->get_lower(), maximum));
  });
}

} // namespace inde::ui
