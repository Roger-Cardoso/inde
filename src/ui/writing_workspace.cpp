#include "inde/ui/writing_workspace.hpp"

#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include "inde/project/manifest.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace inde::ui {
namespace {

constexpr std::size_t reading_speed = 200;

void prepare_tool_label(Gtk::Label &label) {
  label.set_xalign(0.0F);
  label.set_wrap(true);
  label.add_css_class("dim-label");
}

void clear_list(Gtk::ListBox &list) {
  while (auto *child = list.get_first_child())
    list.remove(*child);
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

WritingWorkspace::WritingWorkspace(application::ProjectService &service)
    : Gtk::Box(Gtk::Orientation::VERTICAL), service_(service) {
  build_ui();
}

void WritingWorkspace::build_ui() {
  set_hexpand(true);
  set_vexpand(true);
  set_margin_start(18);
  set_margin_end(18);
  set_margin_bottom(14);

  page_stack_.set_hexpand(true);
  page_stack_.set_vexpand(true);
  page_stack_.set_transition_type(Gtk::StackTransitionType::CROSSFADE);
  append(page_stack_);

  library_heading_.set_hexpand(true);
  library_title_.set_halign(Gtk::Align::START);
  library_title_.add_css_class("title-1");
  library_summary_.set_halign(Gtk::Align::START);
  library_summary_.set_xalign(0.0F);
  library_summary_.add_css_class("dim-label");
  library_context_.set_halign(Gtk::Align::START);
  library_context_.set_xalign(0.0F);
  library_context_.set_ellipsize(Pango::EllipsizeMode::END);
  library_context_.add_css_class("dim-label");
  library_heading_.append(library_title_);
  library_heading_.append(library_summary_);
  library_heading_.append(library_context_);
  library_header_.append(library_heading_);
  leave_group_button_.set_visible(false);
  library_header_.prepend(leave_group_button_);
  set_accessible_label(library_filters_button_,
                       "Abrir pesquisa e filtros da Biblioteca");
  library_header_.append(library_filters_button_);
  library_header_.append(new_group_button_);
  group_actions_button_.set_visible(false);
  library_header_.append(group_actions_button_);
  new_document_button_.add_css_class("suggested-action");
  library_header_.append(new_document_button_);
  library_header_.add_css_class("toolbar-surface");
  library_page_.append(library_header_);
  library_page_.add_css_class("writing-library");

  document_list_.set_selection_mode(Gtk::SelectionMode::NONE);
  document_list_.set_min_children_per_line(1);
  document_list_.set_max_children_per_line(6);
  document_list_.set_column_spacing(12);
  document_list_.set_row_spacing(12);
  document_list_.set_homogeneous(true);
  document_list_.set_halign(Gtk::Align::FILL);
  document_list_.set_valign(Gtk::Align::START);
  document_list_.set_hexpand(true);
  document_list_.set_vexpand(false);
  library_scroll_.set_policy(Gtk::PolicyType::NEVER,
                             Gtk::PolicyType::AUTOMATIC);
  library_scroll_.set_child(document_list_);
  library_scroll_.set_hexpand(true);
  library_scroll_.set_vexpand(true);
  library_scroll_.set_propagate_natural_height(false);
  library_scroll_.set_propagate_natural_width(false);

  empty_state_.set_halign(Gtk::Align::CENTER);
  empty_state_.set_valign(Gtk::Align::CENTER);
  empty_state_.set_vexpand(true);
  empty_state_.set_margin(36);
  empty_state_.add_css_class("content-card");
  empty_title_.add_css_class("title-2");
  empty_message_.set_wrap(true);
  empty_message_.set_max_width_chars(58);
  empty_message_.set_justify(Gtk::Justification::CENTER);
  empty_message_.add_css_class("dim-label");
  empty_state_.append(empty_title_);
  empty_state_.append(empty_message_);

  library_results_stack_.set_hexpand(true);
  library_results_stack_.set_vexpand(true);
  library_results_stack_.set_hhomogeneous(false);
  library_results_stack_.set_vhomogeneous(false);
  library_results_stack_.add(library_scroll_, "results");
  library_results_stack_.add(empty_state_, "empty");
  library_overlay_.set_child(library_results_stack_);
  library_overlay_.set_hexpand(true);
  library_overlay_.set_vexpand(true);

  search_.set_placeholder_text("Pesquisar nome ou conteúdo");
  set_accessible_label(search_, "Pesquisar Documentos");
  set_accessible_description(
      search_, "Filtra Documentos literalmente pelo nome ou pelo conteúdo.");
  placement_filter_.append("all", "Todas as colocações editoriais");
  placement_filter_.set_active_id("all");
  constrain_combo_text(placement_filter_, 42);
  set_accessible_label(placement_filter_, "Filtrar por unidade editorial");
  group_filter_.append("all", "Todos os grupos");
  group_filter_.set_active_id("all");
  purpose_filter_.append("all", "Todas as finalidades");
  for (const auto purpose :
       {project::DocumentPurpose::MainText,
        project::DocumentPurpose::Annotation,
        project::DocumentPurpose::Revision, project::DocumentPurpose::Outline,
        project::DocumentPurpose::Research, project::DocumentPurpose::Reference,
        project::DocumentPurpose::Other})
    purpose_filter_.append(project::to_string(purpose),
                           project::display_name(purpose));
  purpose_filter_.set_active_id("all");
  perspective_filter_.set_placeholder_text("Perspectiva ou ponto de vista");
  entity_filter_ = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar entidade usada",
      [this](const std::string &search, std::size_t limit) {
        std::vector<IncrementalSelection> result;
        if (!service_.current())
          return result;
        persistence::EntityQuery query;
        query.search = search;
        query.limit = std::min(limit, std::size_t{50});
        for (const auto &entity : service_.narrative().entities(query))
          result.push_back({entity.id, entity.name, entity.summary});
        return result;
      },
      "Nenhuma entidade selecionada");
  library_filters_panel_.set_margin(18);
  auto *filter_title = Gtk::make_managed<Gtk::Label>("Pesquisa e filtros");
  filter_title->set_halign(Gtk::Align::START);
  filter_title->add_css_class("title-2");
  library_filters_panel_.append(*filter_title);
  library_filters_panel_.append(search_);
  library_filters_panel_.append(placement_filter_);
  library_filters_panel_.append(group_filter_);
  library_filters_panel_.append(purpose_filter_);
  library_filters_panel_.append(perspective_filter_);
  library_filters_panel_.append(revisions_filter_);
  library_filters_panel_.append(*entity_filter_);
  apply_library_filters_button_.add_css_class("suggested-action");
  library_filter_actions_.set_halign(Gtk::Align::END);
  library_filter_actions_.append(clear_library_filters_button_);
  library_filter_actions_.append(apply_library_filters_button_);
  library_filters_panel_.append(library_filter_actions_);
  library_filters_surface_.set_child(library_filters_panel_);
  library_filters_surface_.add_css_class("filter-sheet");
  library_filters_surface_.set_margin(8);
  library_filters_revealer_.set_child(library_filters_surface_);
  library_filters_revealer_.set_hexpand(true);
  library_filters_revealer_.set_vexpand(true);
  library_filters_revealer_.set_halign(Gtk::Align::FILL);
  library_filters_revealer_.set_valign(Gtk::Align::FILL);
  library_filters_revealer_.set_transition_type(
      Gtk::RevealerTransitionType::SLIDE_DOWN);
  set_overlay_revealer_open(library_filters_revealer_, false);
  library_overlay_.add_overlay(library_filters_revealer_);
  library_page_.append(library_overlay_);

  editor_toolbar_.add_css_class("toolbar-surface");
  editor_toolbar_.add_css_class("writing-toolbar");
  editor_toolbar_.append(back_button_);
  title_entry_.set_hexpand(true);
  title_entry_.set_placeholder_text("Nome do Documento");
  set_accessible_label(title_entry_, "Nome do Documento");
  editor_toolbar_.append(title_entry_);
  placement_.append("none", "Sem colocação editorial");
  placement_.set_active_id("none");
  constrain_combo_text(placement_, 38);
  placement_.set_hexpand(false);
  placement_.set_tooltip_text(
      "Situa o Documento sem transformar a unidade editorial no texto.");
  set_accessible_label(placement_, "Colocação editorial do Documento");
  editor_toolbar_.append(placement_);
  open_placement_button_.set_sensitive(false);
  open_placement_button_.add_css_class("flat");
  open_placement_button_.set_tooltip_text(
      "Abrir o detalhe desta unidade no workspace Editorial");
  set_accessible_label(open_placement_button_,
                       "Abrir unidade editorial do Documento");
  editor_toolbar_.append(open_placement_button_);
  editor_toolbar_.append(organization_button_);
  editor_toolbar_.append(tools_button_);
  save_button_.add_css_class("suggested-action");
  save_button_.set_sensitive(false);
  editor_toolbar_.append(save_button_);
  delete_button_.add_css_class("flat");
  delete_button_.add_css_class("quiet-destructive");
  editor_toolbar_.append(delete_button_);
  editor_page_.append(editor_toolbar_);

  formatting_toolbar_.add_css_class("toolbar-surface");
  formatting_toolbar_.add_css_class("writing-format-toolbar");
  for (auto *button :
       {&undo_button_, &redo_button_, &bold_button_, &italic_button_,
        &underline_button_, &strike_button_, &heading_button_,
        &subheading_button_, &quote_button_, &clear_format_button_})
    formatting_toolbar_.append(*button);
  bold_button_.set_tooltip_text("Negrito na seleção");
  italic_button_.set_tooltip_text("Itálico na seleção");
  underline_button_.set_tooltip_text("Sublinhado na seleção");
  strike_button_.set_tooltip_text("Tachado na seleção");
  heading_button_.set_tooltip_text("Título no parágrafo selecionado");
  subheading_button_.set_tooltip_text("Subtítulo no parágrafo selecionado");
  quote_button_.set_tooltip_text("Citação no parágrafo selecionado");
  editor_page_.append(formatting_toolbar_);

  editor_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
  editor_.set_left_margin(34);
  editor_.set_right_margin(34);
  editor_.set_top_margin(28);
  editor_.set_bottom_margin(36);
  editor_.set_pixels_above_lines(3);
  editor_.set_pixels_below_lines(3);
  editor_.add_css_class("writing-editor");
  set_accessible_label(editor_, "Conteúdo do Documento");
  set_accessible_description(
      editor_, "Editor formatado do Documento atual. Alterações são salvas "
               "automaticamente após uma breve pausa.");
  editor_scroll_.set_policy(Gtk::PolicyType::AUTOMATIC,
                            Gtk::PolicyType::AUTOMATIC);
  editor_scroll_.set_child(editor_);
  editor_scroll_.set_hexpand(true);
  editor_scroll_.set_vexpand(true);
  editor_scroll_.add_css_class("content-card");
  editor_scroll_.add_css_class("writing-paper");
  editor_surface_.set_child(editor_scroll_);
  editor_surface_.set_hexpand(true);
  editor_surface_.set_vexpand(true);
  editor_size_observer_.set_can_target(false);
  editor_size_observer_.set_hexpand(true);
  editor_size_observer_.set_vexpand(true);
  editor_size_observer_.signal_resize().connect([this](int width, int) {
    const int prose_margin = std::clamp((width - 1120) / 2, 34, 360);
    editor_.set_left_margin(prose_margin);
    editor_.set_right_margin(prose_margin);
  });
  editor_surface_.add_overlay(editor_size_observer_);
  editor_body_overlay_.set_child(editor_surface_);
  editor_body_overlay_.set_hexpand(true);
  editor_body_overlay_.set_vexpand(true);

  stats_title_.add_css_class("title-3");
  stats_title_.set_halign(Gtk::Align::START);
  for (auto *label : {&stats_words_, &stats_characters_, &stats_structure_,
                      &stats_reading_time_, &repeated_words_})
    prepare_tool_label(*label);
  stats_grid_.append(stats_words_);
  stats_grid_.append(stats_characters_);
  stats_grid_.append(stats_structure_);
  stats_grid_.append(stats_reading_time_);
  tools_panel_.append(stats_title_);
  tools_panel_.append(stats_grid_);
  goal_title_.add_css_class("heading");
  goal_title_.set_halign(Gtk::Align::START);
  tools_panel_.append(goal_separator_);
  tools_panel_.append(goal_title_);
  tools_panel_.append(goal_enabled_);
  word_goal_.set_range(1, 10000000);
  word_goal_.set_increments(100, 1000);
  word_goal_.set_value(1000);
  tools_panel_.append(word_goal_);
  goal_progress_.set_show_text(true);
  tools_panel_.append(goal_progress_);
  repeated_title_.add_css_class("heading");
  repeated_title_.set_halign(Gtk::Align::START);
  tools_panel_.append(repeated_separator_);
  tools_panel_.append(repeated_title_);
  tools_panel_.append(repeated_words_);
  paragraphs_title_.add_css_class("heading");
  paragraphs_title_.set_halign(Gtk::Align::START);
  long_paragraphs_list_.add_css_class("boxed-list");
  tools_panel_.append(paragraphs_separator_);
  tools_panel_.append(paragraphs_title_);
  tools_panel_.append(long_paragraphs_list_);
  anchors_title_.add_css_class("heading");
  anchors_title_.set_halign(Gtk::Align::START);
  anchor_actions_.append(add_anchor_button_);
  anchor_actions_.append(rename_anchor_button_);
  anchor_actions_.append(remove_anchor_button_);
  anchors_list_.add_css_class("boxed-list");
  tools_panel_.append(anchors_separator_);
  tools_panel_.append(anchors_title_);
  tools_panel_.append(anchor_actions_);
  tools_panel_.append(anchors_list_);
  references_title_.add_css_class("heading");
  references_title_.set_halign(Gtk::Align::START);
  reference_actions_.append(add_reference_button_);
  reference_actions_.append(open_reference_button_);
  reference_actions_.append(remove_reference_button_);
  references_list_.add_css_class("boxed-list");
  tools_panel_.append(references_separator_);
  tools_panel_.append(references_title_);
  tools_panel_.append(reference_actions_);
  tools_panel_.append(references_list_);
  tools_panel_.set_margin(16);
  tools_panel_.set_size_request(390, -1);
  tools_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  tools_scroll_.set_child(tools_panel_);
  tools_scroll_.set_vexpand(true);
  tools_surface_.set_child(tools_scroll_);
  tools_surface_.add_css_class("writing-tools-sheet");
  tools_revealer_.set_child(tools_surface_);
  tools_revealer_.set_halign(Gtk::Align::END);
  tools_revealer_.set_valign(Gtk::Align::FILL);
  tools_revealer_.set_vexpand(true);
  tools_revealer_.set_transition_type(Gtk::RevealerTransitionType::SLIDE_LEFT);
  set_overlay_revealer_open(tools_revealer_, false);
  editor_body_overlay_.add_overlay(tools_revealer_);
  editor_page_.append(editor_body_overlay_);

  editor_status_.set_halign(Gtk::Align::START);
  editor_status_.set_hexpand(true);
  editor_status_.add_css_class("dim-label");
  word_count_.set_halign(Gtk::Align::END);
  word_count_.add_css_class("dim-label");
  editor_footer_.append(editor_status_);
  editor_footer_.append(word_count_);
  editor_page_.append(editor_footer_);

  page_stack_.add(library_page_, "library");
  page_stack_.add(editor_page_, "editor");
  page_stack_.set_visible_child("library");
  build_formatting_tags();

  configure_overflow_button(
      group_actions_button_,
      {{"Editar grupo", [this] { edit_current_group(); },
        "document-edit-symbolic"},
       {"Remover grupo", [this] { delete_current_group(); },
        "user-trash-symbolic", true, true, true}},
      "Ações deste grupo");

  library_filters_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::toggle_library_filters));
  search_.signal_search_changed().connect(
      sigc::mem_fun(*this, &WritingWorkspace::refresh_library));
  search_.signal_activate().connect(
      sigc::mem_fun(*this, &WritingWorkspace::dismiss_library_filters));
  clear_library_filters_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::clear_library_filters));
  apply_library_filters_button_.signal_clicked().connect([this] {
    refresh_library();
    dismiss_library_filters();
  });
  new_group_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::create_document_group));
  leave_group_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::leave_document_group));
  back_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::show_library));
  tools_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::toggle_tools));
  organization_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::edit_document_organization));
  delete_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::delete_document));
  title_entry_.signal_changed().connect(
      sigc::mem_fun(*this, &WritingWorkspace::mark_dirty));
  placement_.signal_changed().connect([this] {
    const auto placement = placement_.get_active_id().raw();
    open_placement_button_.set_sensitive(!placement.empty() &&
                                         placement != "none");
    placement_.set_tooltip_text(
        placement.empty() || placement == "none"
            ? "Situa o Documento sem transformar a unidade editorial no texto."
            : placement_label(placement));
    mark_dirty();
  });
  for (auto *combo : {&placement_filter_, &group_filter_, &purpose_filter_})
    combo->signal_changed().connect([this] {
      if (!refreshing_library_controls_)
        refresh_library();
    });
  perspective_filter_.signal_changed().connect(
      sigc::mem_fun(*this, &WritingWorkspace::refresh_library));
  revisions_filter_.signal_toggled().connect(
      sigc::mem_fun(*this, &WritingWorkspace::refresh_library));
  open_placement_button_.signal_clicked().connect([this] {
    const auto placement = placement_.get_active_id().raw();
    if (!placement.empty() && placement != "none")
      signal_editorial_source_requested_.emit(placement);
  });
  editor_.get_buffer()->signal_changed().connect([this] {
    mark_dirty();
    refresh_formatting_toolbar();
  });
  editor_.get_buffer()->signal_mark_set().connect(
      [this](const Gtk::TextBuffer::iterator &,
             const Glib::RefPtr<Gtk::TextMark> &) {
        refresh_formatting_toolbar();
      });
  goal_enabled_.signal_toggled().connect([this] {
    word_goal_.set_sensitive(goal_enabled_.get_active());
    mark_dirty();
    update_goal_state();
  });
  word_goal_.signal_value_changed().connect([this] {
    mark_dirty();
    update_goal_state();
  });
  undo_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::undo));
  redo_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::redo));
  bold_button_.signal_clicked().connect(
      [this] { apply_text_style(project::DocumentTextStyle::Bold); });
  italic_button_.signal_clicked().connect(
      [this] { apply_text_style(project::DocumentTextStyle::Italic); });
  underline_button_.signal_clicked().connect(
      [this] { apply_text_style(project::DocumentTextStyle::Underline); });
  strike_button_.signal_clicked().connect(
      [this] { apply_text_style(project::DocumentTextStyle::Strikethrough); });
  heading_button_.signal_clicked().connect(
      [this] { apply_text_style(project::DocumentTextStyle::Heading); });
  subheading_button_.signal_clicked().connect(
      [this] { apply_text_style(project::DocumentTextStyle::Subheading); });
  quote_button_.signal_clicked().connect(
      [this] { apply_text_style(project::DocumentTextStyle::Quote); });
  clear_format_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::clear_text_formatting));
  add_anchor_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::add_anchor));
  rename_anchor_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::rename_anchor));
  remove_anchor_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::remove_anchor));
  add_reference_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::add_entity_reference));
  open_reference_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::open_reference_entity));
  remove_reference_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::remove_entity_reference));
  update_undo_actions();
  refresh_formatting_toolbar();
}

