#include "inde/ui/author_diagnostics.hpp"
#include "editor_color_picker.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "writing_workspace_util.hpp"
#include <algorithm>
#include <charconv>
#include <set>
#include <stdexcept>
namespace inde::ui {
using S = project::DocumentTextStyle;
void WritingWorkspace::build_editor_controls() {
  stats_title_.set_text("Estatísticas");
  goal_title_.set_text("Meta");
  repeated_title_.set_text("Palavras repetidas");
  paragraphs_title_.set_text("Parágrafos Longos");
  anchors_title_.set_text("Trechos marcados");
  references_title_.set_text("Elementos da história");
  clear_format_button_.set_tooltip_text(
      "Limpar formatação da seleção, preservando texto, comentários e "
      "marcações semânticas");
  organization_button_.set_tooltip_text("Detalhes e organização do Documento");
  font_family_.append("Sans", "Sans");
  font_family_.append("Serif", "Serif");
  font_family_.append("Monospace", "Monospace");
  std::set<std::string> names;
  PangoFontFamily **families{};
  int count{};
  pango_context_list_families(editor_.get_pango_context()->gobj(), &families,
                              &count);
  for (int i = 0; i < count; ++i) {
    std::string name = pango_font_family_get_name(families[i]);
    if (name.size() <= 128)
      names.insert(name);
  }
  g_free(families);
  for (const auto &name : names)
    if (name != "Sans" && name != "Serif" && name != "Monospace")
      font_family_.append(name, name);
  font_family_.set_active_id("Serif");
  for (auto *cell : font_family_.get_cells())
    if (auto *text = dynamic_cast<Gtk::CellRendererText *>(cell)) {
      text->property_ellipsize() = Pango::EllipsizeMode::END;
      text->property_width_chars() = 14;
      text->property_max_width_chars() = 14;
    }
  font_family_.set_tooltip_text(
      "Fonte da seleção; sem seleção, do parágrafo atual");
  set_accessible_label(font_family_, "Fonte");
  paragraph_style_.append("heading", "Título 1");
  for (int i = 2; i <= 6; ++i)
    paragraph_style_.append("heading" + std::to_string(i),
                            "Título " + std::to_string(i));
  paragraph_style_.append("subheading", "Subtítulo");
  paragraph_style_.append("quote", "Citação");
  paragraph_style_.append("body", "Corpo");
  paragraph_style_.set_active_id("body");
  set_accessible_label(paragraph_style_, "Estilo");
  paragraph_style_.set_tooltip_text(
      "Estilo dos parágrafos selecionados ou do parágrafo atual");
  font_size_.set_text("10");
  font_size_.set_max_length(2);
  font_size_.set_input_purpose(Gtk::InputPurpose::DIGITS);
  font_size_.set_placeholder_text("6–96");
  font_size_.set_width_chars(2);
  font_size_.set_max_width_chars(2);
  font_size_.set_tooltip_text("Tamanho em pontos, de 6 a 96");
  set_accessible_label(font_size_, "Tamanho da fonte");
  formatting_toolbar_.append(font_family_);
  formatting_toolbar_.append(paragraph_style_);
  formatting_toolbar_.append(size_down_);
  formatting_toolbar_.append(font_size_);
  formatting_toolbar_.append(size_up_);
  for (auto *button :
       {&align_left_, &align_center_, &align_right_, &align_justify_,
        &color_button_, &bold_button_, &italic_button_, &underline_button_,
        &strike_button_, &undo_button_, &redo_button_, &clear_format_button_})
    formatting_toolbar_.append(*button);
  const auto bind = [this](Gtk::Button &button, const char *label,
                           std::function<void()> action) {
    button.set_tooltip_text(label);
    set_accessible_label(button, label);
    button.set_focus_on_click(false);
    button.signal_clicked().connect(action);
  };
  const auto shift_size = [this](int delta) {
    const auto text = font_size_.get_text().raw();
    int value{};
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        value < 6 || value > 96) {
      signal_status_message_.emit(
          "Informe um tamanho inteiro de 6 a 96 pontos.");
      return;
    }
    font_size_.set_text(std::to_string(std::clamp(value + delta, 6, 96)));
  };
  bind(size_down_, "Diminuir tamanho da fonte",
       [shift_size] { shift_size(-1); });
  bind(size_up_, "Aumentar tamanho da fonte", [shift_size] { shift_size(1); });
  bind(align_left_, "Alinhar à esquerda",
       [this] { apply_editor_format(S::AlignLeft); });
  bind(align_center_, "Centralizar",
       [this] { apply_editor_format(S::AlignCenter); });
  bind(align_right_, "Alinhar à direita",
       [this] { apply_editor_format(S::AlignRight); });
  bind(align_justify_, "Justificar",
       [this] { apply_editor_format(S::AlignJustify); });
  bind(color_button_, "Alterar a cor do texto", [this] { show_text_color(); });
  bind(cut_button_, "Recortar seleção",
       [this] { copy_selection(false, true); });
  bind(consult_button_, "Consultar palavra",
       [this] { show_semantic_dictionary(); });
  set_accessible_label(undo_button_, "Desfazer");
  undo_button_.set_tooltip_text("Desfazer (Ctrl+Z)");
  set_accessible_label(redo_button_, "Refazer");
  redo_button_.set_tooltip_text("Refazer (Ctrl+Shift+Z)");
  bind(anchors_button_, "Trechos marcados",
       [this] { show_tool_section("anchors"); });
  bind(entities_button_, "Elementos da história usadas",
       [this] { show_tool_section("entities"); });
  bind(statistics_button_, "Estatísticas do texto",
       [this] { show_tool_section("statistics"); });
  bind(goal_button_, "Meta de palavras", [this] { show_tool_section("goal"); });
  bind(repeated_button_, "Palavras repetidas",
       [this] { show_tool_section("repeated"); });
  bind(long_paragraphs_button_, "Parágrafos longos",
       [this] { show_tool_section("paragraphs"); });
  bind(comment_button_, "Comentar seleção ou consultar comentários nas Camadas",
       [this] {
         auto buffer = editor_.get_buffer();
         Gtk::TextIter a, b;
         if (buffer->get_selection_bounds(a, b))
           edit_semantic_fields({}, "inde.editor.comment");
         else {
           semantic_project_scope_ = false;
           semantic_filter_ = {};
           semantic_filter_.role = "inde.editor.comment";
           show_semantic_layers();
         }
       });
  bind(inline_button_,
       "Ações no texto — sugestões e modelos do Documento (Ctrl+Shift+K)",
       [this] { show_inline_commands(); });
  const auto defer_format = [this](std::function<void()> action) {
    const std::weak_ptr<int> life = clipboard_lifetime_;
    const auto epoch = edit_epoch_, revision = edit_revision_;
    auto b = editor_.get_buffer();
    const auto cursor = b->get_iter_at_mark(b->get_insert()).get_offset(),
               bound =
                   b->get_iter_at_mark(b->get_selection_bound()).get_offset();
    // GTK input controls can own PRIMARY while emitting changed. Replacing
    // TextView selection in that dispatch can block X11 server-time requests.
    Glib::signal_idle().connect_once(
        [this, life, epoch, revision, cursor, bound, action] {
          if (life.expired())
            return;
          auto buffer = editor_.get_buffer();
          if (epoch != edit_epoch_ || revision != edit_revision_ ||
              cursor !=
                  buffer->get_iter_at_mark(buffer->get_insert()).get_offset() ||
              bound != buffer->get_iter_at_mark(buffer->get_selection_bound())
                           .get_offset()) {
            signal_status_message_.emit(
                "O contexto mudou antes da formatação; selecione novamente.");
            return;
          }
          action();
        });
  };
  font_family_.signal_changed().connect([this, defer_format] {
    if (!refreshing_format_controls_ && !loading_editor_) {
      const auto family = font_family_.get_active_id().raw();
      defer_format(
          [this, family] { apply_editor_format(S::FontFamily, family); });
    }
  });
  font_size_.signal_changed().connect([this, defer_format] {
    if (!refreshing_format_controls_ && !loading_editor_) {
      const auto size = font_size_.get_text().raw();
      int value{};
      const auto parsed =
          std::from_chars(size.data(), size.data() + size.size(), value);
      if (parsed.ec == std::errc{} && parsed.ptr == size.data() + size.size() &&
          value >= 6 && value <= 96)
        defer_format([this, size] { apply_editor_format(S::FontSize, size); });
    }
  });
  font_size_.signal_activate().connect([this] {
    try {
      project::validate_format_span(
          {S::FontSize, 0, 1, font_size_.get_text().raw()});
    } catch (const std::exception &) {
      signal_status_message_.emit(
          "Informe um tamanho inteiro de 6 a 96 pontos.");
      refresh_formatting_toolbar();
    }
  });
  paragraph_style_.signal_changed().connect([this, defer_format] {
    if (refreshing_format_controls_ || loading_editor_)
      return;
    const auto id = paragraph_style_.get_active_id().raw();
    if (id == "body")
      defer_format([this] { apply_paragraph_body(); });
    else if (!id.empty())
      defer_format([this, id] {
        apply_editor_format(project::document_text_style_from_string(id));
      });
  });
  inline_popover_.set_parent(editor_);
  inline_popover_.set_child(inline_suggestions_);
  inline_popover_.set_autohide(false);
  inline_popover_.set_has_arrow(false);
  auto keys = Gtk::EventControllerKey::create();
  const std::weak_ptr<int> lifetime = clipboard_lifetime_;
  keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  keys->signal_key_pressed().connect(
      [this, lifetime](guint key, guint, Gdk::ModifierType modifiers) {
        if (lifetime.expired())
          return false;
        if (ime_active_)
          return false;
        // Ctrl+Shift+K is routed by the central command registry. Ctrl+Alt
        // belongs to layout/IME input, not an editor command.
        if (!inline_popover_.get_mapped())
          return false;
        if (key == GDK_KEY_Escape && !(static_cast<unsigned>(modifiers) & (GDK_CONTROL_MASK|GDK_SHIFT_MASK|GDK_ALT_MASK|GDK_SUPER_MASK))) {
          close_inline_suggestions();
          return true;
        }
        if (key == GDK_KEY_Down && !(static_cast<unsigned>(modifiers) & (GDK_CONTROL_MASK|GDK_SHIFT_MASK|GDK_ALT_MASK|GDK_SUPER_MASK))) {
          for (auto *child = inline_suggestions_.get_first_child(); child;
               child = child->get_next_sibling())
            if (auto *button = dynamic_cast<Gtk::Button *>(child)) {
              button->grab_focus();
              return true;
            }
        }
        return false;
      },
      false);
  editor_.add_controller(keys);
}
Glib::RefPtr<Gtk::TextTag>
WritingWorkspace::value_tag(S style, const std::string &value) {
  project::validate_format_span({style, 0, 1, value});
  for (const auto &v : value_tags_)
    if (v.style == style && v.value == value)
      return v.tag;
  auto tag = editor_.get_buffer()->create_tag();
  if (style == S::FontFamily)
    tag->property_family() = value;
  else if (style == S::FontSize)
    tag->property_size_points() = std::stoi(value);
  else if (style == S::Foreground)
    tag->property_foreground_rgba() = Gdk::RGBA(value);
  value_tags_.push_back({style, value, tag});
  return tag;
}
namespace {
std::pair<std::size_t, std::size_t>
format_range(const Glib::RefPtr<Gtk::TextBuffer> &buffer, bool paragraph) {
  Gtk::TextIter a, b;
  const bool selected = buffer->get_selection_bounds(a, b);
  if (!selected) {
    a = buffer->get_iter_at_mark(buffer->get_insert());
    b = a;
    paragraph = true;
  }
  if (paragraph) {
    a.set_line_offset(0);
    if (!selected || !b.starts_line())
      b.forward_to_line_end();
  }
  return {static_cast<std::size_t>(a.get_offset()),
          static_cast<std::size_t>(b.get_offset())};
}
bool heading(S s) {
  return s == S::Heading || s == S::Subheading || s == S::Quote ||
         (s >= S::Heading2 && s <= S::Heading6);
}
bool alignment(S s) { return s >= S::AlignLeft && s <= S::AlignJustify; }
void subtract_formats(project::Document &d, std::size_t start, std::size_t end,
                      std::function<bool(S)> category) {
  std::vector<project::DocumentFormatSpan> spans;
  for (auto s : d.formatting) {
    if (!category(s.style) || s.end_offset <= start || s.start_offset >= end) {
      spans.push_back(s);
      continue;
    }
    if (s.start_offset < start) {
      auto left = s;
      left.end_offset = start;
      spans.push_back(left);
    }
    if (s.end_offset > end) {
      s.start_offset = end;
      spans.push_back(s);
    }
  }
  d.formatting = std::move(spans);
}
} // namespace
void WritingWorkspace::apply_editor_format(S style,
                                           const std::string &value) try {
  if (!current_document_)
    return;
  require_completed_composition();
  const auto [a, b] =
      format_range(editor_.get_buffer(), project::paragraph_style(style));
  if (a == b) {
    signal_status_message_.emit(
        "Escreva no parágrafo ou selecione um trecho para formatar.");
    return;
  }
  project::validate_format_span({style, a, b, value});
  auto state = capture_edit_state();
  subtract_formats(state.document, a, b, [style](S s) {
    return heading(style)     ? heading(s)
           : alignment(style) ? alignment(s)
                              : s == style;
  });
  state.document.formatting.push_back({style, a, b, value});
  apply_edit_transaction(std::move(state),
                         "Formatação: " + project::to_string(style));
} catch (const std::exception &e) {
  show_error("Não foi possível aplicar a formatação", e);
}
void WritingWorkspace::apply_paragraph_body() try {
  if (!current_document_)
    return;
  require_completed_composition();
  auto [a, b] = format_range(editor_.get_buffer(), true);
  auto state = capture_edit_state();
  subtract_formats(state.document, a, b, heading);
  apply_edit_transaction(std::move(state), "Estilo Corpo");
} catch (const std::exception &e) {
  show_error("Não foi possível aplicar Corpo", e);
}
void WritingWorkspace::show_text_color() {
  if (!current_document_ || !owner_window())
    return;
  const auto epoch = edit_epoch_, revision = edit_revision_;
  const std::weak_ptr<int> life = clipboard_lifetime_;
  const auto buffer = editor_.get_buffer();
  const auto cursor =
      buffer->get_iter_at_mark(buffer->get_insert()).get_offset();
  const auto bound =
      buffer->get_iter_at_mark(buffer->get_selection_bound()).get_offset();
  const auto at = buffer->get_iter_at_offset(std::min(cursor, bound));
  const auto [range_start, range_end] = format_range(buffer, false);
  std::string initial = "#edf1f7";
  for (const auto &v : value_tags_)
    if (v.style == S::Foreground && at.has_tag(v.tag))
      initial = v.value;
  auto *dialog =
      new OverlayDialog("Alterar a cor do texto", *owner_window(), true);
  // Own the composite in C++, so its callbacks are disconnected before GTK
  // disposes its managed chooser child (managed composites destruct too late).
  auto picker = std::make_shared<EditorColorPicker>(initial);
  dialog->set_secondary_text(
      "Cor autoral da seleção, independente dos fundos da realce das marcações. Sem seleção, aplica ao parágrafo atual.");
  dialog->get_content_area()->append(*picker);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Aplicar cor", Gtk::ResponseType::ACCEPT);
  dialog->signal_response().connect([this, dialog, picker, epoch, revision,
                                     cursor, bound, range_start, range_end,
                                     life](int response) {
    if (response == Gtk::ResponseType::ACCEPT && !life.expired()) {
      const auto color = picker->color_hex();
      if (!color) {
        dialog->hide();
        signal_status_message_.emit(
            "Cor RGB inválida; nenhum texto foi alterado. Reabra o seletor "
            "para ajustar a cor.");
        return;
      }
      if (epoch == edit_epoch_ && revision == edit_revision_) {
        // Modal controls can acquire X11 PRIMARY and clear the visual
        // selection. Keep the author's captured target instead of formatting a
        // new cursor.
        try {
          require_completed_composition();
          if (range_start == range_end)
            throw std::runtime_error("Selecione texto ou escreva no parágrafo "
                                     "antes de aplicar a cor.");
          auto state = capture_edit_state();
          state.selection = {static_cast<std::size_t>(cursor),
                             static_cast<std::size_t>(bound)};
          project::validate_format_span(
              {S::Foreground, range_start, range_end, *color});
          subtract_formats(state.document, range_start, range_end,
                           [](S s) { return s == S::Foreground; });
          state.document.formatting.push_back(
              {S::Foreground, range_start, range_end, *color});
          apply_edit_transaction(std::move(state), "Alterar cor do texto");
        } catch (const std::exception &e) {
          signal_status_message_.emit(author::diagnostic(e.what()));
        }
      } else
        signal_status_message_.emit(
            "O Documento mudou; selecione novamente antes de aplicar a cor.");
    }
    dialog->hide();
  });
  dialog->present();
}
void WritingWorkspace::show_tool_section(const std::string &section) {
  if (tools_visible_ && active_tool_section_ == section) {
    toggle_tools();
    return;
  }
  for (auto *child = tools_panel_.get_first_child(); child;
       child = child->get_next_sibling())
    child->set_visible(false);
  tools_button_.set_visible(true);
  const auto show = [](std::initializer_list<Gtk::Widget *> items) {
    for (auto *w : items)
      w->set_visible(true);
  };
  if (section == "statistics")
    show({&stats_title_, &stats_grid_});
  if (section == "goal")
    show({&goal_title_, &goal_enabled_, &word_goal_, &goal_progress_});
  if (section == "repeated")
    show({&repeated_title_, &repeated_words_});
  if (section == "paragraphs")
    show({&paragraphs_title_, &long_paragraphs_list_});
  if (section == "anchors")
    show({&anchors_title_, &anchor_actions_, &anchors_list_,
          &anchor_usages_button_, &text_references_button_});
  if (section == "entities")
    show({&references_title_, &reference_actions_, &references_list_});
  active_tool_section_ = section;
  if (!tools_visible_)
    toggle_tools();
  tools_button_.set_label("Fechar painel");
  refresh_text_tools();
}
} // namespace inde::ui
