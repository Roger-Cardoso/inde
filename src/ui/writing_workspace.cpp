#include "inde/ui/writing_workspace.hpp"

#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include "inde/project/manifest.hpp"
#include "writing_workspace_util.hpp"
#include "editor_diagnostics.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace inde::ui {
namespace {

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
    : Gtk::Box(Gtk::Orientation::VERTICAL), service_(service),
      find_analysis_([](const application::TextAnalysisSnapshot &snapshot, std::stop_token stop) {
        application::TextAnalysisResult result;
        result.search = application::find_text(snapshot.text, *snapshot.search, stop);
        return result;
      }) {
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
  page_stack_.set_transition_type(Gtk::StackTransitionType::NONE);
  page_stack_.set_hhomogeneous(false);page_stack_.set_vhomogeneous(false);
  composition_page_.signal_back().connect([this]{show_library();});
  composition_page_.signal_open_document().connect([this](const std::string &id){open_document(id);});
  composition_page_.signal_narrative_uses().connect([this]{show_narrative_uses(true);});
  compositions_button_.signal_clicked().connect([this]{
    try {flush_changes();close_inline_suggestions();composition_page_.refresh();page_stack_.set_visible_child("compositions");}
    catch(const std::exception &e){show_error("Não foi possível abrir Composições",e);}
  });
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
  library_header_.append(compositions_button_);
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
  placement_filter_.append("all", "Todas as posições no manuscrito");
  placement_filter_.set_active_id("all");
  constrain_combo_text(placement_filter_, 42);
  set_accessible_label(placement_filter_, "Filtrar por elemento do manuscrito");
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
      "Pesquisar elemento da história usada",
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
      "Nenhum elemento selecionado");
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
  placement_.append("none", "Sem posição no manuscrito");
  placement_.set_active_id("none");
  constrain_combo_text(placement_, 20);
  placement_.set_hexpand(false);
  placement_.set_tooltip_text(
      "Situa o Documento sem transformar o elemento do manuscrito no texto.");
  set_accessible_label(placement_, "Colocação editorial do Documento");
  document_menu_.append(placement_);
  open_placement_button_.set_sensitive(false);
  open_placement_button_.add_css_class("flat");
  open_placement_button_.set_tooltip_text(
      "Abrir o detalhe desta unidade no workspace Editorial");
  set_accessible_label(open_placement_button_,
                       "Abrir elemento do manuscrito do Documento");
  document_menu_.append(open_placement_button_);
  document_menu_.append(organization_button_);
  save_button_.add_css_class("suggested-action");
  save_button_.set_sensitive(false);
  document_menu_.append(save_button_);
  auto *export_button=Gtk::make_managed<Gtk::Button>("Exportar Documento…");
  export_button->signal_clicked().connect([this] {document_popover_.popdown();export_document();});
  document_menu_.append(*export_button);
  document_menu_.append(recovery_editor_button_);
  delete_button_.add_css_class("flat");
  delete_button_.add_css_class("quiet-destructive");
  document_menu_.append(delete_button_);
  document_menu_.set_margin(12);
  document_popover_.set_child(document_menu_);
  document_menu_button_.set_icon_name("view-more-symbolic");
  document_menu_button_.set_popover(document_popover_);
  document_menu_button_.set_tooltip_text("Organização, salvamento e recuperação");
  set_accessible_label(document_menu_button_, "Ações do Documento");
  editor_page_.append(editor_toolbar_);