void WritingWorkspace::build_formatting_tags() {
  const auto buffer = editor_.get_buffer();
  const auto add = [&](project::DocumentTextStyle style, const char *name) {
    auto tag = buffer->create_tag(name);
    formatting_tags_.emplace_back(style, tag);
    return tag;
  };
  add(project::DocumentTextStyle::Bold, "inde-bold")->property_weight() =
      static_cast<int>(Pango::Weight::BOLD);
  add(project::DocumentTextStyle::Italic, "inde-italic")->property_style() =
      Pango::Style::ITALIC;
  add(project::DocumentTextStyle::Underline, "inde-underline")
      ->property_underline() = Pango::Underline::SINGLE;
  add(project::DocumentTextStyle::Strikethrough, "inde-strikethrough")
      ->property_strikethrough() = true;
  auto heading = add(project::DocumentTextStyle::Heading, "inde-heading");
  heading->property_weight() = static_cast<int>(Pango::Weight::BOLD);
  heading->property_scale() = 1.5;
  heading->property_pixels_above_lines() = 14;
  heading->property_pixels_below_lines() = 8;
  auto subheading =
      add(project::DocumentTextStyle::Subheading, "inde-subheading");
  subheading->property_weight() = static_cast<int>(Pango::Weight::SEMIBOLD);
  subheading->property_scale() = 1.2;
  subheading->property_pixels_above_lines() = 10;
  subheading->property_pixels_below_lines() = 6;
  auto quote = add(project::DocumentTextStyle::Quote, "inde-quote");
  quote->property_style() = Pango::Style::ITALIC;
  quote->property_left_margin() = 28;
  quote->property_indent() = 10;
}

