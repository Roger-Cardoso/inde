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
  structure_panel_.append(structure_header_);
  structure_panel_.append(structure_hint_);
  structure_filter_summary_.set_xalign(0.0F);
  structure_filter_summary_.set_wrap(true);
  structure_filter_summary_.add_css_class("dim-label");
  structure_panel_.append(structure_filter_summary_);

  structure_search_.set_placeholder_text(
      "Pesquisar título, subtítulo, sinopse ou status");
  set_accessible_label(structure_search_, "Pesquisar estrutura editorial");
  set_accessible_description(
      structure_search_,
      "Filtra título, subtítulo, sinopse ou situação dos elementos.");
  filters_panel_.set_margin(12);
  filters_panel_.append(structure_search_);
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

  structure_toolbar_.set_selection_mode(Gtk::SelectionMode::NONE);
  structure_toolbar_.set_column_spacing(8);
  structure_toolbar_.set_row_spacing(8);
  structure_toolbar_.set_min_children_per_line(1);
  structure_toolbar_.set_max_children_per_line(5);
  // Em larguras menores, as seções da barra quebram em várias linhas. Sem um
  // limite, o FlowBox eleva a altura mínima da janela acima da área útil e o
  // gerenciador de janelas precisa desfazer a maximização. A rolagem mantém
  // todas as ações acessíveis sem deixar o conteúdo controlar a geometria da
  // janela principal.
  structure_toolbar_scroll_.set_child(structure_toolbar_);
  structure_toolbar_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                       Gtk::PolicyType::AUTOMATIC);
  structure_toolbar_scroll_.set_min_content_height(112);
  structure_toolbar_scroll_.set_max_content_height(240);
  structure_toolbar_scroll_.set_propagate_natural_height(true);
  structure_toolbar_scroll_.set_propagate_natural_width(false);
  structure_panel_.append(structure_toolbar_scroll_);
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
  filter_actions_.append(filters_button_);
  filter_section_.append(filter_section_title_);
  filter_section_.append(filter_actions_);
  crud_actions_.append(add_root_button_);
  crud_actions_.append(structure_add_child_button_);
  crud_actions_.append(structure_actions_button_);
  crud_section_.append(crud_section_title_);
  crud_section_.append(crud_actions_);
  structural_type_manager_.set_size_request(190, -1);
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
  set_accessible_label(structure_template_combo_, "Template estrutural");
  set_accessible_description(
      structure_template_combo_,
      "Escolha a estrutura a aplicar antes de usar Aplicar template.");
  template_actions_.append(structure_template_combo_);
  template_actions_.append(structure_template_button_);
  template_section_.append(template_section_title_);
  template_section_.append(template_actions_);
  recovery_actions_.append(structure_restore_button_);
  recovery_section_.append(recovery_section_title_);
  recovery_section_.append(recovery_actions_);
  structure_toolbar_.append(filter_section_);
  structure_toolbar_.append(crud_section_);
  structure_toolbar_.append(type_section_);
  structure_toolbar_.append(template_section_);
  structure_toolbar_.append(recovery_section_);
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
  inspector_identity_panel_.set_margin_top(14);
  inspector_identity_panel_.set_margin_bottom(18);
  inspector_identity_panel_.append(inspector_type_);
  inspector_identity_panel_.append(inspector_name_);
  inspector_identity_panel_.append(inspector_status_);
  inspector_identity_panel_.append(inspector_synopsis_);
  inspector_summary_.append(inspector_cover_overlay_);
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
  inspector_panel_.append(direct_references_surface_);
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
  inspector_panel_.append(inherited_references_surface_);
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
  signal_status_message_.emit("Unidade editorial aberta a partir da Escrita");
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
                      " tipo(s) de entidade apresentada");
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
  entity_type_facets_label_.set_text("Entidades apresentadas");
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
        "Mostrar unidades que apresentam entidades deste tipo");
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
  window.add_action("restore-deletion", [this] {
    try {
      if (service_.restore_last_structural_deletion()) {
        refresh();
        signal_status_message_.emit("Última exclusão restaurada");
      } else
        signal_status_message_.emit("Nenhuma exclusão para restaurar");
    } catch (const std::exception &error) {
      show_error("Não foi possível restaurar a exclusão", error);
    }
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
    signal_status_message_.emit("Template estrutural criado e salvo");
  } catch (const std::exception &error) {
    show_error("Não foi possível aplicar o template", error);
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
            {{"Novo elemento filho",
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

void StructuralEditor::update_inspector() {
  if (!selected_work_id_ || !selected_structural_node_id_) {
    inspector_type_.set_text("Nenhum elemento selecionado");
    inspector_name_.set_text("");
    inspector_status_.set_text("");
    inspector_synopsis_.set_text("");
    inspector_cover_.set_visible(false);
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
  inspector_cover_.set_visible(false);
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
    documents_hint_.set_text("Selecione uma unidade editorial.");
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
        (entity ? entity->name : "Entidade ausente") + " — " +
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
        "Este elemento ainda não possui filhos com entidades apresentadas.");
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
        "Nenhuma entidade foi apresentada nos elementos filhos.");
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
        (entity ? entity->name : "Entidade ausente") + " — " +
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
      new OverlayDialog("Apresentar entidade nesta unidade", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Adicionar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  const auto work_id = *selected_work_id_;
  auto *entity = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar entidade vinculada à Obra",
      [this, work_id](const std::string &search, std::size_t limit) {
        persistence::EntityQuery query;
        query.search = search;
        query.work_id = work_id;
        query.limit = std::min(limit, std::size_t{50});
        std::vector<IncrementalSelection> result;
        for (const auto &value : service_.narrative().entities(query))
          result.push_back(
              {value.id, value.name, "Entidade no escopo da Obra"});
        return result;
      },
      "Pesquise uma entidade vinculada à Obra");
  auto *purpose = Gtk::make_managed<Gtk::Entry>();
  purpose->set_placeholder_text(
      "Finalidade, por exemplo: aparece, mencionada, revelada");
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_placeholder_text("Notas editoriais");
  form->set_margin(16);
  append_labeled_form_field(*form, "Entidade no escopo da Obra", *entity,
                            "Pesquise e escolha a entidade a apresentar.");
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
                  "Vincule uma entidade à Obra no Planejamento primeiro");
            static_cast<void>(service_.narrative().add_editorial_reference(
                *entity->selected_id(), node_id, purpose->get_text(),
                notes->get_text()));
            refresh_editorial_references();
            signal_status_message_.emit("Entidade apresentada na unidade");
          } catch (const std::exception &error) {
            show_error("Não foi possível adicionar a entidade", error);
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
      *owner, "Remover esta entidade da unidade editorial?", false,
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
      new OverlayDialog(*owner, error.what(), false, Gtk::MessageType::ERROR,
                        Gtk::ButtonsType::CLOSE, true);
  dialog->set_title(title);
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->show();
}

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
  auto *dialog = new OverlayDialog(
      *owner, "Remover o tipo de elemento “" + found->name + "”?", false,
      Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);
  dialog->set_secondary_text(
      "Tipos usados por elementos ativos ou pela lixeira são protegidos.");
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.delete_structural_element_type(id);
        refresh_structural_types();
        refresh();
        signal_status_message_.emit("Tipo estrutural removido");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover o tipo", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
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
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog(
      *owner, "Remover este elemento e todos os seus descendentes?", false,
      Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);
  dialog->set_secondary_text(
      "A ramificação poderá ser restaurada com “Desfazer exclusão”.");
  dialog->signal_response().connect(
      [this, dialog, id = std::move(id)](int response) {
        if (response == Gtk::ResponseType::YES)
          try {
            service_.delete_structural_branch(id);
            refresh();
          } catch (const std::exception &error) {
            show_error("Não foi possível remover a ramificação", error);
          }
        dialog->hide();
      });
  dialog->present();
}

} // namespace inde::ui
