#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/narrative_structure_workspace.hpp"
#include "inde/ui/work_setup.hpp"
#include "narrative_structure_ui_helpers.hpp"
#include <algorithm>
namespace inde::ui {
void NarrativeStructureWorkspace::start_work() {
  if (auto *owner = owner_window())
    show_work_setup(*owner, service_, [this](const std::string &id) {
      refresh_works();
      work_combo_.set_active_id(id);
      refresh_library();
      signal_catalog_changed_.emit();
      create_button_.grab_focus();
    });
}
void NarrativeStructureWorkspace::add_model_row(
    const project::StructureModel &v) {
  auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  auto *content = narrative_structure_ui_detail::row_content(
      v.name, std::string(v.is_builtin ? "Sistema" : "Usuário") + " · " +
                  v.description);
  content->set_hexpand(true);
  row->append(*content);
  auto *menu = Gtk::make_managed<Gtk::MenuButton>();
  menu->set_valign(Gtk::Align::START);
  row->append(*menu);
  configure_overflow_button(
      *menu,
      {{"Aplicar",
        [this, id = v.id] {
          models_popover_.popdown();
          selected_model_id_ = id;
          instantiate_model();
        },
        "list-add-symbolic"},
       {"Remover",
        [this, id = v.id] {
          models_popover_.popdown();
          selected_model_id_ = id;
          delete_model();
        },
        "user-trash-symbolic", !v.is_builtin, true, true}},
      "Ações do modelo: " + v.name);
  models_list_.append(*row);
  model_row_ids_.push_back(v.id);
}
void NarrativeStructureWorkspace::add_structure_row(
    const project::NarrativeStructure &v, const std::string &description) {
  auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  auto *check = Gtk::make_managed<Gtk::CheckButton>();
  check->set_valign(Gtk::Align::CENTER);
  check->set_visible(!selection_source_.empty());
  check->set_sensitive(v.id != selection_source_);
  check->set_active(std::find(selection_targets_.begin(),
                              selection_targets_.end(),
                              v.id) != selection_targets_.end());
  set_accessible_label(*check, "Comparar com " + v.name);
  row->append(*check);
  comparison_checks_[v.id] = check;
  check->signal_toggled().connect(sigc::track_obj(
      [this, id = v.id] {
        if (!updating_selection_)
          toggle_comparison_target(id);
      },
      *check));
  auto *content = narrative_structure_ui_detail::row_content(
      v.name, (v.id == selection_source_ ? "ORIGEM DA COMPARAÇÃO · " : "") +
                  description);
  content->set_hexpand(true);
  row->append(*content);
  auto *menu = Gtk::make_managed<Gtk::MenuButton>();
  menu->set_valign(Gtk::Align::START);
  menu->set_sensitive(selection_source_.empty());
  row->append(*menu);
  auto action = [this, id = v.id](auto function) {
    return [this, id, function] {
      selected_structure_id_ = id;
      function();
    };
  };
  configure_overflow_button(
      *menu,
      {{"Comparar com…", [this, id = v.id] { start_comparison_selection(id); },
        "view-dual-symbolic"},
       {"Duplicar", action([this] { duplicate_selected(false); }),
        "edit-copy-symbolic"},
       {"Criar alternativa", action([this] { duplicate_selected(true); }),
        "list-add-symbolic"},
       {"Usar como plano atual", action([this] { activate_selected(); }), "emblem-ok-symbolic",
        !v.is_active},
       {"Salvar como modelo de planejamento", action([this] { capture_model(); }),
        "document-save-symbolic"},
       {"Remover", action([this] { delete_selected(); }), "user-trash-symbolic",
        true, true, true}},
      "Ações do plano: " + v.name);
  structures_list_.append(*row);
  structure_row_ids_.push_back(v.id);
}
void NarrativeStructureWorkspace::start_comparison_selection(
    const std::string &id) {
  try {
    selection_source_name_ = service_.builder().snapshot(id).name;
  } catch (const std::exception &e) {
    show_error(e);
    return;
  }
  selection_source_ = id;
  selection_targets_.clear();
  selected_structure_id_ = id;
  structures_list_.set_selection_mode(Gtk::SelectionMode::NONE);
  structures_list_.set_activate_on_single_click(true);
  refresh_library();
  update_comparison_selection();
}
void NarrativeStructureWorkspace::toggle_comparison_target(
    const std::string &id) {
  if (selection_source_.empty() || id == selection_source_)
    return;
  auto at = std::find(selection_targets_.begin(), selection_targets_.end(), id);
  if (at != selection_targets_.end())
    selection_targets_.erase(at);
  else if (selection_targets_.size() < 8)
    selection_targets_.push_back(id);
  else
    signal_status_message_.emit("Compare até oito alvos por consulta; "
                                "desmarque um para escolher outro.");
  update_comparison_selection();
}
void NarrativeStructureWorkspace::update_comparison_selection() {
  const bool active = !selection_source_.empty();
  selection_bar_.set_visible(active);
  structures_scroll_.set_margin_bottom(active ? 100 : 0);
  library_help_.set_visible(!active);
  selection_compare_.set_sensitive(!selection_targets_.empty());
  selection_hint_.set_tooltip_text(selection_source_name_);
  const auto name=Glib::ustring(selection_source_name_);
  selection_hint_.set_text("Origem: " + name.substr(0,70).raw() + (name.size()>70?"…":"") +
                           ". Clique nos alvos ou marque as caixas. " +
                           std::to_string(selection_targets_.size()) +
                           " selecionados (até 8). Nenhum dado será alterado.");
  selection_origin_.set_visible(false);
  if (active)
    try {
      selection_origin_.set_visible(!service_.builder()
                                         .snapshot(selection_source_)
                                         .provenance_json.empty());
    } catch (const std::exception &) {
    }
  updating_selection_ = true;
  for (auto &[id, check] : comparison_checks_)
    check->set_active(std::find(selection_targets_.begin(),
                                selection_targets_.end(),
                                id) != selection_targets_.end());
  updating_selection_ = false;
}
void NarrativeStructureWorkspace::cancel_comparison_selection() {
  if (selection_source_.empty())
    return;
  selected_structure_id_ = selection_source_;
  selection_source_.clear();
  selection_source_name_.clear();
  selection_targets_.clear();
  structures_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  structures_list_.set_activate_on_single_click(false);
  if (service_.current())
    refresh_library();
  update_comparison_selection();
}
void NarrativeStructureWorkspace::confirm_comparison_selection() {
  if (selection_source_.empty() || selection_targets_.empty())
    return;
  try {
    // Compute every selected target first: failure cannot masquerade as a
    // complete comparison.
    std::vector<application::BuilderComparison> results;
    for (const auto &id : selection_targets_)
      results.push_back(service_.builder().compare(selection_source_, id));
    comparison_source_ = selection_source_;
    comparison_targets_ = selection_targets_;
    comparison_target_ = comparison_targets_.front();
    comparison_ = std::move(results.front());
    comparison_page_index_ = 0;
    changing_comparison_target_ = true;
    comparison_target_combo_.remove_all();
    for (std::size_t i = 0; i < comparison_targets_.size(); ++i)
      comparison_target_combo_.append(comparison_targets_[i],
                                      i == 0 ? comparison_.target_name
                                             : results[i].target_name);
    comparison_target_combo_.set_active(0);
    changing_comparison_target_ = false;
    comparison_target_combo_.set_visible(comparison_targets_.size() > 1);
    cancel_comparison_selection();
    render_comparison();
    stack_.set_visible_child("comparison");
  } catch (const std::exception &e) {
    changing_comparison_target_ = false;
    show_error(e);
  }
}
} // namespace inde::ui
