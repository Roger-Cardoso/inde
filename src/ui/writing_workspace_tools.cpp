#include "inde/ui/writing_workspace.hpp"

#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include "inde/project/manifest.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "writing_workspace_util.hpp"
#include "editor_diagnostics.hpp"

namespace inde::ui {

namespace {
// Content actions replace a modal without going through its response footer.
// Release the old form after the clicked callback has finished using captures.
} // namespace

void WritingWorkspace::toggle_tools() {
  tools_visible_ = !tools_visible_;
  set_overlay_revealer_open(tools_revealer_, tools_visible_);
  tools_button_.set_label(tools_visible_ ? "Fechar ferramentas"
                                         : "Ferramentas");
  if (tools_visible_)
    refresh_text_tools();
}

void WritingWorkspace::refresh_text_tools() {
  EditorPhaseTimer measurement("metrics_snapshot_submit");
  if (!current_document_)
    return;
  text_analysis_.submit({current_document_->id,
                         editor_.get_buffer()->get_text().raw(), {}});
  word_count_.set_text("Atualizando contagem…");
  long_paragraphs_list_.set_sensitive(false);
  analysis_delivery_connection_.disconnect();
  analysis_delivery_connection_ = Glib::signal_timeout().connect([this] {
    if (auto result = text_analysis_.take_latest()) {
      editor_diagnostic("metrics_worker", result->elapsed);
      if (current_document_ && result->document_id == current_document_->id) {
        if (result->statistics)
          apply_text_statistics(std::move(*result->statistics));
        else {
          word_count_.set_text("Contagem indisponível");
          signal_status_message_.emit("Falha na análise do texto; tente abrir "
                                      "as ferramentas novamente.");
        }
      }
      return false;
    }
    return text_analysis_.busy();
  }, 30);
}

void WritingWorkspace::invalidate_text_analysis() {
  invalidate_clipboard();
  invalidate_find();
  analysis_connection_.disconnect();
  analysis_delivery_connection_.disconnect();
  text_analysis_.invalidate();
  if (current_document_)
    word_count_.set_text("Atualizando contagem…");
  // Offsets from an older snapshot must not remain clickable after an edit.
  long_paragraphs_list_.set_sensitive(false);
}

void WritingWorkspace::apply_text_statistics(
    project::DocumentTextStatistics statistics) {
  EditorPhaseTimer measurement("metrics_apply_gtk");
  current_statistics_ = std::move(statistics);
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
  long_paragraphs_list_.set_sensitive(true);
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

} // namespace inde::ui
