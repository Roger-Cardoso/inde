#include "inde/ui/accessibility.hpp"
#include "inde/ui/bounded_choices.hpp"
#include "inde/ui/narrative_structure_workspace.hpp"
#include "inde/ui/temporal_context.hpp"

#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>

namespace inde::ui {

void NarrativeStructureWorkspace::build_ui() {
  set_hexpand(true);
  set_vexpand(true);
  title_.add_css_class("title-1");
  title_.set_halign(Gtk::Align::START);
  hint_.add_css_class("dim-label");
  hint_.set_halign(Gtk::Align::START);
  hint_.set_wrap(true);
  hint_.set_max_width_chars(80);
  heading_.set_hexpand(true);
  heading_.append(title_);
  heading_.append(hint_);
  header_.append(heading_);
  header_actions_.set_selection_mode(Gtk::SelectionMode::NONE);
  header_actions_.set_column_spacing(6);
  header_actions_.set_row_spacing(6);
  header_actions_.set_max_children_per_line(3);
  header_actions_.set_min_children_per_line(1);
  header_actions_.append(work_combo_);
  bound_choice_width(work_combo_);
  header_actions_.append(library_button_);
  header_actions_.append(roles_button_);
  header_.append(header_actions_);
  append(header_);
  stack_.set_hexpand(true);
  stack_.set_vexpand(true);
  stack_.set_hhomogeneous(false);
  stack_.set_vhomogeneous(false);
  detail_sections_.set_hhomogeneous(false);
  detail_sections_.set_vhomogeneous(false);
  roles_sections_.set_hhomogeneous(false);
  roles_sections_.set_vhomogeneous(false);
  stack_.set_transition_type(Gtk::StackTransitionType::SLIDE_LEFT_RIGHT);
  append(stack_);

  library_toolbar_.set_selection_mode(Gtk::SelectionMode::NONE);
  library_toolbar_.set_column_spacing(6);
  library_toolbar_.set_row_spacing(6);
  library_toolbar_.set_min_children_per_line(1);
  library_toolbar_.set_max_children_per_line(7);
  library_toolbar_.set_halign(Gtk::Align::FILL);
  library_toolbar_.append(create_button_);
  create_button_.set_label("Novo plano");
  models_button_.set_label("Modelos de planejamento");
  models_popover_.add_css_class("context-menu");
  models_popover_.set_has_arrow(false);
  models_button_.set_popover(models_popover_);
  library_toolbar_.append(models_button_);
  library_page_.append(library_toolbar_);
  library_help_.set_xalign(0);
  library_help_.set_wrap(true);
  library_help_.add_css_class("dim-label");
  library_page_.append(library_help_);
  setup_hint_.set_xalign(0);
  setup_hint_.set_wrap(true);
  setup_hint_.set_max_width_chars(72);
  setup_box_.append(setup_hint_);
  setup_button_.set_halign(Gtk::Align::START);
  setup_button_.add_css_class("suggested-action");
  setup_box_.append(setup_button_);
  library_page_.append(setup_box_);
  setup_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::start_work));

  auto *models_heading = Gtk::make_managed<Gtk::Label>("Modelos de planejamento");
  models_heading->set_halign(Gtk::Align::START);
  models_heading->add_css_class("heading");
  models_panel_.append(*models_heading);
  models_panel_.set_margin(12);
  auto *models_help = Gtk::make_managed<Gtk::Label>(
      "Esqueletos reutilizáveis, não alternativas desta Obra. Aplique pelo "
      "menu do modelo.");
  models_help->set_xalign(0);
  models_help->set_wrap(true);
  models_help->set_max_width_chars(46);
  models_panel_.append(*models_help);
  models_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  models_list_.add_css_class("boxed-list");
  models_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  models_scroll_.set_child(models_list_);
  models_scroll_.set_min_content_height(180);
  models_scroll_.set_max_content_height(360);
  models_scroll_.set_propagate_natural_height(true);
  models_scroll_.set_size_request(360, -1);
  models_panel_.append(models_scroll_);
  models_popover_.set_child(models_panel_);

  auto *structures_heading =
      Gtk::make_managed<Gtk::Label>("Alternativas desta Obra");
  structures_heading->set_halign(Gtk::Align::START);
  structures_heading->add_css_class("heading");
  structures_panel_.append(*structures_heading);
  structures_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  structures_list_.set_activate_on_single_click(false);
  structures_list_.add_css_class("boxed-list");
  structures_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                Gtk::PolicyType::AUTOMATIC);
  structures_scroll_.set_child(structures_list_);
  structures_scroll_.set_vexpand(true);
  structures_panel_.append(structures_scroll_);
  library_overlay_.set_child(structures_panel_);
  library_overlay_.set_vexpand(true);
  selection_bar_.set_halign(Gtk::Align::CENTER);
  selection_bar_.set_valign(Gtk::Align::END);
  selection_bar_.set_margin(12);
  selection_bar_.add_css_class("comparison-selection-bar");
  selection_hint_.set_wrap(true);
  selection_hint_.set_max_width_chars(70);
  selection_bar_.append(selection_hint_);
  selection_actions_.append(selection_cancel_);
  selection_actions_.append(selection_origin_);
  selection_actions_.append(selection_compare_);
  selection_compare_.add_css_class("suggested-action");
  selection_bar_.append(selection_actions_);
  library_overlay_.add_overlay(selection_bar_);
  selection_bar_.set_visible(false);
  selection_cancel_.signal_clicked().connect(sigc::mem_fun(
      *this, &NarrativeStructureWorkspace::cancel_comparison_selection));
  selection_compare_.signal_clicked().connect(sigc::mem_fun(
      *this, &NarrativeStructureWorkspace::confirm_comparison_selection));
  selection_origin_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::choose_comparison));
  library_page_.append(library_overlay_);
  stack_.add(library_page_, "library");

  detail_title_.set_hexpand(true);
  detail_title_.set_halign(Gtk::Align::START);
  detail_title_.set_ellipsize(Pango::EllipsizeMode::END);
  detail_title_.set_max_width_chars(60);
  detail_title_.add_css_class("title-2");
  detail_header_.append(detail_back_);
  detail_header_.set_selection_mode(Gtk::SelectionMode::NONE);
  detail_header_.set_column_spacing(6);
  detail_header_.set_row_spacing(6);
  detail_header_.set_min_children_per_line(1);
  detail_header_.set_max_children_per_line(2);
  detail_page_.append(detail_title_);
  order_combo_.append("narrative", "Ordem do plano narrativo");
  order_combo_.append("writing", "Ordem de escrita (opcional)");
  order_combo_.append("line", "Ordem nesta linha narrativa");
  order_combo_.set_active_id("narrative");
  order_combo_.set_tooltip_text("Só esta ordem será editada. Cronologia e "
                                "apresentação editorial são independentes.");
  detail_header_.append(order_combo_);
  bound_choice_width(order_combo_);
  order_combo_.signal_changed().connect([this] {
    if (selected_structure_id_)
      refresh_detail();
  });
  detail_page_.append(detail_header_);
  detail_sections_.set_vexpand(true);
  detail_sections_.set_hexpand(true);
  detail_switcher_.set_stack(detail_sections_);
  detail_page_.append(detail_switcher_);
  lines_toolbar_.append(add_line_button_);
  lines_toolbar_.append(edit_line_button_);
  lines_toolbar_.append(remove_line_button_);
  lines_panel_.append(lines_toolbar_);
  auto *line_heading = Gtk::make_managed<Gtk::Label>("Linhas narrativas");
  line_heading->set_halign(Gtk::Align::START);
  line_heading->add_css_class("heading");
  lines_panel_.prepend(*line_heading);
  lines_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  lines_list_.add_css_class("boxed-list");
  lines_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  lines_scroll_.set_child(lines_list_);
  lines_scroll_.set_vexpand(true);
  lines_panel_.append(lines_scroll_);
  auto *line_dates =
      Gtk::make_managed<Gtk::Button>("Datas da linha selecionada…");
  line_dates->set_halign(Gtk::Align::START);
  line_dates->set_sensitive(false);
  lines_panel_.append(*line_dates);
  line_dates->signal_clicked().connect([this] {
    if (selected_line_id_ && owner_window())
      show_temporal_context(*owner_window(), service_, "line",
                            *selected_line_id_);
  });
  lines_list_.signal_row_selected().connect(sigc::track_obj(
      [this, line_dates](Gtk::ListBoxRow *row) {
        line_dates->set_sensitive(row && row->get_index() >= 0 &&
                                  static_cast<std::size_t>(row->get_index()) <
                                      line_row_ids_.size());
      },
      *line_dates));
  units_toolbar_.append(add_unit_button_);
  units_toolbar_.append(open_unit_button_);
  units_panel_.append(units_toolbar_);
  auto *unit_dates =
      Gtk::make_managed<Gtk::Button>("Datas da parte selecionada…");
  unit_dates->set_halign(Gtk::Align::START);
  unit_dates->set_sensitive(false);
  units_panel_.append(*unit_dates);
  unit_dates->signal_clicked().connect([this] {
    if (selected_unit_id_ && owner_window())
      show_temporal_context(*owner_window(), service_, "unit",
                            *selected_unit_id_);
  });
  units_list_.signal_row_selected().connect(sigc::track_obj(
      [this, unit_dates](Gtk::ListBoxRow *row) {
        unit_dates->set_sensitive(row && row->get_index() >= 0 &&
                                  static_cast<std::size_t>(row->get_index()) <
                                      unit_row_ids_.size());
      },
      *unit_dates));
  auto *unit_heading = Gtk::make_managed<Gtk::Label>("Partes da narrativa");
  unit_heading->set_halign(Gtk::Align::START);
  unit_heading->add_css_class("heading");
  units_panel_.prepend(*unit_heading);
  units_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  units_list_.add_css_class("boxed-list");
  links_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  links_list_.add_css_class("boxed-list");
  units_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  units_scroll_.set_child(units_list_);
  units_scroll_.set_vexpand(true);
  units_panel_.append(units_scroll_);
  auto *links_heading =
      Gtk::make_managed<Gtk::Label>("Ordem, causas e alternativas");
  links_heading->set_halign(Gtk::Align::START);
  links_heading->add_css_class("heading");
  links_panel_.append(*links_heading);
  auto *links_toolbar =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
  links_toolbar->append(link_button_);
  links_toolbar->append(dependency_scope_button_);
  links_toolbar->append(remove_link_button_);
  links_panel_.append(*links_toolbar);
  links_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  links_scroll_.set_child(links_list_);
  links_scroll_.set_vexpand(true);
  links_panel_.append(links_scroll_);
  detail_sections_.add(units_panel_, "units", "Partes");
  detail_sections_.add(lines_panel_, "lines", "Linhas");
  detail_sections_.add(links_panel_, "links", "Vínculos");
  detail_page_.append(detail_sections_);
  stack_.add(detail_page_, "detail");
  build_unit_page();
  comparison_title_.set_hexpand(true);
  comparison_title_.set_xalign(0);
  comparison_title_.set_wrap(true);
  comparison_header_.append(comparison_back_);
  comparison_header_.append(comparison_title_);
  comparison_page_.append(comparison_header_);
  comparison_page_.append(comparison_target_combo_);
  comparison_target_combo_.set_visible(false);
  bound_choice_width(comparison_target_combo_, 44);
  comparison_target_combo_.signal_changed().connect([this] {
    if (changing_comparison_target_)
      return;
    try {
      comparison_target_ = comparison_target_combo_.get_active_id().raw();
      comparison_ =
          service_.builder().compare(comparison_source_, comparison_target_);
      comparison_page_index_ = 0;
      render_comparison();
    } catch (const std::exception &e) {
      show_error(e);
    }
  });
  comparison_scroll_.set_child(comparison_body_);
  comparison_scroll_.set_vexpand(true);
  comparison_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                Gtk::PolicyType::AUTOMATIC);
  comparison_page_.append(comparison_scroll_);
  auto *paging = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
  paging->append(comparison_previous_);
  paging->append(comparison_next_);
  comparison_page_.append(*paging);
  stack_.add(comparison_page_, "comparison");
  comparison_back_.signal_clicked().connect([this] {
    show_library();
    refresh_library();
  });
  comparison_previous_.signal_clicked().connect([this] {
    if (comparison_page_index_)
      --comparison_page_index_;
    render_comparison();
  });
  comparison_next_.signal_clicked().connect([this] {
    ++comparison_page_index_;
    render_comparison();
  });
  dependency_scope_button_.signal_clicked().connect(sigc::mem_fun(
      *this, &NarrativeStructureWorkspace::choose_dependency_scope));

  roles_toolbar_.set_selection_mode(Gtk::SelectionMode::NONE);
  roles_toolbar_.set_min_children_per_line(1);
  roles_toolbar_.set_max_children_per_line(4);
  roles_toolbar_.set_column_spacing(6);
  roles_toolbar_.set_row_spacing(6);
  roles_toolbar_.append(create_role_button_);
  roles_toolbar_.append(delete_role_button_);
  roles_toolbar_.append(assign_role_button_);
  roles_toolbar_.append(delete_assignment_button_);
  roles_page_.append(roles_toolbar_);
  roles_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  roles_list_.add_css_class("boxed-list");
  assignments_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  assignments_list_.add_css_class("boxed-list");
  roles_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  roles_scroll_.set_child(roles_list_);
  assignments_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                 Gtk::PolicyType::AUTOMATIC);
  assignments_scroll_.set_child(assignments_list_);
  roles_sections_.add(roles_scroll_, "roles", "Papéis disponíveis");
  roles_sections_.add(assignments_scroll_, "assignments", "Atribuições");
  roles_switcher_.set_stack(roles_sections_);
  roles_sections_.set_vexpand(true);
  roles_page_.append(roles_switcher_);
  roles_page_.append(roles_sections_);
  stack_.add(roles_page_, "roles");

  work_combo_.signal_changed().connect([this] {
    if (refreshing_works_)
      return;
    cancel_comparison_selection();
    selected_structure_id_.reset();
    selected_unit_id_.reset();
    selected_line_id_.reset();
    show_library();
    refresh_library();
    refresh_roles();
  });
  library_button_.signal_clicked().connect([this] { show_library(); });
  roles_button_.signal_clicked().connect([this] { show_roles(); });
  detail_back_.signal_clicked().connect([this] { show_library(); });
  structures_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_structure_id_ =
        row && row->get_index() >= 0 &&
                static_cast<std::size_t>(row->get_index()) <
                    structure_row_ids_.size()
            ? std::optional{structure_row_ids_[row->get_index()]}
            : std::nullopt;
  });
  models_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_model_id_ = row && row->get_index() >= 0 &&
                                 static_cast<std::size_t>(row->get_index()) <
                                     model_row_ids_.size()
                             ? std::optional{model_row_ids_[row->get_index()]}
                             : std::nullopt;
  });
  structures_list_.signal_row_activated().connect([this](Gtk::ListBoxRow *row) {
    if (row && row->get_index() >= 0 &&
        static_cast<std::size_t>(row->get_index()) <
            structure_row_ids_.size()) {
      if (!selection_source_.empty())
        toggle_comparison_target(structure_row_ids_[row->get_index()]);
      else
        show_detail(structure_row_ids_[row->get_index()]);
    }
  });
  lines_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_line_id_ = row && row->get_index() >= 0 &&
                                static_cast<std::size_t>(row->get_index()) <
                                    line_row_ids_.size()
                            ? std::optional{line_row_ids_[row->get_index()]}
                            : std::nullopt;
    remove_line_button_.set_sensitive(selected_line_id_.has_value());
    edit_line_button_.set_sensitive(selected_line_id_.has_value());
    if (!refreshing_detail_ && order_combo_.get_active_id() == "line")
      refresh_detail();
  });
  lines_list_.signal_row_activated().connect([this](Gtk::ListBoxRow *) {
    order_combo_.set_active_id("line");
    refresh_detail();
  });
  units_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_unit_id_ = row && row->get_index() >= 0 &&
                                static_cast<std::size_t>(row->get_index()) <
                                    unit_row_ids_.size()
                            ? std::optional{unit_row_ids_[row->get_index()]}
                            : std::nullopt;
    const bool has = selected_unit_id_.has_value();
    remove_unit_button_.set_sensitive(has);
    open_unit_button_.set_sensitive(has);
    membership_button_.set_sensitive(has);
    reality_button_.set_sensitive(has);
    link_button_.set_sensitive(selected_structure_id_.has_value() &&
                               builder_snapshot_.units.size() > 1);
  });
  links_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_link_id_ = row && row->get_index() >= 0 &&
                                static_cast<std::size_t>(row->get_index()) <
                                    link_row_ids_.size()
                            ? std::optional{link_row_ids_[row->get_index()]}
                            : std::nullopt;
    remove_link_button_.set_sensitive(selected_link_id_.has_value());
    dependency_scope_button_.set_sensitive(selected_link_id_.has_value());
  });
  roles_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_role_id_ = row && row->get_index() >= 0 &&
                                static_cast<std::size_t>(row->get_index()) <
                                    role_row_ids_.size()
                            ? std::optional{role_row_ids_[row->get_index()]}
                            : std::nullopt;
    assign_role_button_.set_sensitive(selected_role_id_.has_value());
  });
  assignments_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_assignment_id_ =
        row && row->get_index() >= 0 &&
                static_cast<std::size_t>(row->get_index()) <
                    assignment_row_ids_.size()
            ? std::optional{assignment_row_ids_[row->get_index()]}
            : std::nullopt;
    delete_assignment_button_.set_sensitive(
        selected_assignment_id_.has_value());
  });
  create_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::create_blank));
  add_line_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::add_line));
  add_unit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::add_unit));
  open_unit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::show_unit));
  units_list_.signal_row_activated().connect(
      [this](Gtk::ListBoxRow *) { show_unit(); });
  edit_line_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::edit_line));
  remove_line_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::delete_line));
  remove_unit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::delete_unit));
  membership_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::connect_unit_line));
  reality_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::connect_unit_entity));
  link_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::add_link));
  remove_link_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::delete_link));
  create_role_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::create_role));
  delete_role_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::delete_role));
  assign_role_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::assign_role));
  delete_assignment_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::delete_assignment));
  reset();
}

} // namespace inde::ui
