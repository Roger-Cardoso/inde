#include "inde/ui/structural_editor.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/bounded_choices.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include "inde/ui/incremental_selector.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace inde::ui {
namespace {

} // namespace

StructuralNodeItem::StructuralNodeItem(project::StructuralNode node)
    : Glib::ObjectBase(typeid(StructuralNodeItem)), Glib::Object(),
      node_(std::move(node)) {}

Glib::RefPtr<StructuralNodeItem>
StructuralNodeItem::create(project::StructuralNode node) {
  return Glib::make_refptr_for_instance<StructuralNodeItem>(
      new StructuralNodeItem(std::move(node)));
}

StructuralEditor::StructuralEditor(application::ProjectService &service)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0), service_(service) {
  build_ui();
  setup_structure_model();
  refresh();
}

void StructuralEditor::build_ui() {
  set_vexpand(true);
  set_hexpand(true);
  page_stack_.set_vexpand(true);
  page_stack_.set_hexpand(true);
  // O detalhe contém um perfil extenso dentro de Gtk::ScrolledWindow. Se a
  // pilha for homogênea, essa página ainda impõe sua altura mínima enquanto a
  // estrutura está visível e o XFWM deixa de conseguir manter a maximização.
  page_stack_.set_hhomogeneous(false);
  page_stack_.set_vhomogeneous(false);
  page_stack_.set_transition_type(Gtk::StackTransitionType::SLIDE_LEFT_RIGHT);
  append(page_stack_);
  page_stack_.add(content_area_, "structure");
  page_stack_.add(detail_page_, "detail");

  detail_page_.set_vexpand(true);
  detail_page_.set_hexpand(true);
  detail_hint_.set_hexpand(true);
  detail_hint_.set_halign(Gtk::Align::END);
  detail_hint_.add_css_class("dim-label");
  detail_header_.set_margin(12);
  detail_header_.add_css_class("detail-toolbar");
  detail_header_.append(back_to_structure_button_);
  detail_header_.append(detail_hint_);
  detail_page_.append(detail_header_);
  detail_page_.append(inspector_scroll_);

  inspector_scroll_.set_child(inspector_panel_);
  inspector_scroll_.set_policy(Gtk::PolicyType::NEVER,
                               Gtk::PolicyType::AUTOMATIC);
  inspector_scroll_.set_vexpand(true);
  content_area_.set_margin(12);
  inspector_panel_.set_margin(12);

  structure_stack_.set_hexpand(true);
  structure_stack_.set_vexpand(true);
  structure_stack_.set_hhomogeneous(false);
  structure_stack_.set_vhomogeneous(false);
  empty_structure_.set_valign(Gtk::Align::START);
  empty_structure_title_.add_css_class("title-2");
  empty_structure_title_.set_halign(Gtk::Align::START);
  empty_structure_hint_.add_css_class("dim-label");
  empty_structure_hint_.set_halign(Gtk::Align::START);
  empty_structure_hint_.set_wrap(true);
  empty_structure_.append(empty_structure_title_);
  empty_structure_.append(empty_structure_hint_);
  structure_stack_.add(empty_structure_, "empty");
  structure_stack_.add(structure_panel_, "structure");
  content_area_.append(structure_stack_);

  structure_title_.add_css_class("title-2");
  structure_title_.add_css_class("page-title");
  structure_title_.set_hexpand(true);
  structure_title_.set_halign(Gtk::Align::START);
  structure_hint_.add_css_class("dim-label");
  structure_panel_.set_vexpand(true);
  structure_header_.append(structure_title_);
  structure_filter_summary_.set_xalign(0.0F);
  structure_filter_summary_.set_wrap(true);
  structure_filter_summary_.add_css_class("dim-label");
  structure_panel_.append(structure_filter_summary_);

  structure_search_.set_placeholder_text(
      "Pesquisar título, subtítulo, sinopse ou status");
  set_accessible_label(structure_search_, "Pesquisar organização do manuscrito");
  set_accessible_description(
      structure_search_,
      "Filtra título, subtítulo, sinopse ou situação dos elementos.");
  filters_panel_.set_margin(12);
  structural_type_filter_label_.set_halign(Gtk::Align::START);
  filters_panel_.append(structural_type_filter_label_);
  structural_type_filter_.append("all", "Todos os tipos de unidade");
  structural_type_filter_.set_active_id("all");
  set_accessible_label(structural_type_filter_, "Tipo de unidade");
  filters_panel_.append(structural_type_filter_);
  status_filter_label_.set_halign(Gtk::Align::START);
  filters_panel_.append(status_filter_label_);
  status_filter_.append("all", "Todos os status");
  for (const auto *status :
       {"Planejamento", "Em desenvolvimento", "Revisão", "Concluído"})
    status_filter_.append(status, status);
  status_filter_.set_active_id("all");
  set_accessible_label(status_filter_, "Situação editorial");
  filters_panel_.append(status_filter_);
  entity_type_facets_label_.set_halign(Gtk::Align::START);
  filters_panel_.append(entity_type_facets_label_);
  entity_type_facets_scroll_.set_child(entity_type_facets_);
  entity_type_facets_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                        Gtk::PolicyType::AUTOMATIC);
  entity_type_facets_scroll_.set_min_content_height(72);
  entity_type_facets_scroll_.set_max_content_height(180);
  entity_type_facets_scroll_.set_propagate_natural_height(false);
  entity_type_facets_scroll_.set_propagate_natural_width(false);
  filters_panel_.append(entity_type_facets_scroll_);
  auto *filter_actions =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
  apply_filters_button_.add_css_class("suggested-action");
  filter_actions->append(clear_filters_button_);
  filter_actions->append(apply_filters_button_);
  filters_panel_.append(*filter_actions);
  filters_panel_.set_vexpand(true);
  filters_surface_.set_child(filters_panel_);
  filters_surface_.add_css_class("filter-sheet");
  filters_surface_.set_margin(8);
  filters_surface_.set_vexpand(true);
  filters_revealer_.set_child(filters_surface_);
  filters_revealer_.set_hexpand(true);
  filters_revealer_.set_vexpand(true);
  filters_revealer_.set_halign(Gtk::Align::FILL);
  filters_revealer_.set_valign(Gtk::Align::FILL);
  filters_revealer_.set_transition_type(
      Gtk::RevealerTransitionType::SLIDE_DOWN);
  set_overlay_revealer_open(filters_revealer_, false);

  organization_toolbar_.add_css_class("organization-toolbar");
  structure_search_.set_hexpand(true);
  organization_toolbar_.append(structure_search_);
  organization_toolbar_.append(filters_button_);
  organization_toolbar_.append(add_root_button_);
  organization_toolbar_.append(structure_add_child_button_);
  organization_toolbar_.append(structure_actions_button_);
  organization_button_.set_label("Organizar");
  organization_button_.set_tooltip_text("Tipos, modelos e recuperação da estrutura");
  organization_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  organization_scroll_.set_min_content_width(360);
  organization_scroll_.set_max_content_height(380);
  organization_scroll_.set_propagate_natural_height(true);
  organization_options_.set_margin(8);
  organization_scroll_.set_child(organization_options_);
  organization_popover_.set_child(organization_scroll_);
  organization_button_.set_popover(organization_popover_);
  organization_toolbar_.append(organization_button_);
  structure_panel_.append(organization_toolbar_);
  for (auto *section :
       {&filter_section_, &crud_section_, &type_section_, &position_section_,
        &template_section_, &recovery_section_}) {
    section->set_margin(10);
    section->add_css_class("tool-section");
    section->set_halign(Gtk::Align::START);
    section->set_valign(Gtk::Align::START);
  }
  for (auto *title : {&filter_section_title_, &crud_section_title_,
                      &position_section_title_, &type_section_title_,
                      &template_section_title_, &recovery_section_title_}) {
    title->add_css_class("heading");
    title->set_halign(Gtk::Align::START);
  }
  filter_section_.append(filter_section_title_);
  filter_section_.append(filter_actions_);
  crud_section_.append(crud_section_title_);
  crud_section_.append(crud_actions_);
  structural_type_manager_.set_size_request(190, -1);
  bound_choice_width(structural_type_manager_);
  type_actions_.set_orientation(Gtk::Orientation::VERTICAL);
  template_actions_.set_orientation(Gtk::Orientation::VERTICAL);
  set_accessible_label(structural_type_manager_,
                       "Tipo de elemento estrutural selecionado");
  type_actions_.append(structural_type_manager_);
  type_actions_.append(structural_type_add_button_);
  type_actions_.append(structural_type_edit_button_);
  type_actions_.append(structural_type_remove_button_);
  type_section_.append(type_section_title_);
  type_section_.append(type_actions_);
  structure_template_combo_.append("three-acts", "Três atos");
  structure_template_combo_.append("simple-novel", "Romance — 10 capítulos");
  structure_template_combo_.append("parts-and-chapters",
                                   "3 partes — 15 capítulos");
  structure_template_combo_.set_active(0);
  set_accessible_label(structure_template_combo_, "Modelo de manuscrito");
  set_accessible_description(
      structure_template_combo_,
      "Escolha a estrutura a aplicar antes de usar Aplicar modelo.");
  template_actions_.append(structure_template_combo_);
  template_actions_.append(structure_template_button_);
  template_section_.append(template_section_title_);
  template_section_.append(template_actions_);
  recovery_actions_.append(structure_restore_button_);
  recovery_section_.append(recovery_section_title_);
  recovery_section_.append(recovery_actions_);
  organization_options_.append(type_section_);
  organization_options_.append(template_section_);
  organization_options_.append(recovery_section_);
  structure_remove_button_.add_css_class("destructive-action");
  structure_search_.signal_search_changed().connect([this] {
    if (refreshing_filters_)
      return;
    structure_filter_ = structure_search_.get_text().raw();
    refresh();
  });
  structure_search_.signal_activate().connect(
      sigc::mem_fun(*this, &StructuralEditor::dismiss_filters));
  structural_type_filter_.signal_changed().connect([this] {
    if (!refreshing_filters_)
      refresh();
  });
  structural_type_manager_.signal_changed().connect([this] {
    const auto id = structural_type_manager_.get_active_id().raw();
    const auto found =
        std::find_if(structural_types_.begin(), structural_types_.end(),
                     [&](const auto &type) { return type.id == id; });
    const bool editable =
        found != structural_types_.end() && !found->is_builtin;
    structural_type_edit_button_.set_sensitive(editable);
    structural_type_remove_button_.set_sensitive(editable);
  });
  structural_type_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &StructuralEditor::create_structural_type));
  structural_type_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &StructuralEditor::edit_structural_type));
  structural_type_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &StructuralEditor::delete_structural_type));
  status_filter_.signal_changed().connect([this] {
    if (!refreshing_filters_)
      refresh();
  });
  filters_button_.signal_clicked().connect([this] {
    filters_visible_ = !filters_visible_;
    set_overlay_revealer_open(filters_revealer_, filters_visible_);
    filters_button_.set_label(filters_visible_ ? "Ocultar filtros" : "Filtros");
    if (filters_visible_)
      structure_search_.grab_focus();
  });
  clear_filters_button_.signal_clicked().connect([this] {
    refreshing_filters_ = true;
    structure_filter_.clear();
    selected_entity_type_ids_.clear();
    structure_search_.set_text("");
    structural_type_filter_.set_active_id("all");
    status_filter_.set_active_id("all");
    refreshing_filters_ = false;
    refresh();
  });
  apply_filters_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &StructuralEditor::dismiss_filters));
  structure_scroll_.set_child(structure_list_);
  structure_scroll_.set_vexpand(true);
  structure_list_.add_css_class("boxed-list");
  structure_overlay_.set_vexpand(true);
  structure_overlay_.set_hexpand(true);
  structure_overlay_.set_child(structure_scroll_);
  structure_overlay_.add_overlay(filters_revealer_);
  structure_panel_.append(structure_overlay_);

  inspector_title_.add_css_class("title-1");
  inspector_title_.add_css_class("page-title");
  inspector_title_.set_halign(Gtk::Align::START);
  inspector_name_.add_css_class("title-3");
  inspector_name_.set_halign(Gtk::Align::START);
  inspector_name_.set_xalign(0.0F);
  inspector_type_.set_halign(Gtk::Align::START);
  inspector_type_.set_xalign(0.0F);
  inspector_status_.set_halign(Gtk::Align::START);
  inspector_status_.set_xalign(0.0F);
  inspector_synopsis_.set_wrap(true);
  inspector_synopsis_.set_halign(Gtk::Align::START);
  inspector_cover_banner_.set_size_request(-1, 220);
  inspector_cover_banner_.set_hexpand(true);
  inspector_cover_banner_.add_css_class("editorial-profile-cover");
  inspector_cover_.set_content_fit(Gtk::ContentFit::COVER);
  inspector_cover_.set_can_shrink(true);
  inspector_cover_.set_hexpand(true);
  inspector_cover_.set_vexpand(true);
  inspector_cover_overlay_.set_child(inspector_cover_banner_);
  inspector_cover_overlay_.add_overlay(inspector_cover_);
  inspector_cover_overlay_.set_size_request(-1, 220);
  inspector_identity_panel_.set_margin_start(18);
  inspector_identity_panel_.set_margin_end(18);
  inspector_identity_panel_.set_margin_top(8);
  inspector_identity_panel_.set_margin_bottom(8);
  inspector_identity_panel_.append(inspector_type_);
  inspector_identity_panel_.append(inspector_name_);
  inspector_identity_panel_.append(inspector_status_);
  inspector_identity_panel_.append(inspector_synopsis_);
  inspector_cover_section_.set_child(inspector_cover_overlay_);
  inspector_cover_section_.set_expanded(false);
  inspector_summary_.append(inspector_identity_panel_);
  inspector_profile_overlay_.set_child(inspector_summary_);
  inspector_actions_button_.set_halign(Gtk::Align::END);
  inspector_actions_button_.set_valign(Gtk::Align::START);
  inspector_actions_button_.set_margin_top(12);
  inspector_actions_button_.set_margin_end(12);
  inspector_actions_button_.add_css_class("floating-action");
  inspector_profile_overlay_.add_overlay(inspector_actions_button_);
  inspector_summary_surface_.set_child(inspector_profile_overlay_);
  inspector_summary_surface_.add_css_class("content-card");
  inspector_summary_surface_.add_css_class("accent-card");
  inspector_panel_.append(inspector_summary_surface_);
  reference_title_.add_css_class("title-2");
  reference_title_.set_halign(Gtk::Align::START);
  reference_toolbar_.append(reference_add_button_);
  reference_toolbar_.append(reference_actions_button_);
  reference_remove_button_.add_css_class("destructive-action");
  reference_list_.add_css_class("boxed-list");
  direct_references_panel_.set_margin(14);
  direct_references_panel_.append(reference_title_);
  direct_references_panel_.append(reference_toolbar_);
  direct_references_panel_.append(reference_list_);
  direct_references_surface_.set_child(direct_references_panel_);
  direct_references_surface_.add_css_class("content-card");
  direct_references_section_.set_child(direct_references_surface_);
  direct_references_section_.set_expanded(false);
  inherited_references_title_.add_css_class("title-2");
  inherited_references_title_.set_halign(Gtk::Align::START);
  inherited_references_hint_.add_css_class("dim-label");
  inherited_references_hint_.set_halign(Gtk::Align::START);
  inherited_references_hint_.set_wrap(true);
  inherited_references_list_.add_css_class("boxed-list");
  inherited_references_panel_.set_margin(14);
  inherited_references_panel_.append(inherited_references_title_);
  inherited_references_panel_.append(inherited_references_hint_);
  inherited_references_panel_.append(inherited_references_list_);
  inherited_references_surface_.set_child(inherited_references_panel_);
  inherited_references_surface_.add_css_class("content-card");
  inherited_references_section_.set_child(inherited_references_surface_);
  inherited_references_section_.set_expanded(false);
  documents_title_.add_css_class("title-2");
  documents_title_.set_halign(Gtk::Align::START);
  documents_hint_.set_halign(Gtk::Align::START);
  documents_hint_.set_wrap(true);
  documents_hint_.add_css_class("dim-label");
  documents_toolbar_.append(create_document_button_);
  documents_toolbar_.append(filter_documents_button_);
  documents_list_.add_css_class("boxed-list");
  documents_panel_.set_margin(14);
  documents_panel_.append(documents_title_);
  documents_panel_.append(documents_hint_);
  documents_panel_.append(documents_toolbar_);
  documents_panel_.append(documents_list_);
  documents_surface_.set_child(documents_panel_);
  documents_surface_.add_css_class("content-card");
  inspector_panel_.append(documents_surface_);
  inspector_panel_.append(inspector_cover_section_);
  inspector_panel_.append(direct_references_section_);
  inspector_panel_.append(inherited_references_section_);
  configure_overflow_button(
      structure_actions_button_,
      {{"Editar elemento",
        [this] {
          if (selected_structural_node_id_)
            edit_structural_node(*selected_structural_node_id_);
        },
        "document-edit-symbolic"},
       {"Duplicar ramificação", [this] { duplicate_selected_branch(); },
        "edit-copy-symbolic"},
       {"Mover para cima",
        [this] {
          if (!selected_structural_node_id_)
            return;
          try {
            service_.move_structural_node_up(*selected_structural_node_id_);
            refresh();
          } catch (const std::exception &error) {
            show_error("Não foi possível reordenar", error);
          }
        },
        "go-up-symbolic", true, false, true},
       {"Mover para baixo",
        [this] {
          if (!selected_structural_node_id_)
            return;
          try {
            service_.move_structural_node_down(*selected_structural_node_id_);
            refresh();
          } catch (const std::exception &error) {
            show_error("Não foi possível reordenar", error);
          }
        },
        "go-down-symbolic"},
       {"Remover ramificação",
        [this] {
          if (selected_structural_node_id_)
            delete_structural_node(*selected_structural_node_id_);
        },
        "user-trash-symbolic", true, true, true}},
      "Ações do elemento selecionado");
  configure_overflow_button(
      inspector_actions_button_,
      {{"Editar elemento",
        [this] {
          if (selected_structural_node_id_)
            edit_structural_node(*selected_structural_node_id_);
        },
        "document-edit-symbolic"},
       {"Duplicar ramificação", [this] { duplicate_selected_branch(); },
        "edit-copy-symbolic"},
       {"Remover ramificação",
        [this] {
          if (selected_structural_node_id_)
            delete_structural_node(*selected_structural_node_id_);
        },
        "user-trash-symbolic", true, true, true}},
      "Ações deste elemento");
  configure_overflow_button(
      reference_actions_button_,
      {{"Editar apresentação", [this] { edit_editorial_reference(); },
        "document-edit-symbolic"},
       {"Remover apresentação", [this] { delete_editorial_reference(); },
        "user-trash-symbolic", true, true, true}},
      "Ações da apresentação selecionada");
  reference_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &StructuralEditor::add_editorial_reference));
  reference_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &StructuralEditor::edit_editorial_reference));
  reference_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &StructuralEditor::delete_editorial_reference));
  back_to_structure_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &StructuralEditor::show_structure_page));
  create_document_button_.signal_clicked().connect([this] {
    if (selected_structural_node_id_)
      signal_create_document_requested_.emit(*selected_structural_node_id_);
  });
  filter_documents_button_.signal_clicked().connect([this] {
    if (selected_structural_node_id_)
      signal_filter_documents_requested_.emit(*selected_structural_node_id_);
  });
}

} // namespace inde::ui
