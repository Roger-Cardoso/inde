#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/narrative_structure_workspace.hpp"
#include "narrative_structure_ui_helpers.hpp"

#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>

namespace inde::ui {
using narrative_structure_ui_detail::clear;

NarrativeStructureWorkspace::NarrativeStructureWorkspace(
    application::ProjectService &service)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 10), service_(service) {
  build_ui();
}

Gtk::Window *NarrativeStructureWorkspace::owner_window() {
  return dynamic_cast<Gtk::Window *>(get_root());
}
std::string NarrativeStructureWorkspace::selected_work_id() const {
  return work_combo_.get_active_id().raw();
}
void NarrativeStructureWorkspace::reveal_removal_use(
    const project::RemovalUse &use) {
  refresh_works();
  if (!use.work_id.empty() && !work_combo_.set_active_id(use.work_id))
    throw std::runtime_error("Obra do uso não encontrada");
  auto select = [](Gtk::ListBox &list, const auto &ids, const std::string &id) {
    for (std::size_t i = 0; i < ids.size(); ++i)
      if (ids[i] == id) {
        list.select_row(*list.get_row_at_index(static_cast<int>(i)));
        return;
      }
  };
  if (use.location_table == "narrative_roles") {
    show_roles();
    refresh_roles();
    select(roles_list_, role_row_ids_, use.location_id);
    if (use.table == "narrative_role_assignments")
      select(assignments_list_, assignment_row_ids_, use.record_id);
  } else if (use.location_table == "structure_models") {
    show_library();
    refresh_library();
    select(models_list_, model_row_ids_, use.location_id);
    models_button_.popup();
  } else {
    const auto structure = use.location_table == "narrative_structures"
                               ? use.location_id
                               : use.structure_id;
    if (structure.empty())
      throw std::runtime_error("Estrutura de origem não encontrada");
    show_detail(structure);
    if (use.location_table == "narrative_units")
      select(units_list_, unit_row_ids_, use.location_id);
    if (use.location_table == "narrative_units")
      show_unit();
    if (use.location_table == "narrative_lines")
      select(lines_list_, line_row_ids_, use.location_id);
  }
}
void NarrativeStructureWorkspace::refresh_works() {
  refreshing_works_ = true;
  const auto wanted = selected_work_id();
  work_combo_.remove_all();
  for (const auto &work : service_.catalog().works)
    work_combo_.append(work.id, work.title);
  if (!wanted.empty())
    work_combo_.set_active_id(wanted);
  if (work_combo_.get_active_row_number() < 0 &&
      !service_.catalog().works.empty())
    work_combo_.set_active(0);
  const bool has_work = !selected_work_id().empty();
  create_button_.set_sensitive(service_.current() != nullptr);
  work_combo_.set_visible(has_work);
  refreshing_works_ = false;
}
void NarrativeStructureWorkspace::refresh() {
  if (!service_.current()) {
    reset();
    return;
  }
  refresh_works();
  const auto structure = selected_structure_id_;
  refresh_library();
  selected_structure_id_ = structure;
  refresh_roles();
  if ((stack_.get_visible_child_name() == "detail" ||
       stack_.get_visible_child_name() == "unit") &&
      selected_structure_id_)
    refresh_detail();
  if (stack_.get_visible_child_name() == "comparison") {
    try {
      comparison_ =
          comparison_source_.empty()
              ? service_.builder().compare_to_origin(comparison_target_)
              : service_.builder().compare(comparison_source_,
                                           comparison_target_);
      render_comparison();
    } catch (const std::exception &e) {
      comparison_ = {};
      show_library();
      show_error(e);
    }
  }
  if (stack_.get_visible_child_name() == "continuity")
    show_continuity(); // never leave a derived approval/evidence stale after undo
}
void NarrativeStructureWorkspace::reset() {
  comparison_checks_.clear();
  selection_source_.clear();
  selection_source_name_.clear();
  selection_targets_.clear();
  comparison_targets_.clear();
  selection_bar_.set_visible(false);
  models_popover_.popdown();
  structures_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  structures_list_.set_activate_on_single_click(false);
  work_combo_.remove_all();
  create_button_.set_sensitive(false);
  clear(models_list_);
  clear(structures_list_);
  clear(lines_list_);
  clear(units_list_);
  clear(links_list_);
  clear(roles_list_);
  clear(assignments_list_);
  selected_structure_id_.reset();
  selected_model_id_.reset();
  selected_line_id_.reset();
  selected_unit_id_.reset();
  selected_link_id_.reset();
  selected_role_id_.reset();
  selected_assignment_id_.reset();
  builder_snapshot_ = {};
  comparison_ = {};
  comparison_source_.clear();
  comparison_target_.clear();
  comparison_page_index_ = 0;
  displayed_unit_id_.clear();
  show_library();
}
void NarrativeStructureWorkspace::show_library() {
  stack_.set_visible_child("library");
  library_button_.add_css_class("suggested-action");
  roles_button_.remove_css_class("suggested-action");
}
void NarrativeStructureWorkspace::show_roles() {
  cancel_comparison_selection();
  stack_.set_visible_child("roles");
  roles_button_.add_css_class("suggested-action");
  library_button_.remove_css_class("suggested-action");
  refresh_roles();
}
void NarrativeStructureWorkspace::show_detail(std::string id) {
  cancel_comparison_selection();
  selected_structure_id_ = std::move(id);
  stack_.set_visible_child("detail");
  library_button_.remove_css_class("suggested-action");
  roles_button_.remove_css_class("suggested-action");
  refresh_detail();
}

void NarrativeStructureWorkspace::show_error(const std::exception &error) {
  if (auto *owner = owner_window()) {
    auto *dialog =
        new OverlayDialog(*owner, author::diagnostic(error.what()), false, Gtk::MessageType::ERROR,
                          Gtk::ButtonsType::CLOSE, true);
    dialog->present();
  }
}

} // namespace inde::ui
