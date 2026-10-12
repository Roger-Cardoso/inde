#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/project/document_state.hpp"

#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include "inde/project/manifest.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "editor_diagnostics.hpp"
#include "writing_workspace_util.hpp"

namespace inde::ui {

void WritingWorkspace::set_focus_mode(bool focused) {
  focus_mode_ = focused;
  format_popover_.popdown();
  context_popover_.popdown();
  document_popover_.popdown();
  format_menu_button_.set_visible(!focused);
  context_menu_button_.set_visible(!focused);
  document_menu_button_.set_visible(!focused);
  if (focused && tools_visible_) toggle_tools();
  if (current_document_ && page_stack_.get_visible_child_name()=="editor") editor_.grab_focus();
}

void WritingWorkspace::review_document(const std::string &id) {
  open_document(id);
  if (current_document_ && current_document_->id == id) show_semantic_layers();
}

void WritingWorkspace::show_library() {
  try {
    close_inline_suggestions();
    flush_changes();
    invalidate_text_analysis();
    current_document_.reset();
    edit_session_.reset();
    end_recovery();
    page_stack_.set_visible_child("library");
    refresh_library();
    library_filters_button_.grab_focus();
    signal_status_message_.emit("Biblioteca de Documentos ativa");
  } catch (const std::exception &error) {
    show_error("Não foi possível salvar o Documento", error);
  }
}

void WritingWorkspace::open_document(const std::string &id) {
  EditorPhaseTimer measurement("open_document_gtk");
  try {
    close_inline_suggestions();
    flush_changes();
    const auto value = service_.writing().document(id);
    if (!value)
      throw std::runtime_error("Documento não encontrado");
    current_document_ = value;
    save_worker_.clear();
    saved_document_ = value;
    ++edit_epoch_;
    edit_revision_ = submitted_revision_ = 0;
    save_submitted_ = false;
    save_error_.clear();
    restore_editor_state(*value);
    restore_document_position();
    edit_session_.open(capture_edit_state());
    update_editor_status();
    update_undo_actions();
    page_stack_.set_visible_child("editor");
    begin_recovery();
    refresh_recovery_notice();
    editor_.grab_focus();
    signal_status_message_.emit("Documento aberto para Escrita");
  } catch (const std::exception &error) {
    loading_editor_ = false;
    show_error("Não foi possível abrir o Documento", error);
  }
}

void WritingWorkspace::reveal_document(const std::string &id) {
  if(current_document_ && current_document_->id==id) {
    try {
      flush_changes();
      const auto persisted=service_.writing().document(id);
      if(persisted && project::same_document_state(capture_editor_state(),*persisted,false)) {
        page_stack_.set_visible_child("editor");editor_.grab_focus();return;
      }
    }catch(const std::exception &e){show_error("Não foi possível voltar ao texto",e);return;}
  }
  open_document(id);
}

void WritingWorkspace::show_library_for_editorial_node(
    const std::string &node_id) {
  try {
    flush_changes();
    invalidate_text_analysis();
    current_document_.reset();
    edit_session_.reset();
    end_recovery();
    current_group_id_.reset();
    refresh_library_filter_options();
    search_.set_text("");
    if (entity_filter_)
      entity_filter_->clear_selection();
    if (!placement_filter_.set_active_id(node_id))
      throw std::runtime_error("Elemento do manuscrito não encontrada");
    page_stack_.set_visible_child("library");
    refresh_library();
    signal_status_message_.emit("Biblioteca filtrada pelo elemento do manuscrito");
  } catch (const std::exception &error) {
    show_error("Não foi possível abrir a Biblioteca", error);
  }
}

void WritingWorkspace::show_library_for_entity(const std::string &entity_id) {
  try {
    flush_changes();
    invalidate_text_analysis();
    current_document_.reset();
    edit_session_.reset();
    end_recovery();
    current_group_id_.reset();
    search_.set_text("");
    placement_filter_.set_active_id("all");
    if (!entity_filter_)
      throw std::runtime_error("Filtro de elemento da história indisponível");
    entity_filter_->set_selected(entity_id, entity_label(entity_id));
    page_stack_.set_visible_child("library");
    refresh_library();
    signal_status_message_.emit("Biblioteca filtrada pelo elemento da história");
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
          ? "O Documento será criado no elemento do manuscrito selecionado."
          : "O Documento nasce livre e pode receber posição no manuscrito no editor.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar e escrever", Gtk::ResponseType::ACCEPT);
  dialog->allow_primary_shortcut();
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
  try {
    // Persist pending typing before an organizational transaction is recorded.
    flush_changes();
  } catch (const std::exception &error) {
    show_error("Não foi possível salvar antes de organizar", error);
    return;
  }
  auto original = capture_editor_state();
  auto *dialog = new OverlayDialog("Detalhes do Documento", *owner, true);
  dialog->set_secondary_text(
      "Finalidade, grupo, perspectiva e origem de revisão são metadados; o texto e a posição no manuscrito permanecem independentes.");
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
  group->append("none", "Sem grupo — nível principal da Biblioteca");
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
        auto after = capture_edit_state();
        // Retain current text/selection while changing only the accepted
        // fields.
        after.document.purpose = original.purpose;
        after.document.group_id = original.group_id;
        after.document.perspective = original.perspective;
        after.document.revision_of_id = original.revision_of_id;
        after.document.revision_label = original.revision_label;
        apply_edit_transaction(std::move(after), "Organização documental");
        flush_changes();
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
  composition_page_.flush();
  require_completed_composition();
  autosave_connection_.disconnect();
  if (!current_document_)
    return;
  EditorPhaseTimer measurement("save_barrier_gtk");
  queue_recovery(true);
  recovery_worker_.drain();
  receive_recovery_result();
  receive_save_result();
  queue_autosave();
  save_worker_.drain();
  receive_save_result();
  if (!save_error_.empty())
    throw std::runtime_error(save_error_);
  if (edit_session_.dirty())
    throw std::runtime_error("O Documento ainda possui alterações não salvas");
  remember_document_position();
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
  if(!current_document_ || !owner_window())return;
  try {
    flush_changes();
    confirm_project_removal(*owner_window(),service_,project::RemovalKind::Document,current_document_->id,[this]{
      reset();refresh();signal_status_message_.emit("Documento removido; restauração disponível no histórico do Projeto");
    });
  }catch(const std::exception &e){show_error("Não foi possível preparar a remoção",e);}
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
  for (const auto &v : value_tags_)
    buffer->remove_tag(v.tag, begin, end);
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
  if (refreshing_format_controls_)
    return;
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
    button.set_tooltip_text(Glib::ustring(description) +
                            (active  ? " — ativo neste trecho"
                             : mixed ? " — parcialmente ativo na seleção"
                                     : ""));
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
  refreshing_format_controls_ = true;
  auto cursor = editor_.get_buffer()->get_iter_at_mark(
      editor_.get_buffer()->get_insert());
  std::string family = "Serif", paragraph = "body";
  int size = 10;
  for (const auto &[style, tag] : formatting_tags_) {
    if (cursor.has_tag(tag) && project::paragraph_style(style) &&
        style <= project::DocumentTextStyle::Heading6)
      paragraph = project::to_string(style);
  }
  for (const auto &v : value_tags_)
    if (cursor.has_tag(v.tag)) {
      if (v.style == project::DocumentTextStyle::FontFamily)
        family = v.value;
      else if (v.style == project::DocumentTextStyle::FontSize)
        size = std::stoi(v.value);
    }
  // Never reset to a default and then back to the actual value while inside a
  // ComboBox::changed dispatch: GTK can requeue that signal indefinitely.
  if (font_family_.get_active_id().raw() != family) {
    if (!font_family_.set_active_id(family)) {
      font_family_.append(family, family);
      font_family_.set_active_id(family);
    }
  }
  if (font_size_.get_text().raw() != std::to_string(size))
    font_size_.set_text(std::to_string(size));
  if (paragraph_style_.get_active_id().raw() != paragraph)
    paragraph_style_.set_active_id(paragraph);
  refreshing_format_controls_ = false;
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
  for (const auto &v : value_tags_) {
    auto i = buffer->begin();
    bool active = i.has_tag(v.tag);
    std::size_t start = active ? 0 : total;
    while (true) {
      auto next = i;
      if (!next.forward_to_tag_toggle(v.tag))
        break;
      auto offset = static_cast<std::size_t>(next.get_offset());
      if (active && start < offset)
        result.push_back({v.style, start, offset, v.value});
      else if (!active)
        start = offset;
      active = !active;
      i = next;
    }
    if (active && start < total)
      result.push_back({v.style, start, total, v.value});
  }
  std::sort(result.begin(), result.end(),
            [](const auto &left, const auto &right) {
              if (left.start_offset != right.start_offset)
                return left.start_offset < right.start_offset;
              if (left.end_offset != right.end_offset)
                return left.end_offset < right.end_offset;
              return std::pair(project::to_string(left.style), left.value) <
                     std::pair(project::to_string(right.style), right.value);
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
        span.value.empty() ? tag_for_style(span.style)
                           : value_tag(span.style, span.value),
        buffer->get_iter_at_offset(static_cast<int>(span.start_offset)),
        buffer->get_iter_at_offset(static_cast<int>(span.end_offset)));
  }
  refresh_formatting_toolbar();
}

} // namespace inde::ui