void WritingWorkspace::install_actions(Gtk::ApplicationWindow &window) {
  window.add_action("create-document",
                    sigc::mem_fun(*this, &WritingWorkspace::create_document));
  window.add_action("save-document",
                    sigc::mem_fun(*this, &WritingWorkspace::save_document));
  window.add_action("writing-undo",
                    sigc::mem_fun(*this, &WritingWorkspace::undo));
  window.add_action("writing-redo",
                    sigc::mem_fun(*this, &WritingWorkspace::redo));
  window.add_action("writing-bold", [this] {
    if (page_stack_.get_visible_child_name() == "editor")
      apply_text_style(project::DocumentTextStyle::Bold);
  });
  window.add_action("writing-italic", [this] {
    if (page_stack_.get_visible_child_name() == "editor")
      apply_text_style(project::DocumentTextStyle::Italic);
  });
  window.add_action("writing-underline", [this] {
    if (page_stack_.get_visible_child_name() == "editor")
      apply_text_style(project::DocumentTextStyle::Underline);
  });
  new_document_button_.set_action_name("win.create-document");
  save_button_.set_action_name("win.save-document");
}

void WritingWorkspace::refresh() {
  if (!service_.current()) {
    reset();
    return;
  }
  refresh_placement_options();
  refresh_library_filter_options();
  // O Documento aberto já é o estado autoritativo do editor nesta sessão e é
  // descarregado explicitamente ao trocar de Projeto. Reaplicar texto,
  // formatação, âncoras e análise em toda volta ao workspace causava pausas de
  // segundos e também perturbava a geometria da janela sem trazer dado novo.
  // O editor e a Biblioteca são páginas mutuamente exclusivas. Enquanto um
  // Documento está aberto, não há benefício em reconstruir a grade oculta;
  // ela é atualizada normalmente assim que o usuário retorna à Biblioteca.
  if (page_stack_.get_visible_child_name() == "library")
    refresh_library();
}

void WritingWorkspace::reset() {
  autosave_connection_.disconnect();
  analysis_connection_.disconnect();
  undo_group_connection_.disconnect();
  loading_editor_ = true;
  clear_anchor_marks();
  editor_.get_buffer()->set_text("");
  title_entry_.set_text("");
  placement_.set_active_id("none");
  open_placement_button_.set_sensitive(false);
  goal_enabled_.set_active(false);
  word_goal_.set_value(1000);
  loading_editor_ = false;
  dirty_ = false;
  current_document_.reset();
  documents_.clear();
  placement_options_.clear();
  placement_labels_.clear();
  document_groups_.clear();
  current_group_id_.reset();
  entity_references_.clear();
  undo_stack_.clear();
  redo_stack_.clear();
  undo_baseline_.reset();
  selected_anchor_id_.reset();
  selected_entity_reference_id_.reset();
  while (auto *child = document_list_.get_first_child())
    document_list_.remove(*child);
  clear_list(anchors_list_);
  clear_list(references_list_);
  clear_list(long_paragraphs_list_);
  page_stack_.set_visible_child("library");
  library_filters_visible_ = false;
  set_overlay_revealer_open(library_filters_revealer_, false);
  tools_visible_ = false;
  set_overlay_revealer_open(tools_revealer_, false);
  update_undo_actions();
}

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
    context.push_back("entidade " + entity_label(*query.entity_id));
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
    context.push_back("somente revisões documentais");
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
        (document.entity_reference_count == 1 ? " entidade" : " entidades"));
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
        "Crie um Documento livre agora. Você pode situá-lo na estrutura "
        "editorial quando isso for útil.");
  }
}

