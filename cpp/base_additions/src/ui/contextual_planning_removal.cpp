#include "contextual_planning_page.hpp"
#include "contextual_planning_helpers.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include <algorithm>

namespace inde::ui {
namespace {
Gtk::Label *message(const std::string &s) {
  auto *label = Gtk::make_managed<Gtk::Label>(s);
  label->set_wrap(true);
  label->set_xalign(0);
  label->set_max_width_chars(64);
  return label;
}
} // namespace
void ContextualPlanningPage::remove_visual_selection() {
  auto *owner = dynamic_cast<Gtk::Window *>(get_root());
  if (!owner || canvas_.selection().empty() || snapshot_.plan_id.empty()) return;
  const auto expected = contextual_ui::observation(snapshot_);
  const std::vector<std::string> ids(canvas_.selection().begin(), canvas_.selection().end());
  const auto parts = canvas_.selected_parts().size();
  auto *d = new OverlayDialog("Excluir elementos selecionados", *owner, true);
  d->set_secondary_text("Excluir " + std::to_string(ids.size()) + " elemento(s) desta composição? "
      "Os trechos serão retirados apenas deste conjunto; o conteúdo compartilhado, "
      "os interiores e os outros usos permanecem. Excluir uma faixa não exclui os "
      "trechos dentro dela. A reversão fica no histórico do Projeto.");
  auto *error = message("");
  error->add_css_class("error");
  d->get_content_area()->append(*error);
  d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  d->add_button("Excluir elementos", Gtk::ResponseType::ACCEPT);
  d->signal_response().connect(sigc::track_obj([this, d, error, expected, ids, parts](int r) {
    if (r != Gtk::ResponseType::ACCEPT) return;
    try {
      if (parts == 0) {
        // Pure geometry retains the existing local undo/redo disposition path.
        auto values = canvas_.items();
        std::erase_if(values, [&](const auto &v) {
          return std::find(ids.begin(), ids.end(), v.id) != ids.end();
        });
        commit_visual(expected, values);
      } else {
        service_.remove_visual_selection(expected, ids);
        visual_undo_.clear();
        visual_redo_.clear();
      }
    } catch (const std::exception &e) {
      error->set_text(e.what());
      d->keep_open_on_response();
      return;
    }
    selected_id_.clear();
    refresh(work_id_);
    status_.emit("Elementos excluídos desta composição. Reversão no histórico do Projeto.");
  }, *this));
  d->present();
}

void ContextualPlanningPage::remove_plan() {
  auto *owner = dynamic_cast<Gtk::Window *>(get_root());
  if (!owner || snapshot_.plan_id.empty()) {
    status_.emit("Escolha uma composição visual para excluir.");
    return;
  }
  const auto expected = contextual_ui::observation(snapshot_);
  auto *d = new OverlayDialog("Excluir composição visual", *owner, true);
  d->set_secondary_text("Excluir “" + snapshot_.title + "”? Serão retirados " +
      std::to_string(snapshot_.participations.size()) + " uso(s) de trechos, a disposição, "
      "faixas, marcos, encaixes, opções, declarações e vínculos locais desta composição. "
      "Os acontecimentos compartilhados e os documentos vinculados permanecem. "
      "Composições usadas como interior ou ligadas a outras precisam ser desvinculadas "
      "antes. Há reversão no histórico do Projeto.");
  auto *error = message("");
  error->add_css_class("error");
  d->get_content_area()->append(*error);
  d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  d->add_button("Excluir composição", Gtk::ResponseType::ACCEPT);
  d->signal_response().connect(sigc::track_obj([this, d, error, expected](int r) {
    if (r != Gtk::ResponseType::ACCEPT) return;
    try {
      service_.remove_plan(expected);
    } catch (const std::exception &e) {
      error->set_text(e.what());
      d->keep_open_on_response();
      return;
    }
    selected_id_.clear();
    context_returns_.clear();
    module_returns_.clear();
    visual_undo_.clear();
    visual_redo_.clear();
    refresh(work_id_);
    status_.emit("Composição visual excluída. Reversão no histórico do Projeto.");
  }, *this));
  d->present();
}
} // namespace inde::ui