  formatting_toolbar_.add_css_class("toolbar-surface");
  formatting_toolbar_.add_css_class("writing-format-toolbar");
  build_editor_controls();
  bold_button_.set_tooltip_text("Negrito na seleção");
  italic_button_.set_tooltip_text("Itálico na seleção");
  underline_button_.set_tooltip_text("Sublinhado na seleção");
  strike_button_.set_tooltip_text("Tachado na seleção");
  heading_button_.set_tooltip_text("Título no parágrafo selecionado");
  subheading_button_.set_tooltip_text("Subtítulo no parágrafo selecionado");
  quote_button_.set_tooltip_text("Citação no parágrafo selecionado");
  build_clipboard_ui();
  formatting_toolbar_.append(cut_button_);
  build_search_ui();
  formatting_toolbar_.append(consult_button_);
  build_recovery_ui();
  build_semantic_ui();
  // Keep the same authoritative controls, presented on demand in compact rows.
  std::vector<Gtk::Widget *> format_controls;
  for (auto *child = formatting_toolbar_.get_first_child(); child;
       child = child->get_next_sibling()) format_controls.push_back(child);
  for (auto *child : format_controls) formatting_toolbar_.remove(*child);
  formatting_toolbar_.set_orientation(Gtk::Orientation::VERTICAL);
  formatting_toolbar_.set_spacing(8);
  for (std::size_t i = 0; i < format_controls.size(); i += 5) {
    auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    for (auto j = i; j < std::min(i + 5, format_controls.size()); ++j)
      row->append(*format_controls[j]);
    formatting_toolbar_.append(*row);
  }
  format_popover_.set_child(formatting_toolbar_);
  format_menu_button_.set_label("Formatar");
  format_menu_button_.set_popover(format_popover_);
  set_accessible_label(format_menu_button_, "Abrir formatação");
  contextual_toolbar_.set_orientation(Gtk::Orientation::VERTICAL);
  auto *narrative_uses=Gtk::make_managed<Gtk::Button>("Planejamento deste texto…");
  narrative_uses->signal_clicked().connect([this]{context_popover_.popdown();show_narrative_uses();});
  contextual_toolbar_.append(*narrative_uses);
  contextual_toolbar_.set_margin(8);
  contextual_toolbar_.set_spacing(4);
  context_menu_scroll_.set_child(contextual_toolbar_);
  context_menu_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  context_menu_scroll_.set_max_content_height(440);
  context_menu_scroll_.set_propagate_natural_height(true);
  context_popover_.set_child(context_menu_scroll_);
  context_menu_button_.set_label("Ferramentas");
  context_menu_button_.set_popover(context_popover_);
  set_accessible_label(context_menu_button_, "Abrir ferramentas do texto");
  for (auto *child = contextual_toolbar_.get_first_child(); child;
       child = child->get_next_sibling())
    if (auto *button = dynamic_cast<Gtk::Button *>(child))
      button->signal_clicked().connect([this] { context_popover_.popdown(); });
  editor_toolbar_.append(format_menu_button_);
  editor_toolbar_.append(context_menu_button_);
  editor_toolbar_.append(document_menu_button_);

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
    const int prose_margin = std::max(24, (width - 820) / 2);
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
  tools_panel_.append(tools_button_);
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
  linguistic_title_.add_css_class("heading");
  linguistic_title_.set_halign(Gtk::Align::START);
  prepare_tool_label(linguistic_summary_);
  tools_panel_.append(linguistic_separator_);
  tools_panel_.append(linguistic_title_);
  tools_panel_.append(linguistic_summary_);
  tools_panel_.append(semantic_dictionary_button_);
  tools_panel_.append(linguistic_entities_button_);
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
  tools_panel_.append(anchor_usages_button_);
  tools_panel_.append(text_references_button_);
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
  page_stack_.add(composition_page_, "compositions");
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
            ? "Situa o Documento sem transformar o elemento do manuscrito no texto."
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
  const std::weak_ptr<int> buffer_lifetime = clipboard_lifetime_;
  editor_.get_buffer()->signal_changed().connect([this, buffer_lifetime] {
    if (buffer_lifetime.expired()) return;
    const bool restoring = loading_editor_ || restoring_undo_;
    EditorPhaseTimer measurement(restoring ? "buffer_restore_changed" : "signal_changed");
    if (!restoring && editor_diagnostics_enabled() && !editor_frame_tick_) {
      editor_change_started_ = std::chrono::steady_clock::now();
      editor_frame_tick_ = editor_.add_tick_callback([this](const auto &) {
        editor_diagnostic("change_to_frame_tick",
                          std::chrono::steady_clock::now() - editor_change_started_);
        editor_frame_tick_ = 0;
        return false;
      });
    }
    mark_dirty();
    refresh_formatting_toolbar();
    refresh_semantic_lenses();
    schedule_inline_suggestions();
  });
  build_ime_guard();
  auto position_keys=Gtk::EventControllerKey::create();
  position_keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  position_keys->signal_key_pressed().connect([this](guint,guint,Gdk::ModifierType) {position_restore_.disconnect();return false;},false);
  editor_.add_controller(position_keys);
  auto position_click=Gtk::GestureClick::create();
  position_click->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  position_click->signal_pressed().connect([this](int,double,double) {position_restore_.disconnect();});
  editor_.add_controller(position_click);
  auto position_scroll=Gtk::EventControllerScroll::create();
  position_scroll->set_flags(Gtk::EventControllerScroll::Flags::BOTH_AXES);
  position_scroll->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  position_scroll->signal_scroll().connect([this](double,double) {
    position_restore_.disconnect();return false;
  },false);
  editor_.add_controller(position_scroll);
  editor_.get_buffer()->signal_mark_set().connect(
      [this, buffer_lifetime](const Gtk::TextBuffer::iterator &,
             const Glib::RefPtr<Gtk::TextMark> &) {
        if (buffer_lifetime.expired()) return;
        refresh_formatting_toolbar();
        schedule_inline_suggestions();
        if (!loading_editor_ && !restoring_undo_ && current_document_) {
          const auto buffer = editor_.get_buffer();
          edit_session_.update_selection({
              static_cast<std::size_t>(buffer->get_iter_at_mark(buffer->get_insert()).get_offset()),
              static_cast<std::size_t>(buffer->get_iter_at_mark(buffer->get_selection_bound()).get_offset())});
          schedule_position_save();
        }
      });
  editor_.get_buffer()->signal_insert().connect(
      [this, buffer_lifetime](const Gtk::TextBuffer::iterator &position, const Glib::ustring &text, int) {
        if (buffer_lifetime.expired()) return;
        if (!loading_editor_ && !restoring_undo_) { semantic_before_edit(position.get_offset(),position.get_offset(),text.raw());edit_session_.note_mutation(); }
      }, false);
  editor_.get_buffer()->signal_erase().connect(
      [this, buffer_lifetime](const Gtk::TextBuffer::iterator &start, const Gtk::TextBuffer::iterator &end) {
        if (buffer_lifetime.expired()) return;
        if (!loading_editor_ && !restoring_undo_) { semantic_before_edit(start.get_offset(),end.get_offset(),"");edit_session_.note_mutation(); }
      }, false);
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
  semantic_dictionary_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::show_semantic_dictionary));
  linguistic_entities_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::show_linguistic_entities));
  add_anchor_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::add_anchor));
  rename_anchor_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::rename_anchor));
  remove_anchor_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &WritingWorkspace::remove_anchor));
  text_references_button_.signal_clicked().connect(
      [this] { show_text_references(); });
  anchor_usages_button_.signal_clicked().connect([this] {
    if (selected_anchor_id_)
      show_text_references(true, selected_anchor_id_);
  });
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
  for (auto style : {project::DocumentTextStyle::Heading2,project::DocumentTextStyle::Heading3,
       project::DocumentTextStyle::Heading4,project::DocumentTextStyle::Heading5,project::DocumentTextStyle::Heading6}) {
    auto tag=add(style,("inde-"+project::to_string(style)).c_str());
    tag->property_weight()=static_cast<int>(Pango::Weight::BOLD);
    tag->property_scale()=style==project::DocumentTextStyle::Heading2?1.4:style==project::DocumentTextStyle::Heading3?1.3:style==project::DocumentTextStyle::Heading4?1.2:style==project::DocumentTextStyle::Heading5?1.1:1.0;
    tag->property_pixels_above_lines()=10; tag->property_pixels_below_lines()=6;
  }
  add(project::DocumentTextStyle::AlignLeft,"inde-align-left")->property_justification()=Gtk::Justification::LEFT;
  add(project::DocumentTextStyle::AlignCenter,"inde-align-center")->property_justification()=Gtk::Justification::CENTER;
  add(project::DocumentTextStyle::AlignRight,"inde-align-right")->property_justification()=Gtk::Justification::RIGHT;
  add(project::DocumentTextStyle::AlignJustify,"inde-align-justify")->property_justification()=Gtk::Justification::FILL;
}