void WritingWorkspace::refresh_library_filter_options() {
  refreshing_library_controls_ = true;
  const auto active = placement_filter_.get_active_id().raw();
  placement_filter_.remove_all();
  placement_filter_.append("all", "Todas as colocações editoriais");
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
  if (!current_group_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  const auto id = *current_group_id_;
  auto *dialog = new OverlayDialog(*owner, "Remover este grupo da Biblioteca?",
                                   false, Gtk::MessageType::QUESTION,
                                   Gtk::ButtonsType::NONE, true);
  dialog->set_title("Remover grupo");
  dialog->set_secondary_text(
      "Os Documentos permanecem íntegros e voltam para a raiz da Biblioteca.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Remover grupo", Gtk::ResponseType::REJECT);
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::REJECT) {
      try {
        service_.writing().delete_document_group(id);
        current_group_id_.reset();
        refresh_library_filter_options();
        refresh_library();
      } catch (const std::exception &error) {
        show_error("Não foi possível remover o grupo", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
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
  placement_.append("none", "Sem colocação editorial");
  for (const auto &[id, label] : placement_options_)
    placement_.append(id, label);
  placement_.set_active_id(active.empty() ? "none" : active);
  if (placement_.get_active_row_number() < 0)
    placement_.set_active_id("none");
  loading_editor_ = previous_loading;
}

void WritingWorkspace::show_library() {
  try {
    flush_changes();
    current_document_.reset();
    page_stack_.set_visible_child("library");
    refresh_library();
    library_filters_button_.grab_focus();
    signal_status_message_.emit("Biblioteca de Documentos ativa");
  } catch (const std::exception &error) {
    show_error("Não foi possível salvar o Documento", error);
  }
}

void WritingWorkspace::open_document(const std::string &id) {
  try {
    flush_changes();
    const auto value = service_.writing().document(id);
    if (!value)
      throw std::runtime_error("Documento não encontrado");
    current_document_ = value;
    restore_editor_state(*value);
    dirty_ = false;
    undo_stack_.clear();
    redo_stack_.clear();
    undo_baseline_ = capture_editor_state();
    update_editor_status();
    update_undo_actions();
    page_stack_.set_visible_child("editor");
    editor_.grab_focus();
    signal_status_message_.emit("Documento aberto para Escrita");
  } catch (const std::exception &error) {
    loading_editor_ = false;
    show_error("Não foi possível abrir o Documento", error);
  }
}

void WritingWorkspace::reveal_document(const std::string &id) {
  open_document(id);
}

void WritingWorkspace::show_library_for_editorial_node(
    const std::string &node_id) {
  try {
    flush_changes();
    current_document_.reset();
    current_group_id_.reset();
    refresh_library_filter_options();
    search_.set_text("");
    if (entity_filter_)
      entity_filter_->clear_selection();
    if (!placement_filter_.set_active_id(node_id))
      throw std::runtime_error("Unidade editorial não encontrada");
    page_stack_.set_visible_child("library");
    refresh_library();
    signal_status_message_.emit("Biblioteca filtrada pela unidade editorial");
  } catch (const std::exception &error) {
    show_error("Não foi possível abrir a Biblioteca", error);
  }
}

void WritingWorkspace::show_library_for_entity(const std::string &entity_id) {
  try {
    flush_changes();
    current_document_.reset();
    current_group_id_.reset();
    search_.set_text("");
    placement_filter_.set_active_id("all");
    if (!entity_filter_)
      throw std::runtime_error("Filtro de entidade indisponível");
    entity_filter_->set_selected(entity_id, entity_label(entity_id));
    page_stack_.set_visible_child("library");
    refresh_library();
    signal_status_message_.emit("Biblioteca filtrada pela entidade");
  } catch (const std::exception &error) {
    show_error("Não foi possível abrir a Biblioteca", error);
  }
}

void WritingWorkspace::create_document() {
  std::optional<std::string> placement;
  const auto selected = placement_filter_.get_active_id().raw();
  if (!selected.empty() && selected != "all")
    placement = selected;
  create_document_at(std::move(placement));
}

void WritingWorkspace::create_document_for_editorial_node(
    const std::string &node_id) {
  create_document_at(node_id);
}

void WritingWorkspace::create_document_at(
    std::optional<std::string> editorial_node_id) {
  auto *owner = owner_window();
  if (!owner || !service_.current())
    return;
  auto *dialog = new OverlayDialog("Novo Documento", *owner, true);
  dialog->set_secondary_text(
      editorial_node_id
          ? "O Documento será criado na unidade editorial selecionada."
          : "O Documento nasce livre e pode receber colocação editorial no "
            "editor.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar e escrever", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *title = Gtk::make_managed<Gtk::Entry>();
  title->set_placeholder_text("Nome do Documento");
  form->set_margin(16);
  append_labeled_form_field(*form, "Nome", *title, "Campo obrigatório.");
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, title,
       editorial_node_id = std::move(editorial_node_id)](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            const auto created = service_.writing().create_document(
                title->get_text(), editorial_node_id);
            auto organized = created;
            if (current_group_id_) {
              organized.group_id = current_group_id_;
              organized = service_.writing().update_document(organized);
            }
            dialog->hide();
            refresh_library();
            open_document(organized.id);
            return;
          } catch (const std::exception &error) {
            show_error("Não foi possível criar o Documento", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void WritingWorkspace::edit_document_organization() {
  if (!current_document_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto original = capture_editor_state();
  auto *dialog = new OverlayDialog("Organização do Documento", *owner, true);
  dialog->set_secondary_text(
      "Finalidade, grupo, perspectiva e origem de revisão são metadados; o "
      "texto e a colocação editorial permanecem independentes.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar organização", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
  auto *purpose = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto value :
       {project::DocumentPurpose::MainText,
        project::DocumentPurpose::Annotation,
        project::DocumentPurpose::Revision, project::DocumentPurpose::Outline,
        project::DocumentPurpose::Research, project::DocumentPurpose::Reference,
        project::DocumentPurpose::Other})
    purpose->append(project::to_string(value), project::display_name(value));
  purpose->set_active_id(project::to_string(original.purpose));
  auto *group = Gtk::make_managed<Gtk::ComboBoxText>();
  group->append("none", "Sem grupo — raiz da Biblioteca");
  for (const auto &value : service_.writing().document_groups())
    group->append(value.id, value.name);
  group->set_active_id(original.group_id.value_or("none"));
  auto *perspective = Gtk::make_managed<Gtk::Entry>();
  perspective->set_text(original.perspective);
  perspective->set_placeholder_text("Ex.: ponto de vista de Aurora");
  auto *revision = Gtk::make_managed<Gtk::ComboBoxText>();
  revision->append("none", "Não é revisão de outro Documento");
  persistence::DocumentQuery query;
  query.limit = 500;
  for (const auto &value : service_.writing().document_summaries(query)) {
    if (value.id != original.id)
      revision->append(value.id, value.title);
  }
  revision->set_active_id(original.revision_of_id.value_or("none"));
  auto *revision_label = Gtk::make_managed<Gtk::Entry>();
  revision_label->set_text(original.revision_label);
  revision_label->set_placeholder_text("Ex.: v2 — revisão de ritmo");
  form->set_margin(16);
  append_labeled_form_field(*form, "Finalidade", *purpose,
                            "Classifica o uso principal deste Documento.");
  append_labeled_form_field(
      *form, "Grupo da Biblioteca", *group,
      "Organização virtual, sem mover arquivos no disco.");
  append_labeled_form_field(
      *form, "Perspectiva", *perspective,
      "Texto livre para busca e leitura por ponto de vista.");
  append_labeled_form_field(
      *form, "Revisão de", *revision,
      "Origem documental explícita; não é uma versão editorial publicada.");
  append_labeled_form_field(*form, "Rótulo da revisão", *revision_label);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, purpose, group,
                                     perspective, revision,
                                     revision_label](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        original.purpose = project::document_purpose_from_string(
            purpose->get_active_id().raw());
        const auto group_id = group->get_active_id().raw();
        original.group_id = group_id.empty() || group_id == "none"
                                ? std::nullopt
                                : std::optional<std::string>{group_id};
        original.perspective = perspective->get_text();
        const auto source_id = revision->get_active_id().raw();
        original.revision_of_id = source_id.empty() || source_id == "none"
                                      ? std::nullopt
                                      : std::optional<std::string>{source_id};
        original.revision_label = revision_label->get_text();
        if (original.revision_of_id &&
            original.purpose == project::DocumentPurpose::MainText)
          original.purpose = project::DocumentPurpose::Revision;
        current_document_ = service_.writing().update_document(original);
        dirty_ = false;
        refresh_library_filter_options();
        signal_status_message_.emit("Organização documental salva");
      } catch (const std::exception &error) {
        show_error("Não foi possível organizar o Documento", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void WritingWorkspace::flush_changes() {
  autosave_connection_.disconnect();
  if (!dirty_ || !current_document_)
    return;
  commit_undo_group();
  auto value = capture_editor_state();
  current_document_ = service_.writing().update_document(value);
  dirty_ = false;
  undo_baseline_ = capture_editor_state();
  update_editor_status();
}

void WritingWorkspace::save_document() {
  if (!current_document_)
    return;
  try {
    flush_changes();
    signal_status_message_.emit("Documento salvo");
  } catch (const std::exception &error) {
    show_error("Não foi possível salvar o Documento", error);
  }
}

void WritingWorkspace::delete_document() {
  if (!current_document_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  const auto id = current_document_->id;
  const auto title = current_document_->title;
  auto *dialog = new OverlayDialog(
      *owner, "Remover permanentemente o Documento “" + title + "”?", false,
      Gtk::MessageType::QUESTION, Gtk::ButtonsType::NONE, true);
  dialog->set_title("Remover Documento");
  dialog->set_secondary_text(
      "O texto, sua formatação, âncoras e vínculos serão removidos. A unidade "
      "editorial e as entidades permanecem intactas.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Remover", Gtk::ResponseType::REJECT);
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::REJECT) {
      try {
        autosave_connection_.disconnect();
        service_.writing().delete_document(id);
        current_document_.reset();
        dirty_ = false;
        clear_anchor_marks();
        entity_references_.clear();
        page_stack_.set_visible_child("library");
        refresh_library();
        signal_status_message_.emit("Documento removido");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover o Documento", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

Glib::RefPtr<Gtk::TextTag>
WritingWorkspace::tag_for_style(project::DocumentTextStyle style) const {
  const auto found =
      std::find_if(formatting_tags_.begin(), formatting_tags_.end(),
                   [&](const auto &entry) { return entry.first == style; });
  if (found == formatting_tags_.end())
    throw std::runtime_error("Estilo de formatação indisponível");
  return found->second;
}

void WritingWorkspace::apply_text_style(project::DocumentTextStyle style) {
  if (!current_document_)
    return;
  auto buffer = editor_.get_buffer();
  Gtk::TextBuffer::iterator begin;
  Gtk::TextBuffer::iterator end;
  if (!buffer->get_selection_bounds(begin, end))
    return;
  const bool paragraph_style =
      style == project::DocumentTextStyle::Heading ||
      style == project::DocumentTextStyle::Subheading ||
      style == project::DocumentTextStyle::Quote;
  const auto tag = tag_for_style(style);
  const bool was_active = begin.has_tag(tag);
  if (paragraph_style) {
    begin.set_line_offset(0);
    if (!end.starts_line())
      end.forward_to_line_end();
    for (const auto paragraph : {project::DocumentTextStyle::Heading,
                                 project::DocumentTextStyle::Subheading,
                                 project::DocumentTextStyle::Quote})
      buffer->remove_tag(tag_for_style(paragraph), begin, end);
  }
  if (was_active)
    buffer->remove_tag(tag, begin, end);
  else
    buffer->apply_tag(tag, begin, end);
  mark_dirty();
  refresh_formatting_toolbar();
}

void WritingWorkspace::clear_text_formatting() {
  if (!current_document_)
    return;
  auto buffer = editor_.get_buffer();
  Gtk::TextBuffer::iterator begin;
  Gtk::TextBuffer::iterator end;
  if (!buffer->get_selection_bounds(begin, end))
    return;
  for (const auto &[style, tag] : formatting_tags_)
    buffer->remove_tag(tag, begin, end);
  mark_dirty();
  refresh_formatting_toolbar();
}

bool WritingWorkspace::style_active_at_selection(
    project::DocumentTextStyle style, bool &mixed) {
  mixed = false;
  if (!current_document_)
    return false;
  const auto buffer = editor_.get_buffer();
  const auto tag = tag_for_style(style);
  Gtk::TextBuffer::iterator begin;
  Gtk::TextBuffer::iterator end;
  if (!buffer->get_selection_bounds(begin, end)) {
    auto cursor = buffer->get_iter_at_mark(buffer->get_insert());
    if (cursor.has_tag(tag))
      return true;
    // Em uma fronteira de estilo, o cursor pertence visualmente ao trecho
    // imediatamente anterior até que o autor digite algo novo.
    auto previous = cursor;
    return previous.backward_char() && previous.has_tag(tag);
  }

  bool has_active = false;
  bool has_inactive = false;
  auto cursor = begin;
  while (cursor.compare(end) < 0) {
    if (cursor.has_tag(tag))
      has_active = true;
    else
      has_inactive = true;
    if (has_active && has_inactive)
      break;
    auto next = cursor;
    if (!next.forward_to_tag_toggle(tag) || next.compare(end) >= 0)
      break;
    cursor = next;
  }
  mixed = has_active && has_inactive;
  return has_active && !has_inactive;
}

void WritingWorkspace::refresh_formatting_toolbar() {
  const auto update = [this](Gtk::Button &button,
                             project::DocumentTextStyle style,
                             const char *description) {
    bool mixed = false;
    const bool active = style_active_at_selection(style, mixed);
    button.remove_css_class("format-active");
    button.remove_css_class("format-mixed");
    if (active)
      button.add_css_class("format-active");
    else if (mixed)
      button.add_css_class("format-mixed");
    button.set_tooltip_text(
        Glib::ustring(description) +
        (active ? " — ativo neste trecho"
                : mixed ? " — parcialmente ativo na seleção" : ""));
  };
  update(bold_button_, project::DocumentTextStyle::Bold, "Negrito na seleção");
  update(italic_button_, project::DocumentTextStyle::Italic,
         "Itálico na seleção");
  update(underline_button_, project::DocumentTextStyle::Underline,
         "Sublinhado na seleção");
  update(strike_button_, project::DocumentTextStyle::Strikethrough,
         "Tachado na seleção");
  update(heading_button_, project::DocumentTextStyle::Heading,
         "Título no parágrafo selecionado");
  update(subheading_button_, project::DocumentTextStyle::Subheading,
         "Subtítulo no parágrafo selecionado");
  update(quote_button_, project::DocumentTextStyle::Quote,
         "Citação no parágrafo selecionado");

  bool any_format = false;
  for (const auto &[style, tag] : formatting_tags_) {
    bool mixed = false;
    any_format = any_format || style_active_at_selection(style, mixed) || mixed;
  }
  clear_format_button_.remove_css_class("format-active");
  if (any_format)
    clear_format_button_.add_css_class("format-active");
}

std::vector<project::DocumentFormatSpan>
WritingWorkspace::capture_formatting() {
  std::vector<project::DocumentFormatSpan> result;
  auto buffer = editor_.get_buffer();
  const auto total = static_cast<std::size_t>(buffer->get_char_count());
  for (const auto &[style, tag] : formatting_tags_) {
    auto iterator = buffer->begin();
    bool active = iterator.has_tag(tag);
    std::size_t start = active ? 0 : total;
    while (true) {
      auto next = iterator;
      if (!next.forward_to_tag_toggle(tag))
        break;
      const auto offset = static_cast<std::size_t>(next.get_offset());
      if (active && start < offset)
        result.push_back({style, start, offset});
      else if (!active)
        start = offset;
      active = !active;
      iterator = next;
    }
    if (active && start < total)
      result.push_back({style, start, total});
  }
  std::sort(
      result.begin(), result.end(), [](const auto &left, const auto &right) {
        if (left.start_offset != right.start_offset)
          return left.start_offset < right.start_offset;
        if (left.end_offset != right.end_offset)
          return left.end_offset < right.end_offset;
        return project::to_string(left.style) < project::to_string(right.style);
      });
  return result;
}

void WritingWorkspace::restore_formatting(
    const std::vector<project::DocumentFormatSpan> &formatting) {
  auto buffer = editor_.get_buffer();
  buffer->remove_all_tags(buffer->begin(), buffer->end());
  const auto total = static_cast<std::size_t>(buffer->get_char_count());
  for (const auto &span : formatting) {
    if (span.start_offset >= span.end_offset || span.end_offset > total)
      continue;
    buffer->apply_tag(
        tag_for_style(span.style),
        buffer->get_iter_at_offset(static_cast<int>(span.start_offset)),
        buffer->get_iter_at_offset(static_cast<int>(span.end_offset)));
  }
  refresh_formatting_toolbar();
}

void WritingWorkspace::toggle_tools() {
  tools_visible_ = !tools_visible_;
  set_overlay_revealer_open(tools_revealer_, tools_visible_);
  tools_button_.set_label(tools_visible_ ? "Fechar ferramentas"
                                         : "Ferramentas");
  if (tools_visible_)
    refresh_text_tools();
}

void WritingWorkspace::refresh_text_tools() {
  current_statistics_ = project::analyze_document_text(
      editor_.get_buffer()->get_text(), reading_speed, 120);
  stats_words_.set_text(
      std::to_string(current_statistics_.words) +
      (current_statistics_.words == 1 ? " palavra" : " palavras"));
  stats_characters_.set_text(
      std::to_string(current_statistics_.characters_with_spaces) +
      " caracteres com espaços  •  " +
      std::to_string(current_statistics_.characters_without_spaces) +
      " sem espaços");
  stats_structure_.set_text(
      std::to_string(current_statistics_.paragraphs) +
      (current_statistics_.paragraphs == 1 ? " parágrafo" : " parágrafos") +
      "  •  " + std::to_string(current_statistics_.sentences) +
      (current_statistics_.sentences == 1 ? " frase" : " frases"));
  stats_reading_time_.set_text(
      current_statistics_.reading_minutes == 0
          ? "Tempo de leitura: menos de 1 minuto (200 palavras/minuto)"
          : "Tempo de leitura: cerca de " +
                std::to_string(current_statistics_.reading_minutes) +
                (current_statistics_.reading_minutes == 1 ? " minuto"
                                                          : " minutos") +
                " (200 palavras/minuto)");
  word_count_.set_text(
      std::to_string(current_statistics_.words) +
      (current_statistics_.words == 1 ? " palavra" : " palavras"));
  if (current_statistics_.repeated_words.empty()) {
    repeated_words_.set_text("Nenhuma recorrência relevante nesta amostra.");
  } else {
    std::string text;
    for (std::size_t index = 0;
         index < current_statistics_.repeated_words.size(); ++index) {
      if (index != 0)
        text += "  •  ";
      text += current_statistics_.repeated_words[index].word + " (" +
              std::to_string(current_statistics_.repeated_words[index].count) +
              ")";
    }
    repeated_words_.set_text(text);
  }
  refresh_long_paragraphs(current_statistics_.long_paragraphs);
  update_goal_state();
}

void WritingWorkspace::update_goal_state() {
  word_goal_.set_sensitive(goal_enabled_.get_active());
  if (!goal_enabled_.get_active()) {
    goal_progress_.set_fraction(0.0);
    goal_progress_.set_text("Sem meta definida");
    return;
  }
  const auto goal = static_cast<std::size_t>(word_goal_.get_value_as_int());
  const auto fraction =
      goal == 0 ? 0.0
                : std::min(1.0, static_cast<double>(current_statistics_.words) /
                                    static_cast<double>(goal));
  goal_progress_.set_fraction(fraction);
  goal_progress_.set_text(std::to_string(current_statistics_.words) + " / " +
                          std::to_string(goal) + " palavras");
}

void WritingWorkspace::refresh_long_paragraphs(
    const std::vector<project::LongParagraph> &paragraphs) {
  clear_list(long_paragraphs_list_);
  if (paragraphs.empty()) {
    auto *empty = Gtk::make_managed<Gtk::Label>(
        "Nenhum parágrafo com 120 palavras ou mais.");
    prepare_tool_label(*empty);
    empty->set_margin(8);
    long_paragraphs_list_.append(*empty);
    return;
  }
  for (const auto &paragraph : paragraphs) {
    auto *button = Gtk::make_managed<Gtk::Button>(
        "Parágrafo " + std::to_string(paragraph.paragraph_number) + " — " +
        std::to_string(paragraph.word_count) + " palavras");
    button->set_halign(Gtk::Align::FILL);
    button->signal_clicked().connect([this, offset = paragraph.start_offset] {
      navigate_to_offset(offset);
    });
    long_paragraphs_list_.append(*button);
  }
}

void WritingWorkspace::navigate_to_offset(std::size_t offset) {
  auto buffer = editor_.get_buffer();
  const auto safe =
      std::min(offset, static_cast<std::size_t>(buffer->get_char_count()));
  auto iterator = buffer->get_iter_at_offset(static_cast<int>(safe));
  buffer->place_cursor(iterator);
  editor_.scroll_to(iterator, 0.2);
  editor_.grab_focus();
}

void WritingWorkspace::clear_anchor_marks() {
  auto buffer = editor_.get_buffer();
  for (auto &anchor : anchor_marks_) {
    if (anchor.start)
      buffer->delete_mark(anchor.start);
    if (anchor.end)
      buffer->delete_mark(anchor.end);
  }
  anchor_marks_.clear();
  selected_anchor_id_.reset();
}

void WritingWorkspace::restore_anchor_marks(
    const std::vector<project::DocumentAnchor> &anchors) {
  clear_anchor_marks();
  auto buffer = editor_.get_buffer();
  const auto total = static_cast<std::size_t>(buffer->get_char_count());
  for (const auto &anchor : anchors) {
    const auto start = std::min(anchor.start_offset, total);
    const auto end = std::min(anchor.end_offset, total);
    anchor_marks_.push_back(
        {anchor.id, anchor.label,
         buffer->create_mark(
             "anchor-start-" + anchor.id,
             buffer->get_iter_at_offset(static_cast<int>(start)), true),
         buffer->create_mark("anchor-end-" + anchor.id,
                             buffer->get_iter_at_offset(static_cast<int>(end)),
                             false)});
  }
}

std::vector<project::DocumentAnchor> WritingWorkspace::capture_anchors() {
  std::vector<project::DocumentAnchor> result;
  auto buffer = editor_.get_buffer();
  for (const auto &anchor : anchor_marks_) {
    const auto start = buffer->get_iter_at_mark(anchor.start).get_offset();
    const auto end = buffer->get_iter_at_mark(anchor.end).get_offset();
    result.push_back(
        {anchor.id, anchor.label,
         static_cast<std::size_t>(std::max(0, std::min(start, end))),
         static_cast<std::size_t>(std::max(0, std::max(start, end)))});
  }
  std::sort(result.begin(), result.end(),
            [](const auto &left, const auto &right) {
              if (left.start_offset != right.start_offset)
                return left.start_offset < right.start_offset;
              return left.label < right.label;
            });
  return result;
}

void WritingWorkspace::refresh_anchor_list() {
  clear_list(anchors_list_);
  for (const auto &anchor : capture_anchors()) {
    auto *button = Gtk::make_managed<Gtk::Button>(anchor.label);
    button->set_halign(Gtk::Align::FILL);
    button->set_tooltip_text(
        anchor.start_offset == anchor.end_offset
            ? "Posição " + std::to_string(anchor.start_offset)
            : "Trecho " + std::to_string(anchor.start_offset) + "–" +
                  std::to_string(anchor.end_offset));
    button->signal_clicked().connect([this, id = anchor.id] {
      select_anchor(id);
      open_selected_anchor();
    });
    anchors_list_.append(*button);
  }
  if (anchor_marks_.empty()) {
    auto *empty = Gtk::make_managed<Gtk::Label>(
        "Selecione um trecho ou posicione o cursor para criar uma âncora.");
    prepare_tool_label(*empty);
    empty->set_margin(8);
    anchors_list_.append(*empty);
  }
  const bool selected = selected_anchor_id_.has_value();
  rename_anchor_button_.set_sensitive(selected);
  remove_anchor_button_.set_sensitive(selected);
}

void WritingWorkspace::select_anchor(std::string id) {
  selected_anchor_id_ = std::move(id);
  refresh_anchor_list();
}

void WritingWorkspace::open_selected_anchor() {
  if (!selected_anchor_id_)
    return;
  const auto found = std::find_if(
      anchor_marks_.begin(), anchor_marks_.end(),
      [&](const auto &anchor) { return anchor.id == *selected_anchor_id_; });
  if (found == anchor_marks_.end())
    return;
  auto buffer = editor_.get_buffer();
  auto start = buffer->get_iter_at_mark(found->start);
  auto end = buffer->get_iter_at_mark(found->end);
  if (start.get_offset() == end.get_offset())
    buffer->place_cursor(start);
  else
    buffer->select_range(start, end);
  editor_.scroll_to(start, 0.2);
  editor_.grab_focus();
}

void WritingWorkspace::add_anchor() {
  if (!current_document_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto buffer = editor_.get_buffer();
  Gtk::TextBuffer::iterator start;
  Gtk::TextBuffer::iterator end;
  if (!buffer->get_selection_bounds(start, end)) {
    start = buffer->get_iter_at_mark(buffer->get_insert());
    end = start;
  }
  const auto start_offset = start.get_offset();
  const auto end_offset = end.get_offset();
  auto *dialog = new OverlayDialog("Nova âncora textual", *owner, true);
  dialog->set_secondary_text(
      start_offset == end_offset
          ? "A âncora acompanhará esta posição durante a edição."
          : "A âncora acompanhará o trecho selecionado durante a edição.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar âncora", Gtk::ResponseType::ACCEPT);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome da âncora");
  name->set_margin(16);
  dialog->get_content_area()->append(*name);
  dialog->signal_response().connect(
      [this, dialog, name, start_offset, end_offset](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            if (name->get_text().empty())
              throw std::runtime_error("O nome da âncora é obrigatório");
            const auto id = project::new_uuid();
            auto buffer = editor_.get_buffer();
            anchor_marks_.push_back(
                {id, name->get_text(),
                 buffer->create_mark("anchor-start-" + id,
                                     buffer->get_iter_at_offset(start_offset),
                                     true),
                 buffer->create_mark("anchor-end-" + id,
                                     buffer->get_iter_at_offset(end_offset),
                                     false)});
            selected_anchor_id_ = id;
            refresh_anchor_list();
            mark_dirty();
          } catch (const std::exception &error) {
            show_error("Não foi possível criar a âncora", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void WritingWorkspace::rename_anchor() {
  if (!selected_anchor_id_)
    return;
  auto found = std::find_if(
      anchor_marks_.begin(), anchor_marks_.end(),
      [&](const auto &anchor) { return anchor.id == *selected_anchor_id_; });
  auto *owner = owner_window();
  if (!owner || found == anchor_marks_.end())
    return;
  auto *dialog = new OverlayDialog("Renomear âncora", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text(found->label);
  name->set_margin(16);
  dialog->get_content_area()->append(*name);
  const auto id = found->id;
  dialog->signal_response().connect([this, dialog, name, id](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (name->get_text().empty())
          throw std::runtime_error("O nome da âncora é obrigatório");
        auto current =
            std::find_if(anchor_marks_.begin(), anchor_marks_.end(),
                         [&](const auto &anchor) { return anchor.id == id; });
        if (current != anchor_marks_.end())
          current->label = name->get_text();
        refresh_anchor_list();
        refresh_entity_references();
        mark_dirty();
      } catch (const std::exception &error) {
        show_error("Não foi possível renomear a âncora", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void WritingWorkspace::remove_anchor() {
  if (!selected_anchor_id_)
    return;
  const auto id = *selected_anchor_id_;
  auto found =
      std::find_if(anchor_marks_.begin(), anchor_marks_.end(),
                   [&](const auto &anchor) { return anchor.id == id; });
  if (found == anchor_marks_.end())
    return;
  auto buffer = editor_.get_buffer();
  buffer->delete_mark(found->start);
  buffer->delete_mark(found->end);
  anchor_marks_.erase(found);
  for (auto &reference : entity_references_) {
    if (reference.anchor_id == id)
      reference.anchor_id.reset();
  }
  selected_anchor_id_.reset();
  refresh_anchor_list();
  refresh_entity_references();
  mark_dirty();
}

std::optional<std::string> WritingWorkspace::selected_anchor_label(
    const std::optional<std::string> &id) const {
  if (!id)
    return std::nullopt;
  const auto found =
      std::find_if(anchor_marks_.begin(), anchor_marks_.end(),
                   [&](const auto &anchor) { return anchor.id == *id; });
  return found == anchor_marks_.end()
             ? std::optional<std::string>{"Âncora indisponível"}
             : std::optional<std::string>{found->label};
}

void WritingWorkspace::refresh_entity_references() {
  clear_list(references_list_);
  for (const auto &reference : entity_references_) {
    auto label = entity_label(reference.entity_id);
    if (const auto anchor = selected_anchor_label(reference.anchor_id))
      label += " — em “" + *anchor + "”";
    else
      label += " — Documento inteiro";
    auto *button = Gtk::make_managed<Gtk::Button>(label);
    button->set_halign(Gtk::Align::FILL);
    button->set_tooltip_text(reference.notes.empty()
                                 ? "Referência explícita criada pelo usuário"
                                 : reference.notes);
    button->signal_clicked().connect(
        [this, id = reference.id] { select_entity_reference(id); });
    references_list_.append(*button);
  }
  if (entity_references_.empty()) {
    auto *empty = Gtk::make_managed<Gtk::Label>(
        "Nenhuma entidade foi vinculada explicitamente ao Documento.");
    prepare_tool_label(*empty);
    empty->set_margin(8);
    references_list_.append(*empty);
  }
  const bool selected = selected_entity_reference_id_.has_value();
  open_reference_button_.set_sensitive(selected);
  remove_reference_button_.set_sensitive(selected);
}

void WritingWorkspace::select_entity_reference(std::string id) {
  selected_entity_reference_id_ = std::move(id);
  refresh_entity_references();
}

void WritingWorkspace::add_entity_reference() {
  if (!current_document_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog("Vincular entidade ao Documento", *owner, true);
  dialog->set_secondary_text(
      "O vínculo é explícito e não altera o texto nem os dados da entidade.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Vincular", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *entity = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar entidade",
      [this](const std::string &search, std::size_t limit) {
        std::vector<IncrementalSelection> result;
        persistence::EntityQuery query;
        query.search = search;
        query.limit = std::min(limit, std::size_t{50});
        for (const auto &value : service_.narrative().entities(query))
          result.push_back({value.id, value.name, value.summary});
        return result;
      });
  auto *anchor = Gtk::make_managed<Gtk::ComboBoxText>();
  anchor->append("none", "Documento inteiro");
  for (const auto &value : capture_anchors())
    anchor->append(value.id, value.label);
  anchor->set_active_id(selected_anchor_id_.value_or("none"));
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_placeholder_text("Nota opcional sobre o uso");
  form->set_margin(16);
  append_labeled_form_field(*form, "Entidade", *entity,
                            "Pesquise e escolha uma entidade narrativa.");
  append_labeled_form_field(
      *form, "Âncora textual", *anchor,
      "Use Documento inteiro quando não houver trecho específico.");
  append_labeled_form_field(*form, "Nota", *notes);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, entity, anchor,
                                     notes](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (!entity->selected_id())
          throw std::runtime_error("Escolha uma entidade para vincular");
        const auto anchor_id = anchor->get_active_id().raw();
        project::DocumentEntityReference reference{
            project::new_uuid(), *entity->selected_id(),
            anchor_id.empty() || anchor_id == "none"
                ? std::nullopt
                : std::optional<std::string>{anchor_id},
            notes->get_text()};
        const auto duplicate =
            std::find_if(entity_references_.begin(), entity_references_.end(),
                         [&](const auto &existing) {
                           return existing.entity_id == reference.entity_id &&
                                  existing.anchor_id == reference.anchor_id;
                         });
        if (duplicate != entity_references_.end())
          throw std::runtime_error(
              "A entidade já está vinculada ao mesmo contexto");
        entity_references_.push_back(reference);
        selected_entity_reference_id_ = reference.id;
        refresh_entity_references();
        mark_dirty();
      } catch (const std::exception &error) {
        show_error("Não foi possível vincular a entidade", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void WritingWorkspace::remove_entity_reference() {
  if (!selected_entity_reference_id_)
    return;
  const auto id = *selected_entity_reference_id_;
  entity_references_.erase(
      std::remove_if(entity_references_.begin(), entity_references_.end(),
                     [&](const auto &reference) { return reference.id == id; }),
      entity_references_.end());
  selected_entity_reference_id_.reset();
  refresh_entity_references();
  mark_dirty();
}

void WritingWorkspace::open_reference_entity() {
  if (!selected_entity_reference_id_)
    return;
  const auto found =
      std::find_if(entity_references_.begin(), entity_references_.end(),
                   [&](const auto &reference) {
                     return reference.id == *selected_entity_reference_id_;
                   });
  if (found == entity_references_.end())
    return;
  try {
    flush_changes();
    signal_entity_source_requested_.emit(found->entity_id);
  } catch (const std::exception &error) {
    show_error("Não foi possível salvar antes de navegar", error);
  }
}

project::Document WritingWorkspace::capture_editor_state() {
  if (!current_document_)
    throw std::runtime_error("Nenhum Documento está aberto");
  auto value = *current_document_;
  value.title = title_entry_.get_text();
  value.content = editor_.get_buffer()->get_text();
  const auto placement = placement_.get_active_id().raw();
  value.editorial_node_id = placement.empty() || placement == "none"
                                ? std::nullopt
                                : std::optional<std::string>{placement};
  value.word_goal = goal_enabled_.get_active()
                        ? std::optional<std::size_t>{static_cast<std::size_t>(
                              word_goal_.get_value_as_int())}
                        : std::nullopt;
  value.formatting = capture_formatting();
  value.anchors = capture_anchors();
  value.entity_references = entity_references_;
  return value;
}

void WritingWorkspace::restore_editor_state(const project::Document &state) {
  loading_editor_ = true;
  restoring_undo_ = true;
  title_entry_.set_text(state.title);
  editor_.get_buffer()->set_text(state.content);
  if (!placement_.set_active_id(
          state.editorial_node_id ? *state.editorial_node_id : "none"))
    placement_.set_active_id("none");
  const auto placement = placement_.get_active_id().raw();
  open_placement_button_.set_sensitive(!placement.empty() &&
                                       placement != "none");
  goal_enabled_.set_active(state.word_goal.has_value());
  word_goal_.set_value(
      static_cast<double>(state.word_goal.value_or(std::size_t{1000})));
  restore_formatting(state.formatting);
  restore_anchor_marks(state.anchors);
  entity_references_ = state.entity_references;
  selected_anchor_id_.reset();
  selected_entity_reference_id_.reset();
  restoring_undo_ = false;
  loading_editor_ = false;
  refresh_anchor_list();
  refresh_entity_references();
  refresh_text_tools();
}

void WritingWorkspace::mark_dirty() {
  if (loading_editor_ || restoring_undo_ || !current_document_)
    return;
  dirty_ = true;
  // Qualquer mutação nova cria um ramo de edição e invalida o refazer antes
  // mesmo de o grupo de digitação ser consolidado.
  redo_stack_.clear();
  update_editor_status();
  undo_group_connection_.disconnect();
  undo_group_connection_ = Glib::signal_timeout().connect(
      [this] {
        commit_undo_group();
        return false;
      },
      650);
  analysis_connection_.disconnect();
  analysis_connection_ = Glib::signal_timeout().connect(
      [this] {
        refresh_text_tools();
        refresh_anchor_list();
        return false;
      },
      260);
  update_undo_actions();
  schedule_autosave();
}

void WritingWorkspace::schedule_autosave() {
  autosave_connection_.disconnect();
  autosave_connection_ = Glib::signal_timeout().connect(
      [this] {
        save_document();
        return false;
      },
      900);
}

void WritingWorkspace::commit_undo_group() {
  const bool group_pending = undo_group_connection_.connected();
  undo_group_connection_.disconnect();
  if (!group_pending || !undo_baseline_ || !current_document_)
    return;
  undo_stack_.push_back(*undo_baseline_);
  const auto maximum = editor_.get_buffer()->get_text().size() > 2 * 1024 * 1024
                           ? std::size_t{10}
                           : std::size_t{50};
  if (undo_stack_.size() > maximum)
    undo_stack_.erase(undo_stack_.begin(),
                      undo_stack_.begin() + static_cast<std::ptrdiff_t>(
                                                undo_stack_.size() - maximum));
  undo_baseline_ = capture_editor_state();
  redo_stack_.clear();
  update_undo_actions();
}

void WritingWorkspace::undo() {
  if (!current_document_ || page_stack_.get_visible_child_name() != "editor")
    return;
  commit_undo_group();
  if (undo_stack_.empty())
    return;
  auto current = capture_editor_state();
  auto target = undo_stack_.back();
  undo_stack_.pop_back();
  redo_stack_.push_back(std::move(current));
  restore_editor_state(target);
  undo_baseline_ = capture_editor_state();
  dirty_ = true;
  update_editor_status();
  update_undo_actions();
  schedule_autosave();
}

void WritingWorkspace::redo() {
  if (!current_document_ || page_stack_.get_visible_child_name() != "editor" ||
      redo_stack_.empty())
    return;
  undo_group_connection_.disconnect();
  auto current = capture_editor_state();
  auto target = redo_stack_.back();
  redo_stack_.pop_back();
  undo_stack_.push_back(std::move(current));
  restore_editor_state(target);
  undo_baseline_ = capture_editor_state();
  dirty_ = true;
  update_editor_status();
  update_undo_actions();
  schedule_autosave();
}

void WritingWorkspace::update_undo_actions() {
  undo_button_.set_sensitive(!undo_stack_.empty() ||
                             undo_group_connection_.connected());
  redo_button_.set_sensitive(!redo_stack_.empty());
}

void WritingWorkspace::update_editor_status() {
  editor_status_.set_text(
      dirty_ ? "Alterações pendentes — salvamento automático" : "Salvo");
  save_button_.set_sensitive(dirty_);
  update_undo_actions();
}

Gtk::Window *WritingWorkspace::owner_window() {
  return dynamic_cast<Gtk::Window *>(get_root());
}

void WritingWorkspace::show_error(const Glib::ustring &title,
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

std::string WritingWorkspace::placement_label(
    const std::optional<std::string> &node_id) const {
  if (!node_id)
    return "Documento livre";
  const auto found = placement_labels_.find(*node_id);
  return found == placement_labels_.end() ? "Colocação editorial indisponível"
                                          : found->second;
}

std::string WritingWorkspace::entity_label(const std::string &entity_id) const {
  const auto entity = service_.narrative().entity(entity_id);
  return entity ? entity->name : "Entidade indisponível";
}

std::string
WritingWorkspace::search_match_label(project::DocumentSearchMatch match) {
  switch (match) {
  case project::DocumentSearchMatch::Name:
    return "Nome";
  case project::DocumentSearchMatch::Content:
    return "Conteúdo";
  case project::DocumentSearchMatch::NameAndContent:
    return "Nome e conteúdo";
  case project::DocumentSearchMatch::None:
    return "";
  }
  return "";
}

} // namespace inde::ui
