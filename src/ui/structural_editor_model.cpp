#include "inde/ui/structural_editor.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include "inde/ui/incremental_selector.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace inde::ui {

void StructuralEditor::setup_structure_model() {
  structure_root_model_ = Gio::ListStore<StructuralNodeItem>::create();
  structure_tree_model_ = Gtk::TreeListModel::create(
      structure_root_model_,
      sigc::mem_fun(*this, &StructuralEditor::create_child_model), false, true);
  structure_selection_ = Gtk::SingleSelection::create(structure_tree_model_);
  structure_selection_->set_autoselect(false);
  structure_selection_->set_can_unselect(true);
  structure_factory_ = Gtk::SignalListItemFactory::create();
  structure_factory_->signal_setup().connect(
      [this](const Glib::RefPtr<Gtk::ListItem> &list_item) {
        auto *expander = Gtk::make_managed<Gtk::TreeExpander>();
        auto *label = Gtk::make_managed<Gtk::Label>();
        label->set_halign(Gtk::Align::START);
        label->set_hexpand(true);
        expander->set_child(*label);
        list_item->set_child(*expander);
        auto select_bound_node = [this, item = list_item.get()] {
          auto row = item ? std::dynamic_pointer_cast<Gtk::TreeListRow>(
                                item->get_item())
                          : nullptr;
          auto bound = row ? std::dynamic_pointer_cast<StructuralNodeItem>(
                                 row->get_item())
                           : nullptr;
          if (bound)
            selected_structural_node_id_ = bound->node().id;
          return bound != nullptr;
        };
        attach_context_menu(
            *expander,
            {{"Novo elemento interno",
              [this, select_bound_node] {
                if (select_bound_node())
                  create_structural_node(selected_structural_node_id_);
              },
              "list-add-symbolic"},
             {"Editar elemento",
              [this, select_bound_node] {
                if (select_bound_node())
                  edit_structural_node(*selected_structural_node_id_);
              },
              "document-edit-symbolic"},
             {"Duplicar ramificação",
              [this, select_bound_node] {
                if (select_bound_node())
                  duplicate_selected_branch();
              },
              "edit-copy-symbolic"},
             {"Mover para cima",
              [this, select_bound_node] {
                if (!select_bound_node())
                  return;
                try {
                  service_.move_structural_node_up(
                      *selected_structural_node_id_);
                  refresh();
                } catch (const std::exception &error) {
                  show_error("Não foi possível reordenar", error);
                }
              },
              "go-up-symbolic", true, false, true},
             {"Mover para baixo",
              [this, select_bound_node] {
                if (!select_bound_node())
                  return;
                try {
                  service_.move_structural_node_down(
                      *selected_structural_node_id_);
                  refresh();
                } catch (const std::exception &error) {
                  show_error("Não foi possível reordenar", error);
                }
              },
              "go-down-symbolic"},
             {"Remover ramificação",
              [this, select_bound_node] {
                if (select_bound_node())
                  delete_structural_node(*selected_structural_node_id_);
              },
              "user-trash-symbolic", true, true, true}});
      });
  structure_factory_->signal_bind().connect(
      [](const Glib::RefPtr<Gtk::ListItem> &list_item) {
        auto row =
            std::dynamic_pointer_cast<Gtk::TreeListRow>(list_item->get_item());
        auto *expander =
            dynamic_cast<Gtk::TreeExpander *>(list_item->get_child());
        auto *label = expander
                          ? dynamic_cast<Gtk::Label *>(expander->get_child())
                          : nullptr;
        auto item =
            row ? std::dynamic_pointer_cast<StructuralNodeItem>(row->get_item())
                : nullptr;
        if (expander && row)
          expander->set_list_row(row);
        if (label && item)
          label->set_text(
              project::structural_node_position_label(item->node()) + " — " +
              item->node().title);
      });
  structure_list_.set_model(structure_selection_);
  structure_list_.set_factory(structure_factory_);
  structure_selection_->property_selected().signal_changed().connect(
      sigc::mem_fun(*this, &StructuralEditor::on_structure_selection_changed));
}

Glib::RefPtr<Gio::ListModel> StructuralEditor::create_child_model(
    const Glib::RefPtr<Glib::ObjectBase> &object) {
  auto parent = std::dynamic_pointer_cast<StructuralNodeItem>(object);
  if (!parent)
    return {};
  auto children = Gio::ListStore<StructuralNodeItem>::create();
  for (const auto &node : structure_model_nodes_) {
    if (node.parent_id && *node.parent_id == parent->node().id &&
        is_structure_node_visible(node))
      children->append(StructuralNodeItem::create(node));
  }
  return children->get_n_items() == 0 ? Glib::RefPtr<Gio::ListModel>{}
                                      : children;
}

bool StructuralEditor::is_structure_node_visible(
    const project::StructuralNode &node) const {
  const bool matches_entity_type =
      selected_entity_type_ids_.empty() ||
      matching_editorial_node_ids_.contains(node.id);
  const auto structural_type = structural_type_filter_.get_active_id().raw();
  const bool matches_structural_type =
      structural_type.empty() || structural_type == "all" ||
      node.structural_type_id == structural_type;
  const auto status = status_filter_.get_active_id().raw();
  const bool matches_status =
      status.empty() || status == "all" || node.status == status;
  bool matches_text = true;
  if (!structure_filter_.empty()) {
    const auto needle = Glib::ustring(structure_filter_).lowercase();
    const auto text =
        Glib::ustring(project::structural_node_type_name(node) + " " +
                      node.designator + " " + node.title + " " + node.subtitle +
                      " " + node.synopsis + " " + node.status)
            .lowercase();
    matches_text = text.find(needle) != Glib::ustring::npos;
  }
  if (matches_text && matches_entity_type && matches_structural_type &&
      matches_status)
    return true;
  for (const auto &child : structure_model_nodes_) {
    if (child.parent_id && *child.parent_id == node.id &&
        is_structure_node_visible(child))
      return true;
  }
  return false;
}

void StructuralEditor::on_structure_selection_changed() {
  // Refiltrar reconstrói o modelo e pode reemitir a seleção anterior. Isso não
  // é uma intenção de abrir o detalhe nem deve roubar o foco da busca.
  if (refreshing_structure_model_)
    return;
  auto row = structure_selection_
                 ? std::dynamic_pointer_cast<Gtk::TreeListRow>(
                       structure_selection_->get_selected_item())
                 : nullptr;
  auto item =
      row ? std::dynamic_pointer_cast<StructuralNodeItem>(row->get_item())
          : nullptr;
  if (item)
    selected_structural_node_id_ = item->node().id;
  else
    selected_structural_node_id_.reset();
  const bool selected = selected_structural_node_id_.has_value();
  structure_add_child_button_.set_sensitive(selected);
  structure_up_button_.set_sensitive(selected);
  structure_down_button_.set_sensitive(selected);
  structure_edit_button_.set_sensitive(selected);
  structure_remove_button_.set_sensitive(selected);
  structure_duplicate_button_.set_sensitive(selected);
  structure_actions_button_.set_sensitive(selected);
  update_inspector();
  if (selected)
    show_detail_page();
}

} // namespace inde::ui
