#include "comic_ui.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
namespace inde::ui {
comic::Book ComicWorkspace::lightweight(const comic::Book &b) {
  comic::Book s;
  s.id = b.id;
  s.work_id = b.work_id;
  s.title = b.title;
  s.slug = b.slug;
  s.lang = b.lang;
  s.layout = b.layout;
  s.revision = b.revision;
  s.removed = b.removed;
  s.panels = b.panels;
  for (const auto &[id, a] : b.assets)
    s.assets[id] = {a.id, a.name, a.mime, "", a.role};
  return s;
}
std::string ComicWorkspace::signature() const {
  auto s = lightweight(book_);
  s.revision = 0;
  return persistence::json::serialize(comic::encode(s), false);
}
void ComicWorkspace::reset_history() {
  undo_.clear();
  redo_.clear();
  asset_data_.clear();
  for (const auto &[id, a] : book_.assets)
    asset_data_[id] = a.data;
  edit_group_.clear();
  saved_signature_ = signature();
  update_history_buttons();
}
void ComicWorkspace::update_history_buttons() {
  undo_button_.set_sensitive(!undo_.empty() && !job_ && !file_);
  redo_button_.set_sensitive(!redo_.empty() && !job_ && !file_);
}
void ComicWorkspace::checkpoint(const std::string &group) {
  if (binding_ || restoring_ || book_.id.empty())
    return;
  const auto now = std::chrono::steady_clock::now();
  const auto key = group + ":" + std::to_string(panel_index_) + ":" +
                   std::to_string(element_index_);
  if (!group.empty() && key == edit_group_ &&
      now - last_edit_ < std::chrono::milliseconds(700)) {
    last_edit_ = now;
    return;
  }
  undo_.push_back(lightweight(book_));
  if (undo_.size() > 40)
    undo_.erase(undo_.begin());
  redo_.clear();
  edit_group_ = group.empty() ? "" : key;
  last_edit_ = now;
  update_history_buttons();
}
void ComicWorkspace::undo() {
  guard([&] {
    if (undo_.empty() || job_ || file_ || composing_)
      return;
    restoring_ = true;
    redo_.push_back(lightweight(book_));
    const auto revision = book_.revision;
    book_ = std::move(undo_.back());
    undo_.pop_back();
    book_.revision = revision;
    for (auto &[id, a] : book_.assets)
      a.data = asset_data_.at(id);
    panel_index_ =
        std::clamp(panel_index_, 0, std::max(0, int(book_.panels.size()) - 1));
    if (panel())
      element_index_ = std::clamp(
          element_index_, 0, std::max(0, int(panel()->elements.size()) - 1));
    edit_group_.clear();
    fill();
    restoring_ = false;
    changed();
  });
}
void ComicWorkspace::redo() {
  guard([&] {
    if (redo_.empty() || job_ || file_ || composing_)
      return;
    restoring_ = true;
    undo_.push_back(lightweight(book_));
    const auto revision = book_.revision;
    book_ = std::move(redo_.back());
    redo_.pop_back();
    book_.revision = revision;
    for (auto &[id, a] : book_.assets)
      a.data = asset_data_.at(id);
    panel_index_ =
        std::clamp(panel_index_, 0, std::max(0, int(book_.panels.size()) - 1));
    if (panel())
      element_index_ = std::clamp(
          element_index_, 0, std::max(0, int(panel()->elements.size()) - 1));
    edit_group_.clear();
    fill();
    restoring_ = false;
    changed();
  });
}
bool ComicWorkspace::can_switch() {
  if (composing_) {
    status_.set_text(
        "Confirme ou cancele a entrada de texto antes de trocar de espaço.");
    return false;
  }
  if (file_) {
    status_.set_text("Conclua a seleção de arquivo antes de trocar de espaço.");
    return false;
  }
  return true;
}
void ComicWorkspace::save_changes() {
  if (job_ || file_ || composing_)
    throw std::runtime_error(
        "Conclua a operação em Comic antes de salvar o Projeto");
  if (dirty_)
    save();
}
void ComicWorkspace::reset() {
  if (job_ || file_ || composing_)
    throw std::runtime_error("Operação da comic em andamento");
  book_ = {};
  project_id_.clear();
  dirty_ = false;
  image_.reset();
  panel_index_ = element_index_ = 0;
  prepared_.reset();
  reset_history();
  binding_ = true;
  books_.remove_all();
  works_.remove_all();
  sites_.remove_all();
  documents_.remove_all();
  binding_ = false;
  fill();
}
void ComicWorkspace::sync_sources() {
  binding_ = true;
  const auto selected_site = sites_.get_active_id(),
             selected_document = documents_.get_active_id(),
             selected_work = works_.get_active_id();
  books_.remove_all();
  for (const auto &b : service_.books(removed_.get_active()))
    books_.append(b.id, b.title + (b.removed ? " · retirado" : ""));
  books_.set_active_id(book_.id);
  works_.remove_all();
  for (const auto &w : publication_.source().catalog().works)
    works_.append(w.id, w.title);
  if (!book_.work_id.empty())
    works_.set_active_id(book_.work_id);
  else if (!selected_work.empty())
    works_.set_active_id(selected_work);
  else
    works_.set_active(0);
  sites_.remove_all();
  for (const auto &s : publication_.sites())
    sites_.append(s.id, s.name);
  if (selected_site.empty())
    sites_.set_active(0);
  else
    sites_.set_active_id(selected_site);
  documents_.remove_all();
  for (const auto &d : publication_.source().writing().documents())
    documents_.append(d.id, d.title);
  if (selected_document.empty() || !documents_.set_active_id(selected_document))
    documents_.set_active(0);
  binding_ = false;
}
void ComicWorkspace::activate() {
  guard([&] {
    const auto *p = publication_.source().current();
    if (!p)
      throw std::runtime_error("Abra um Projeto");
    if (project_id_ != p->manifest().project_id) {
      reset();
      refresh();
      return;
    }
    sync_sources();
    if (!book_.id.empty() && book_.revision > 0) {
      const auto latest = service_.book(book_.id);
      if (latest.revision != book_.revision) {
        if (dirty_)
          status_.set_text("A comic mudou em outra sessão. Seu rascunho foi "
                           "preservado; salvar detectará o conflito.");
        else {
          book_ = latest;
          reset_history();
          fill();
        }
      }
    }
  });
}
void ComicWorkspace::update_zoom() {
  const auto id = zoom_.get_active_id().raw();
  zoom_factor_ = id.empty() || id == "fit" ? 0 : std::stod(id) / 100.;
  const double ratio = panel() ? panel()->ratio : 1.6;
  canvas_.set_content_width(zoom_factor_ > 0 ? int(900 * zoom_factor_ + 32)
                                             : 320);
  canvas_.set_content_height(
      zoom_factor_ > 0 ? int(900 * zoom_factor_ / ratio + 32) : 240);
  canvas_.queue_draw();
}
ViewState ComicWorkspace::capture_view() const {
  const auto *p = panel_index_ >= 0 && panel_index_ < int(book_.panels.size())
                      ? &book_.panels[panel_index_]
                      : nullptr;
  const auto *e =
      p && element_index_ >= 0 && element_index_ < int(p->elements.size())
          ? &p->elements[element_index_]
          : nullptr;
  return ViewState::Object{
      {"version", 1},
      {"comic", book_.id},
      {"panel", p ? p->id : ""},
      {"element", e ? e->id : ""},
      {"zoom", zoom_.get_active_id().raw()},
      {"horizontal",
       scroll_state(canvas_scroll_.get_hadjustment()->get_value())},
      {"vertical", scroll_state(canvas_scroll_.get_vadjustment()->get_value())},
      {"inspector", inspector_.get_current_page()},
      {"inspector_visible", inspector_.get_visible()}};
}
void ComicWorkspace::restore_view(const ViewState &v) {
  if (v.at("version").as_integer() != 1)
    throw std::runtime_error("Contexto de Comic incompatível");
  const auto id = v.at("comic").as_string();
  if (id.empty())
    return;
  if (id != book_.id) {
    if (dirty_)
      throw std::runtime_error("Rascunho preservado. Salve ou descarte antes "
                               "de retomar outra comic");
    book_ = service_.book(id);
    reset_history();
  }
  panel_index_ = element_index_ = 0;
  for (int i = 0; i < int(book_.panels.size()); ++i)
    if (book_.panels[i].id == v.at("panel").as_string())
      panel_index_ = i;
  if (panel())
    for (int i = 0; i < int(panel()->elements.size()); ++i)
      if (panel()->elements[i].id == v.at("element").as_string())
        element_index_ = i;
  fill();
  const auto zoom = v.at("zoom").as_string();
  if (!zoom_.set_active_id(zoom))
    zoom_.set_active_id("fit");
  const auto tab = v.at("inspector").as_integer();
  if (tab < 0 || tab > 3)
    throw std::runtime_error("Painel salvo inválido");
  inspector_.set_current_page(int(tab));
  if (v.as_object().contains("inspector_visible"))
    inspector_.set_visible(v.at("inspector_visible").as_boolean());
  const auto x = view_scroll(v.at("horizontal")),
             y = view_scroll(v.at("vertical"));
  Glib::signal_idle().connect_once(sigc::track_object(
      [this, x, y] {
        canvas_scroll_.get_hadjustment()->set_value(x);
        canvas_scroll_.get_vadjustment()->set_value(y);
      },
      *this));
}
} // namespace inde::ui
namespace inde::ui {
void ComicWorkspace::toggle_inspector() {
  inspector_.set_visible(!inspector_.get_visible());
}
std::vector<Gtk::Widget *> ComicWorkspace::keyboard_regions() {
  std::vector<Gtk::Widget *> out{&library_, &canvas_};
  if (inspector_.get_visible())
    out.push_back(&inspector_);
  return out;
}
void ComicWorkspace::install_keyboard(KeyboardCommands &keys) {
  auto own = [this](Gtk::Widget *focus) {
    if (!get_mapped())
      return false;
    for (auto *w = focus; w; w = w->get_parent())
      if (w == this)
        return true;
    return false;
  };
  keys.add({"comic.undo",
            "Desfazer composição",
            "Comic",
            "<Control>z",
            {},
            own,
            [this] {
              return undo_.empty()
                         ? std::string(
                               "Não há alteração de composição para desfazer.")
                         : std::string{};
            },
            [this] { undo(); }});
  keys.add({"comic.redo",
            "Refazer composição",
            "Comic",
            "<Control><Shift>z",
            {},
            own,
            [this] {
              return redo_.empty()
                         ? std::string(
                               "Não há alteração de composição para refazer.")
                         : std::string{};
            },
            [this] { redo(); }});
}
} // namespace inde::ui