void WritingWorkspace::install_actions(Gtk::ApplicationWindow &window) {
  const auto add=[this,&window](const char *name,std::function<void()> run,bool require_document=true) {
    window.add_action(name,[this,&window,run=std::move(run),require_document] {
      auto *host=find_modal_overlay(window);
      if(!get_mapped() || ime_active_ || (host && host->has_dialog()) ||
         (require_document && (!current_document_ || page_stack_.get_visible_child_name()!="editor")))return;
      run();
    });
  };
  add("writing-suggest",[this]{suggest_editorial_change();});
  add("writing-export",[this]{export_document();});
  add("writing-inline",[this]{show_inline_commands();});
  add("writing-find",[this]{open_find(false);});
  add("writing-replace",[this]{open_find(true);});
  add("writing-tools",[this]{toggle_tools();});
  add("create-document",[this]{create_document();},false);
  add("save-document",[this]{save_document();});
  add("writing-undo",[this]{undo();});
  add("writing-redo",[this]{redo();});
  add("writing-bold",[this]{apply_text_style(project::DocumentTextStyle::Bold);});
  add("writing-italic",[this]{apply_text_style(project::DocumentTextStyle::Italic);});
  add("writing-underline",[this]{apply_text_style(project::DocumentTextStyle::Underline);});
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
  composition_page_.reset();
  position_save_.disconnect();
  position_restore_.disconnect();
  close_inline_suggestions();
  ime_commit_delivery_.disconnect();
  ime_active_ = ime_commit_pending_ = false;
  gtk_text_view_reset_im_context(editor_.gobj());
  end_recovery();
  save_delivery_.disconnect();
  save_worker_.clear();
  saved_document_.reset();
  ++edit_epoch_; edit_revision_ = 0; save_submitted_ = false; save_error_.clear();
  if (editor_frame_tick_) {
    editor_.remove_tick_callback(editor_frame_tick_);
    editor_frame_tick_ = 0;
  }
  invalidate_text_analysis();
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
  edit_session_.reset();
  current_document_.reset();
  semantic_explorer_document_id_.clear();
  // Visual tags are project-local caches, not a growing registry across a
  // writer's long-lived sequence of projects. Author formatting is untouched.
  for (const auto &[key,tag] : semantic_tags_) {
    (void)key;
    editor_.get_buffer()->get_tag_table()->remove(tag);
  }
  semantic_tags_.clear();
  semantic_filter_ = {};
  semantic_project_scope_ = false;
  semantic_use_filter_ = {};
  semantic_offset_ = 0;
  while (auto *child = semantic_page_.get_first_child())
    semantic_page_.remove(*child);
  documents_.clear();
  placement_options_.clear();
  placement_labels_.clear();
  document_groups_.clear();
  current_group_id_.reset();
  entity_references_.clear();
  close_find();
  find_query_.set_text("");
  find_replacement_.set_text("");
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

WritingWorkspace::~WritingWorkspace() {
  close_inline_suggestions();inline_popover_.unparent();
  // GtkTextView only knows how to remove anchored text children. Detach our
  // custom popover before its dispose loop; controller disposal is too late.
  if (editor_context_popover_ && editor_context_popover_->get_parent())
    editor_context_popover_->unparent();
  editor_context_popover_ = nullptr;
  // GTK can emit mark/clipboard notifications while the containing Window
  // unrealizes after our C++ toolbar members have already been destroyed.
  // Invalidate callbacks before dismantling any member; never dereference this
  // from a retained TextBuffer after this lifetime has ended.
  clipboard_lifetime_.reset();
  ime_commit_delivery_.disconnect();
  recovery_maintenance_delivery_.disconnect();
  end_recovery();
  invalidate_clipboard();
  save_delivery_.disconnect();
  find_debounce_.disconnect();
  find_delivery_.disconnect();
  if (editor_frame_tick_)
    editor_.remove_tick_callback(editor_frame_tick_);
  autosave_connection_.disconnect();
  undo_group_connection_.disconnect();
  invalidate_text_analysis();
}

} // namespace inde::ui
