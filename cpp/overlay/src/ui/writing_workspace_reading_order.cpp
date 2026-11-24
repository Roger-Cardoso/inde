#include "inde/ui/writing_workspace.hpp"
#include "object_native_ui.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include <algorithm>
namespace inde::ui {
void WritingWorkspace::build_reading_order_ui() {
  for (int direction : {-1, 1}) {
    auto *button = Gtk::make_managed<Gtk::Button>(direction < 0
        ? "Mover bloco acima na leitura" : "Mover bloco abaixo na leitura");
    button->set_tooltip_text("Move o bloco do cursor com suas folhas e marcações; "
                            "não muda cronologia nem ordens do planejamento.");
    button->signal_clicked().connect([this, direction] {
      document_popover_.popdown(); move_reading_current(direction);
    });
    document_menu_.append(*button);
  }
  reading_gutter_.move = [this](const std::string &id, const std::string &reference, bool after) {
    move_reading_relative(id, reference, after);
  };
  const auto adjustment = editor_scroll_.get_vadjustment();
  adjustment->signal_value_changed().connect(sigc::track_obj([this] { reading_gutter_.queue_draw(); }, *this));
  adjustment->signal_changed().connect(sigc::track_obj([this] { reading_gutter_.queue_draw(); }, *this));
  reading_gutter_.set_visible(false);
}
void WritingWorkspace::refresh_reading_blocks() {
  if (loading_editor_ || restoring_undo_) return;
  auto blocks=visible_reading_blocks();
  const bool visible=editor_specialization_=="structure" && !blocks.empty();
  reading_gutter_.set_blocks(std::move(blocks));
  reading_gutter_.set_visible(visible);
}
void WritingWorkspace::move_reading_current(int direction) {
  if(object_editor_ && document_surfaces_.get_visible_child_name()=="objects") {object_editor_->move_reading(direction);return;}
  if (!current_document_) return;
  const auto rows = visible_reading_blocks();
  if (rows.empty()) {
    signal_status_message_.emit("Identifique os parágrafos deste Documento antes de mover blocos.");
    show_document_paragraphs(); return;
  }
  const auto point = static_cast<std::size_t>(editor_.get_buffer()->get_iter_at_mark(
      editor_.get_buffer()->get_insert()).get_offset());
  auto it = std::upper_bound(rows.begin(), rows.end(), point,
      [](auto p, const auto &b) { return p < b.start; });
  if (it != rows.begin()) --it;
  const auto i = static_cast<std::size_t>(it - rows.begin());
  if ((direction < 0 && i == 0) || (direction > 0 && i + 1 == rows.size())) {
    signal_status_message_.emit("O bloco já está no limite da leitura."); return;
  }
  move_reading_relative(it->id, rows[direction < 0 ? i - 1 : i + 1].id, direction > 0);
}
void WritingWorkspace::move_reading_relative(std::string id, std::string reference, bool after) {
  if (!current_document_ || page_stack_.get_visible_child_name() != "editor") return;
  try {
    require_completed_composition();
    if (auto *owner = owner_window())
      if (auto *host = find_modal_overlay(*owner); host && host->has_dialog()) return;
    flush_changes();
    apply_edit_transaction(application::move_reading_relative(capture_edit_state(), id, reference, after),
                           "Mover bloco em relação ao vizinho");
    auto at = editor_.get_buffer()->get_iter_at_mark(editor_.get_buffer()->get_insert());
    editor_.scroll_to(at, .15);
    signal_status_message_.emit("Ordem alterada pela identidade do vizinho; os blocos ocultos mantêm sua ordem relativa.");
  } catch (const std::exception &e) { show_error("Não foi possível mover o bloco", e); }
}
void WritingWorkspace::move_reading_to(std::string id, std::string destination) {
  if (!current_document_ || page_stack_.get_visible_child_name() != "editor") return;
  try {
    require_completed_composition();
    if (auto *owner = owner_window())
      if (auto *host = find_modal_overlay(*owner); host && host->has_dialog()) return;
    flush_changes();
    auto after = application::move_reading_block(capture_edit_state(), id, destination);
    if (apply_edit_transaction(std::move(after), "Mover bloco na ordem de leitura")) {
      auto at = editor_.get_buffer()->get_iter_at_mark(editor_.get_buffer()->get_insert());
      editor_.scroll_to(at, .15);
      signal_status_message_.emit("Bloco movido na leitura. Folhas e vínculos conservados; "
        "cronologia e planejamento inalterados. Desfazer restaura a ordem anterior.");
    }
  } catch (const std::exception &e) { show_error("Não foi possível mover o bloco", e); }
}
} // namespace inde::ui
