#include "comic_ui.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/accessibility.hpp"
#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <gdk/gdkkeysyms.h>
#include <gdkmm/general.h>
#include <gdkmm/pixbufloader.h>
namespace inde::ui {
namespace {
void label(Gtk::Box &box, const std::string &s) {
  auto *l = Gtk::make_managed<Gtk::Label>(s);
  l->set_xalign(0);
  l->set_wrap(true);
  box.append(*l);
}
void field(Gtk::Box &box, const std::string &s, Gtk::Widget &w) {
  auto *l = Gtk::make_managed<Gtk::Label>(s);
  l->set_xalign(0);
  l->set_wrap(true);
  l->set_mnemonic_widget(w);
  box.append(*l);
  box.append(w);
  set_accessible_label(w, s);
}
std::filesystem::path resources() {
  const auto *p = std::getenv("INDE_PUBLICACAO_RECURSOS");
  if (!p)
    throw std::runtime_error(
        "Abra o INDE pelo lançador para localizar os recursos de publicação");
  return p;
}
GSubprocess *spawn(const std::vector<std::string> &args) {
  std::vector<const char *> raw;
  for (const auto &a : args)
    raw.push_back(a.c_str());
  raw.push_back(nullptr);
  GError *error = nullptr;
  auto *p = g_subprocess_newv(raw.data(), G_SUBPROCESS_FLAGS_NONE, &error);
  if (!p) {
    std::string s =
        error ? error->message : "Não foi possível iniciar publicação";
    g_clear_error(&error);
    throw std::runtime_error(s);
  }
  return p;
}
void color(const Cairo::RefPtr<Cairo::Context> &cr, const std::string &s) {
  Gdk::RGBA c;
  c.set(s);
  cr->set_source_rgba(c.get_red(), c.get_green(), c.get_blue(), c.get_alpha());
}
void rounded(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y,
             double w, double h, double r) {
  r = std::min({r, w / 2, h / 2});
  cr->begin_new_sub_path();
  cr->arc(x + w - r, y + r, r, -M_PI / 2, 0);
  cr->arc(x + w - r, y + h - r, r, 0, M_PI / 2);
  cr->arc(x + r, y + h - r, r, M_PI / 2, M_PI);
  cr->arc(x + r, y + r, r, M_PI, 3 * M_PI / 2);
  cr->close_path();
}
} // namespace
ComicWorkspace::ComicWorkspace(publication::Service &p)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8), publication_(p), service_(p) {
  set_margin(12);
  heading_.add_css_class("title-2");
  heading_.set_xalign(0);
  top_.append(heading_);
  location_.set_hexpand(true);
  location_.set_ellipsize(Pango::EllipsizeMode::END);
  location_.set_xalign(0);
  top_.append(location_);
  top_.append(undo_button_);
  top_.append(redo_button_);
  top_.append(save_);
  top_.append(discard_);
  append(top_);
  body_.append(library_split_);
  library_split_.set_hexpand(true);
  library_split_.set_vexpand(true);
  append(body_);
  body_.set_vexpand(true);
  library_split_.set_start_child(library_);
  library_split_.set_end_child(inspector_split_);
  library_split_.set_position(230);
  library_split_.set_resize_start_child(false);
  library_split_.set_shrink_start_child(false);
  inspector_split_.set_start_child(left_);
  inspector_split_.set_end_child(inspector_);
  inspector_split_.set_resize_end_child(false);
  inspector_split_.set_shrink_end_child(false);
  inspector_split_.set_position(880);
  signal_map().connect([this] {
    Glib::signal_idle().connect_once(sigc::track_object(
        [this] {
          if (!get_mapped() ||
              g_object_get_data(G_OBJECT(inspector_split_.gobj()),
                                "inde-comic-initial-width"))
            return;
          const int width = inspector_split_.get_width();
          if (width > 720) {
            inspector_split_.set_position(width - 360);
            g_object_set_data(G_OBJECT(inspector_split_.gobj()),
                              "inde-comic-initial-width", GINT_TO_POINTER(1));
          }
        },
        *this));
  });
  library_.set_size_request(205, -1);
  label(library_, "COMICS DA OBRA");
  library_.append(books_);
  library_.append(removed_);
  library_actions_.append(create_);
  library_actions_.append(demo_);
  library_actions_.append(retire_);
  library_.append(library_actions_);
  label(library_, "QUADROS");
  panel_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  panel_list_scroll_.set_child(panel_list_);
  panel_list_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                Gtk::PolicyType::AUTOMATIC);
  panel_list_scroll_.set_vexpand(true);
  library_.append(panel_list_scroll_);
  auto *sequence = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
  sequence->append(add_panel_);
  sequence->append(panel_up_);
  sequence->append(panel_down_);
  library_.append(*sequence);
  library_.append(remove_panel_);
  for (const auto &id : {"fit", "50", "75", "100", "125", "150", "200"})
    zoom_.append(id, std::string(id) == "fit" ? "Ajustar quadro"
                                              : std::string(id) + "%");
  zoom_.set_active_id("fit");
  canvas_tools_.append(zoom_);
  canvas_tools_.append(snap_);
  snap_.set_active(true);
  canvas_tools_.append(image_button_);
  left_.append(canvas_tools_);
  set_accessible_label(panel_up_, "Mover quadro para cima");
  set_accessible_label(panel_down_, "Mover quadro para baixo");
  set_accessible_label(canvas_, "Composição do quadro — arraste para mover, "
                                "canto para redimensionar, setas para ajustes");
  canvas_.set_focusable(true);
  canvas_.set_content_width(320);
  canvas_.set_content_height(240);
  canvas_.set_hexpand(true);
  canvas_.set_vexpand(true);
  canvas_.set_draw_func(sigc::mem_fun(*this, &ComicWorkspace::draw));
  canvas_scroll_.set_child(canvas_);
  canvas_scroll_.set_policy(Gtk::PolicyType::AUTOMATIC,
                            Gtk::PolicyType::AUTOMATIC);
  canvas_scroll_.set_hexpand(true);
  canvas_scroll_.set_vexpand(true);
  left_.append(canvas_scroll_);
  left_.append(elements_);
  element_actions_.append(add_element_);
  element_actions_.append(remove_element_);
  left_.append(element_actions_);
  help_.set_text("Arraste para mover e use o canto inferior para "
                 "redimensionar. Setas ajustam a posição; Shift amplia o "
                 "passo. Confira as fontes incorporadas na prévia de leitura.");
  help_.set_wrap(true);
  help_.set_xalign(0);
  left_.append(help_);
  auto page = [&](Gtk::ScrolledWindow &scroll, Gtk::Box &box,
                  const char *name) {
    scroll.set_child(box);
    scroll.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scroll.set_min_content_width(295);
    scroll.set_size_request(305, -1);
    box.set_margin(10);
    inspector_.append_page(scroll, name);
  };
  page(form_scroll_, form_, "Elemento");
  page(panel_scroll_, panel_form_, "Quadro");
  page(book_scroll_, book_form_, "Obra");
  page(publication_scroll_, publication_form_, "Publicação");
  publication_form_.append(generate_);
  publication_form_.append(preview_);
  preview_.set_sensitive(false);
  field(book_form_, "Obra", works_);
  field(book_form_, "Título do comic", title_);
  field(book_form_, "Endereço público", slug_);
  field(book_form_, "Idioma da obra (ex.: pt-BR)", language_);
  layout_.append("strip", "Vertical");
  layout_.append("mosaic", "Mosaico");
  field(book_form_, "Montagem de leitura", layout_);
  field(publication_form_, "Site usado na publicação", sites_);
  field(panel_form_, "Título do quadro", panel_title_);
  field(panel_form_, "Descrição da arte", alt_);
  for (const auto &[id, name] :
       std::vector<std::pair<std::string, std::string>>{{"bubble", "Balão"},
                                                        {"text", "Texto livre"},
                                                        {"sfx", "Onomatopeia"}})
    kind_.append(id, name);
  ratio_.set_range(.4, 3);
  ratio_.set_increments(.1, .5);
  ratio_.set_digits(2);
  field(panel_form_, "Proporção do quadro", ratio_);
  field(form_, "Tipo de texto", kind_);
  for (const auto &[id, name] :
       std::vector<std::pair<std::string, std::string>>{
           {"rounded", "Arredondado"},
           {"oval", "Oval"},
           {"caption", "Narração"},
           {"cloud", "Pensamento"},
           {"shout", "Grito"}})
    shape_.append(id, name);
  field(form_, "Forma", shape_);
  field(form_, "Cor do texto", ink_);
  field(form_, "Fundo do balão", bubble_background_);
  field(form_, "Locutor / identificação", speaker_);
  label(form_, "Texto (HTML é gerado pelo INDE; digite texto comum)");
  text_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
  text_scroll_.set_child(text_);
  text_scroll_.set_min_content_height(110);
  form_.append(text_scroll_);
  field(form_, "Sentido do som (traduzível)", gloss_);
  field(form_, "Idioma deste texto (vazio herda a obra)", text_language_);
  for (const auto &[id, name] :
       std::vector<std::pair<std::string, std::string>>{
           {"auto", "Automática"},
           {"ltr", "Esquerda para direita"},
           {"rtl", "Direita para esquerda"}})
    direction_.append(id, name);
  field(form_, "Direção", direction_);
  form_.append(translate_);
  for (const auto &[id, name] :
       std::vector<std::pair<std::string, std::string>>{
           {"start", "Início"}, {"center", "Centro"}, {"end", "Fim"}}) {
    align_.append(id, name);
    valign_.append(id, name);
  }
  field(form_, "Alinhamento horizontal", align_);
  field(form_, "Alinhamento vertical", valign_);
  geometry_.set_column_spacing(8);
  geometry_.set_row_spacing(4);
  int row = 0;
  for (auto [name, spin] :
       std::vector<std::pair<std::string, Gtk::SpinButton *>>{
           {"X %", &x_},
           {"Y %", &y_},
           {"Largura %", &w_},
           {"Altura %", &h_},
           {"Fonte (px)", &size_}}) {
    auto *l = Gtk::make_managed<Gtk::Label>(name);
    l->set_xalign(0);
    geometry_.attach(*l, 0, row);
    spin->set_range(0, 120);
    spin->set_increments(1, 5);
    spin->set_digits(1);
    geometry_.attach(*spin, 1, row++);
  }
  form_.append(geometry_);
  for (const auto &[id, name] :
       std::vector<std::pair<std::string, std::string>>{{"none", "Nenhuma"},
                                                        {"fade", "Aparecer"},
                                                        {"float", "Flutuar"},
                                                        {"pulse", "Pulsar"}})
    animation_.append(id, name);
  field(form_, "Animação", animation_);
  field(form_, "Fonte do elemento", fonts_);
  for (const auto &[id, name] :
       std::vector<std::pair<std::string, std::string>>{
           {"general", "Geral"},
           {"ja", "Japonês"},
           {"zh-Hans", "Chinês simplificado"},
           {"zh-Hant", "Chinês tradicional"},
           {"ko", "Coreano"},
           {"ar", "Árabe"},
           {"he", "Hebraico"},
           {"hi", "Hindi"},
           {"th", "Tailandês"}})
    font_role_.append(id, name);
  font_role_.set_active_id("general");
  field(book_form_, "Uso da próxima fonte complementar", font_role_);
  book_form_.append(font_button_);
  field(panel_form_, "Documento de origem para cópia explícita", documents_);
  panel_form_.append(copy_document_);
  label(panel_form_,
        "Copiar cria texto próprio do comic e registra a origem. Editar o "
        "comic não altera o Documento e não há sincronização automática.");
  status_.set_wrap(true);
  status_.set_selectable(true);
  status_.set_xalign(0);
  append(status_);
  auto on_edit = [this] {
    if (!binding_)
      guard([&] {
        checkpoint("properties");
        update();
        this->changed();
      });
  };
  for (auto *entry : {&title_, &slug_, &language_, &panel_title_, &alt_,
                      &speaker_, &gloss_, &text_language_})
    entry->signal_changed().connect(on_edit);
  text_.get_buffer()->signal_changed().connect(on_edit);
  g_signal_connect(
      text_.gobj(), "preedit-changed",
      G_CALLBACK(+[](GtkTextView *, gchar *preedit, gpointer data) {
        auto *self = static_cast<ComicWorkspace *>(data);
        self->ime_end_.disconnect();
        self->edit_group_.clear();
        if (preedit && *preedit) {
          self->composing_ = true;
        } else if (self->composing_) {
          self->ime_end_ =
              Glib::signal_timeout().connect(sigc::track_object(
                                                 [self] {
                                                   self->composing_ = false;
                                                   self->edit_group_.clear();
                                                   return false;
                                                 },
                                                 *self),
                                             1);
        }
      }),
      this);
  works_.signal_changed().connect(on_edit);
  ratio_.signal_value_changed().connect(on_edit);
  ink_.signal_color_set().connect(on_edit);
  bubble_background_.signal_color_set().connect(on_edit);
  undo_button_.signal_clicked().connect([this] { undo(); });
  redo_button_.signal_clicked().connect([this] { redo(); });
  zoom_.signal_changed().connect([this] { update_zoom(); });
  panel_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    if (binding_ || !row)
      return;
    panel_index_ = row->get_index();
    element_index_ = 0;
    edit_group_.clear();
    fill_panel();
  });
  auto move_panel = [this](int delta) {
    if (!panel())
      return;
    const int other = panel_index_ + delta;
    if (other < 0 || other >= int(book_.panels.size()))
      return;
    checkpoint();
    std::swap(book_.panels[panel_index_], book_.panels[other]);
    panel_index_ = other;
    fill();
    changed();
  };
  panel_up_.signal_clicked().connect([move_panel] { move_panel(-1); });
  panel_down_.signal_clicked().connect([move_panel] { move_panel(1); });
  translate_.signal_toggled().connect(on_edit);
  for (auto *c : {&layout_, &kind_, &shape_, &direction_, &align_, &valign_,
                  &animation_, &fonts_})
    c->signal_changed().connect(on_edit);
  for (auto *spin : {&x_, &y_, &w_, &h_, &size_})
    spin->signal_value_changed().connect(on_edit);
  books_.signal_changed().connect([this] {
    if (binding_)
      return;
    guard([&] {
      if (dirty_) {
        binding_ = true;
        books_.set_active_id(book_.id);
        binding_ = false;
        throw std::runtime_error("Salve ou descarte antes de trocar o comic");
      }
      if (books_.get_active_id().empty())
        return;
      book_ = service_.book(books_.get_active_id());
      reset_history();
      panel_index_ = element_index_ = 0;
      fill();
    });
  });
  panels_.signal_changed().connect([this] {
    if (binding_)
      return;
    panel_index_ = std::max(0, panels_.get_active_row_number());
    element_index_ = 0;
    fill_panel();
  });
  elements_.signal_changed().connect([this] {
    if (binding_)
      return;
    element_index_ = std::max(0, elements_.get_active_row_number());
    fill_element();
  });
  create_.signal_clicked().connect([this] {
    guard([&] {
      if (!can_leave())
        return;
      const auto work = works_.get_active_id().raw();
      if (work.empty())
        throw std::runtime_error("Crie ou selecione primeiro uma Obra nativa");
      book_ = comic::example(work);
      reset_history();
      book_.panels.resize(1);
      book_.panels.front().elements.clear();
      book_.title = "Novo comic";
      book_.slug = "novo-comic";
      panel_index_ = element_index_ = 0;
      fill();
      changed();
    });
  });
  demo_.signal_clicked().connect([this] {
    guard([&] {
      if (!can_leave())
        return;
      const auto work = works_.get_active_id().raw();
      if (work.empty())
        throw std::runtime_error("Crie ou selecione primeiro uma Obra nativa");
      book_ = comic::example(work);
      reset_history();
      panel_index_ = element_index_ = 0;
      fill();
      changed();
    });
  });
  save_.signal_clicked().connect([this] { guard([&] { save(); }); });
  discard_.signal_clicked().connect([this] {
    guard([&] {
      dirty_ = false;
      refresh();
    });
  });
  retire_.signal_clicked().connect([this] {
    guard([&] {
      if (!can_leave() || book_.revision == 0)
        return;
      book_ = service_.retire(book_.id, book_.revision, !book_.removed);
      dirty_ = false;
      removed_.set_active(true);
      refresh();
    });
  });
  removed_.signal_toggled().connect([this] {
    if (!binding_)
      guard([&] {
        if (!can_leave()) {
          binding_ = true;
          removed_.set_active(!removed_.get_active());
          binding_ = false;
          return;
        }
        refresh();
      });
  });
  add_panel_.signal_clicked().connect([this] {
    if (book_.id.empty())
      return;
    checkpoint();
    comic::Panel p;
    p.id = project::new_uuid();
    p.title = "Novo quadro";
    book_.panels.push_back(p);
    panel_index_ = int(book_.panels.size()) - 1;
    element_index_ = 0;
    fill();
    changed();
  });
  remove_panel_.signal_clicked().connect([this] {
    if (!panel())
      return;
    checkpoint();
    book_.panels.erase(book_.panels.begin() + panel_index_);
    panel_index_ = std::max(0, panel_index_ - 1);
    element_index_ = 0;
    fill();
    changed();
  });
  add_element_.signal_clicked().connect([this] {
    if (!panel())
      return;
    checkpoint();
    comic::Element e;
    e.id = project::new_uuid();
    e.text = "Nova fala";
    panel()->elements.push_back(e);
    element_index_ = int(panel()->elements.size()) - 1;
    fill_panel();
    changed();
  });
  remove_element_.signal_clicked().connect([this] {
    if (!element())
      return;
    checkpoint();
    panel()->elements.erase(panel()->elements.begin() + element_index_);
    element_index_ = std::max(0, element_index_ - 1);
    fill_panel();
    changed();
  });
  copy_document_.signal_clicked().connect([this] {
    guard([&] {
      if (!panel())
        return;
      auto d =
          publication_.source().writing().document(documents_.get_active_id());
      if (!d)
        throw std::runtime_error("Escolha um Documento");
      checkpoint();
      auto e = comic::copy_document(*d);
      panel()->elements.push_back(e);
      element_index_ = int(panel()->elements.size()) - 1;
      fill_panel();
      changed();
    });
  });
  image_button_.signal_clicked().connect(
      [this] { guard([&] { choose_asset(false); }); });
  font_button_.signal_clicked().connect(
      [this] { guard([&] { choose_asset(true); }); });
  generate_.signal_clicked().connect([this] { guard([&] { generate(); }); });
  preview_.signal_clicked().connect([this] { guard([&] { preview(); }); });
  auto drag = Gtk::GestureDrag::create();
  drag->signal_drag_begin().connect([this](double x, double y) {
    if (!panel() || book_.removed)
      return;
    canvas_.grab_focus();
    const double px = (x - canvas_left_) / canvas_width_ * 100,
                 py = (y - canvas_top_) / canvas_height_ * 100;
    for (int i = int(panel()->elements.size()) - 1; i >= 0; --i) {
      const auto &e = panel()->elements[i];
      if (px >= e.x && px <= e.x + e.w && py >= e.y && py <= e.y + e.h) {
        element_index_ = i;
        fill_panel();
        checkpoint();
        drag_x_ = e.x;
        drag_y_ = e.y;
        drag_w_ = e.w;
        drag_h_ = e.h;
        resizing_ =
            std::abs(px - (e.x + e.w)) < 3 && std::abs(py - (e.y + e.h)) < 3;
        dragging_ = true;
        break;
      }
    }
  });
  drag->signal_drag_update().connect([this](double dx, double dy) {
    if (!dragging_ || !element())
      return;
    if (resizing_) {
      element()->w = std::clamp(drag_w_ + dx / canvas_width_ * 100, 5.,
                                100 - element()->x);
      element()->h = std::clamp(drag_h_ + dy / canvas_height_ * 100, 5.,
                                100 - element()->y);
    } else {
      guide_ =
          comic::snap(*element(), drag_x_ + dx / canvas_width_ * 100,
                      drag_y_ + dy / canvas_height_ * 100, snap_.get_active());
      element()->x = guide_.x;
      element()->y = guide_.y;
    }
    fill_element();
    changed();
  });
  drag->signal_drag_end().connect([this](double, double) {
    dragging_ = false;
    guide_ = {};
    canvas_.queue_draw();
  });
  canvas_.add_controller(drag);
  auto keys = Gtk::EventControllerKey::create();
  keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  keys->signal_key_pressed().connect(
      [this](guint key, guint, Gdk::ModifierType modifiers) {
        if (composing_)
          return false;
        const auto flags = static_cast<unsigned>(modifiers);
        if (flags & static_cast<unsigned>(Gdk::ModifierType::CONTROL_MASK)) {
          if (key == GDK_KEY_z) {
            if (flags & static_cast<unsigned>(Gdk::ModifierType::SHIFT_MASK))
              redo();
            else
              undo();
            return true;
          }
          if (key == GDK_KEY_y) {
            redo();
            return true;
          }
        }
        if (!canvas_.has_focus() || !element() || book_.removed)
          return false;
        double dx = 0, dy = 0;
        if (key == GDK_KEY_Left)
          dx = -1;
        else if (key == GDK_KEY_Right)
          dx = 1;
        else if (key == GDK_KEY_Up)
          dy = -1;
        else if (key == GDK_KEY_Down)
          dy = 1;
        else
          return false;
        if (flags & static_cast<unsigned>(Gdk::ModifierType::SHIFT_MASK)) {
          dx *= 5;
          dy *= 5;
        }
        checkpoint("nudge");
        auto p = comic::snap(*element(), element()->x + dx, element()->y + dy,
                             false);
        element()->x = p.x;
        element()->y = p.y;
        fill_element();
        changed();
        return true;
      },
      false);
  add_controller(keys);
}
ComicWorkspace::~ComicWorkspace() {
  ime_end_.disconnect();
  g_signal_handlers_disconnect_by_data(text_.gobj(), this);
  lifetime_.reset();
  if (file_)
    file_->hide();
  for (auto *p : {job_, preview_job_})
    if (p) {
      g_subprocess_force_exit(p);
      g_object_unref(p);
    }
}
void ComicWorkspace::guard(const std::function<void()> &fn) {
  try {
    fn();
  } catch (const std::exception &e) {
    restoring_ = false;
    status_.set_text(e.what());
  }
}
comic::Panel *ComicWorkspace::panel() {
  return panel_index_ >= 0 && panel_index_ < int(book_.panels.size())
             ? &book_.panels[panel_index_]
             : nullptr;
}
comic::Element *ComicWorkspace::element() {
  auto *p = panel();
  return p && element_index_ >= 0 && element_index_ < int(p->elements.size())
             ? &p->elements[element_index_]
             : nullptr;
}
void ComicWorkspace::changed() {
  if (binding_)
    return;
  for (const auto &[id, a] : book_.assets)
    if (!asset_data_.contains(id))
      asset_data_[id] = a.data;
  dirty_ =
      book_.revision == 0 ? !book_.id.empty() : signature() != saved_signature_;
  update_history_buttons();
  update_zoom();
  prepared_.reset();
  preview_.set_sensitive(false);
  status_.set_text("Alterações na comic. Você pode alternar de espaço; salve "
                   "antes de fechar o Projeto ou preparar a publicação.");
  canvas_.queue_draw();
}
bool ComicWorkspace::can_leave() {
  if (composing_) {
    status_.set_text("Confirme ou cancele a entrada de texto antes de sair.");
    return false;
  }
  if (job_ || file_) {
    status_.set_text("Conclua a operação em andamento primeiro.");
    return false;
  }
  if (dirty_) {
    status_.set_text("Salve ou descarte as alterações do comic primeiro.");
    return false;
  }
  return true;
}
void ComicWorkspace::refresh() {
  if (!can_leave())
    return;
  const auto *p = publication_.source().current();
  if (!p)
    throw std::runtime_error("Abra um Projeto");
  if (project_id_ != p->manifest().project_id) {
    project_id_ = p->manifest().project_id;
    book_ = {};
  }
  binding_ = true;
  works_.remove_all();
  for (const auto &w : publication_.source().catalog().works)
    works_.append(w.id, w.title);
  if (!book_.work_id.empty())
    works_.set_active_id(book_.work_id);
  else
    works_.set_active(0);
  sites_.remove_all();
  for (const auto &s : publication_.sites())
    sites_.append(s.id, s.name);
  sites_.set_active(0);
  books_.remove_all();
  const auto books = service_.books(removed_.get_active());
  bool found = false;
  for (const auto &b : books) {
    books_.append(b.id, b.title + (b.removed ? " · retirado" : ""));
    if (b.id == book_.id) {
      book_ = b;
      found = true;
    }
  }
  if (!found)
    book_ = books.empty() ? comic::Book{} : books.front();
  books_.set_active_id(book_.id);
  documents_.remove_all();
  for (const auto &d : publication_.source().writing().documents())
    documents_.append(d.id, d.title);
  documents_.set_active(0);
  binding_ = false;
  panel_index_ = element_index_ = 0;
  reset_history();
  fill();
  status_.set_text("Escolha uma Obra e crie um comic ou experimente a amostra. "
                   "O catálogo misto será integrado no próximo corte.");
}
void ComicWorkspace::fill() {
  prepared_.reset();
  preview_.set_sensitive(false);
  binding_ = true;
  location_.set_text(book_.title);
  location_.set_tooltip_text(book_.title);
  title_.set_text(book_.title);
  slug_.set_text(book_.slug);
  language_.set_text(book_.lang);
  layout_.set_active_id(book_.layout);
  if (!book_.work_id.empty())
    works_.set_active_id(book_.work_id);
  works_.set_sensitive(book_.revision == 0);
  panels_.remove_all();
  for (const auto &p : book_.panels)
    panels_.append(p.id, p.title);
  panels_.set_active(panel_index_);
  while (auto *child = panel_list_.get_first_child())
    panel_list_.remove(*child);
  int sequence = 0;
  for (const auto &p : book_.panels) {
    auto *label = Gtk::make_managed<Gtk::Label>(std::to_string(++sequence) +
                                                "  " + p.title);
    label->set_xalign(0);
    label->set_margin(10);
    label->set_ellipsize(Pango::EllipsizeMode::END);
    panel_list_.append(*label);
  }
  if (auto *row = panel_list_.get_row_at_index(panel_index_))
    panel_list_.select_row(*row);
  retire_.set_label(book_.removed ? "Restaurar comic" : "Retirar comic");
  retire_.set_sensitive(book_.revision > 0);
  save_.set_sensitive(!book_.removed && !book_.id.empty());
  book_form_.set_sensitive(!book_.removed);
  panel_form_.set_sensitive(!book_.removed && panel());
  add_panel_.set_sensitive(!book_.removed && !book_.id.empty());
  remove_panel_.set_sensitive(!book_.removed);
  add_element_.set_sensitive(!book_.removed);
  remove_element_.set_sensitive(!book_.removed);
  image_button_.set_sensitive(!book_.removed);
  generate_.set_sensitive(!book_.removed && book_.revision > 0);
  binding_ = false;
  fill_panel();
  update_history_buttons();
}
void ComicWorkspace::fill_panel() {
  binding_ = true;
  image_.reset();
  panel_up_.set_sensitive(!book_.removed && panel_index_ > 0);
  panel_down_.set_sensitive(!book_.removed &&
                            panel_index_ + 1 < int(book_.panels.size()));
  elements_.remove_all();
  if (auto *p = panel()) {
    panel_title_.set_text(p->title);
    alt_.set_text(p->alt);
    ratio_.set_value(p->ratio);
    for (const auto &e : p->elements)
      elements_.append(e.id, e.speaker.empty()
                                 ? Glib::ustring(e.text).substr(0, 40)
                                 : Glib::ustring(e.speaker));
    elements_.set_active(element_index_);
    if (!p->image.empty()) {
      const auto &a = book_.assets.at(p->image);
      const auto raw = comic::raw_asset(a);
      auto loader = Gdk::PixbufLoader::create(a.mime, true);
      loader->write(reinterpret_cast<const guint8 *>(raw.data()), raw.size());
      loader->close();
      image_ = loader->get_pixbuf();
    }
  } else {
    panel_title_.set_text("");
    alt_.set_text("");
  }
  binding_ = false;
  fill_element();
  update_zoom();
  canvas_.queue_draw();
}
void ComicWorkspace::fill_element() {
  binding_ = true;
  auto *e = element();
  comic::Element empty;
  const auto &v = e ? *e : empty;
  form_.set_sensitive(e && !book_.removed);
  text_.get_buffer()->set_text(v.text);
  speaker_.set_text(v.speaker);
  gloss_.set_text(v.gloss);
  text_language_.set_text(v.lang);
  direction_.set_active_id(v.dir);
  translate_.set_active(v.translate);
  kind_.set_active_id(v.kind);
  Gdk::RGBA color;
  color.set(v.ink);
  ink_.set_rgba(color);
  color.set(v.background);
  bubble_background_.set_rgba(color);
  shape_.set_active_id(v.shape);
  align_.set_active_id(v.align);
  valign_.set_active_id(v.valign);
  animation_.set_active_id(v.animation);
  x_.set_value(v.x);
  y_.set_value(v.y);
  w_.set_value(v.w);
  h_.set_value(v.h);
  size_.set_value(v.size);
  fonts_.remove_all();
  fonts_.append("", "Fonte do sistema");
  for (const auto &[id, a] : book_.assets)
    if (a.mime.starts_with("font/"))
      fonts_.append(id, a.name);
  fonts_.set_active_id(v.font_asset);
  binding_ = false;
  canvas_.queue_draw();
}
void ComicWorkspace::update() {
  book_.title = title_.get_text();
  book_.slug = slug_.get_text();
  book_.lang = language_.get_text();
  book_.layout = layout_.get_active_id();
  if (book_.revision == 0)
    book_.work_id = works_.get_active_id();
  if (auto *p = panel()) {
    p->title = panel_title_.get_text();
    p->alt = alt_.get_text();
    p->ratio = ratio_.get_value();
  }
  if (auto *e = element()) {
    e->text = text_.get_buffer()->get_text();
    e->speaker = speaker_.get_text();
    e->gloss = gloss_.get_text();
    e->lang = text_language_.get_text();
    e->dir = direction_.get_active_id();
    e->translate = translate_.get_active();
    e->kind = kind_.get_active_id();
    const auto hex = [](const Gdk::RGBA &c) {
      char out[8];
      std::snprintf(out, sizeof out, "#%02x%02x%02x",
                    int(std::lround(c.get_red() * 255)),
                    int(std::lround(c.get_green() * 255)),
                    int(std::lround(c.get_blue() * 255)));
      return std::string(out);
    };
    e->ink = hex(ink_.get_rgba());
    e->background = hex(bubble_background_.get_rgba());
    e->shape = shape_.get_active_id();
    e->align = align_.get_active_id();
    e->valign = valign_.get_active_id();
    e->animation = animation_.get_active_id();
    e->font_asset = fonts_.get_active_id();
    e->w = std::clamp(w_.get_value(), 5., 100.);
    e->h = std::clamp(h_.get_value(), 5., 100.);
    e->x = std::clamp(x_.get_value(), 0., 100 - e->w);
    e->y = std::clamp(y_.get_value(), 0., 100 - e->h);
    e->size = std::clamp(size_.get_value(), 10., 120.);
  }
}
void ComicWorkspace::save() {
  if (composing_)
    throw std::runtime_error(
        "Confirme ou cancele a entrada de texto antes de salvar.");
  if (book_.removed || book_.id.empty())
    throw std::runtime_error("Crie ou restaure um comic");
  update();
  book_ = service_.save(book_, book_.revision);
  dirty_ = false;
  saved_signature_ = signature();
  sync_sources();
  fill();
  status_.set_text("Comic salvo no Projeto · revisão " +
                   std::to_string(book_.revision));
}
void ComicWorkspace::choose_asset(bool font) {
  if (!panel() || book_.removed || file_)
    return;
  auto *window = dynamic_cast<Gtk::Window *>(get_root());
  if (!window)
    throw std::runtime_error("Janela indisponível");
  file_ = Gtk::FileChooserNative::create(
      font ? "Importar fonte complementar" : "Importar ilustração", *window,
      Gtk::FileChooser::Action::OPEN, "Importar", "Cancelar");
  const auto panel_id = panel()->id;
  file_->signal_response().connect([this, font, panel_id](int response) {
    guard([&] {
      auto dialog = file_;
      file_.reset();
      dialog->hide();
      if (response != int(Gtk::ResponseType::ACCEPT))
        return;
      auto file = dialog->get_file();
      if (!file)
        return;
      auto a = comic::import_asset(file->get_path(), font);
      if (font)
        a.role = font_role_.get_active_id();
      auto candidate = book_;
      candidate.assets[a.id] = a;
      if (!font) {
        auto p = std::find_if(candidate.panels.begin(), candidate.panels.end(),
                              [&](const auto &p) { return p.id == panel_id; });
        if (p == candidate.panels.end())
          throw std::runtime_error("Quadro mudou; importe novamente");
        p->image = a.id;
      }
      comic::validate(candidate);
      checkpoint();
      book_ = std::move(candidate);
      fill_panel();
      changed();
      status_.set_text(
          font ? "Fonte incorporada. Confira o desenho exato na prévia web."
               : "Ilustração incorporada ao comic.");
    });
  });
  file_->set_modal(true);
  file_->show();
}
void ComicWorkspace::generate() {
  if (!can_leave())
    return;
  if (book_.revision == 0)
    throw std::runtime_error("Salve primeiro o comic");
  const auto site_id = sites_.get_active_id().raw();
  if (site_id.empty())
    throw std::runtime_error("Crie um Site na primeira aba e selecione-o aqui");
  const auto site = publication_.site(site_id);
  prepared_ = service_.prepare(book_.id, site.id, book_.revision, site.revision,
                               resources() / "recursos");
  const auto input = service_.materialize(*prepared_);
  output_ = publication_.source().current()->path() / "outputs/comics" /
            prepared_->id / "site";
  job_ =
      spawn({"python3", (resources() / "publicacao.py").string(),
             "--gerar-edicao", input.string(), "--destino", output_.string()});
  top_.set_sensitive(false);
  body_.set_sensitive(false);
  actions_.set_sensitive(false);
  status_.set_text("Preparando site do comic com a identidade do Site…");
  using Context = std::pair<ComicWorkspace *, std::weak_ptr<int>>;
  auto *context = new Context(this, lifetime_);
  g_subprocess_wait_async(
      job_, nullptr,
      [](GObject *object, GAsyncResult *result, gpointer data) {
        std::unique_ptr<Context> context(static_cast<Context *>(data));
        GError *error = nullptr;
        const bool ok =
            g_subprocess_wait_finish(G_SUBPROCESS(object), result, &error) &&
            g_subprocess_get_successful(G_SUBPROCESS(object));
        g_clear_error(&error);
        if (context->second.expired())
          return;
        auto *self = context->first;
        g_object_unref(self->job_);
        self->job_ = nullptr;
        self->top_.set_sensitive(true);
        self->body_.set_sensitive(true);
        self->actions_.set_sensitive(true);
        const bool current = ok && self->prepared_ &&
                             self->service_.is_current(*self->prepared_);
        self->preview_.set_sensitive(current);
        self->status_.set_text(
            current ? "Comic preparado. Pacote público: " +
                          (self->output_.parent_path() / "site-publico.zip")
                              .string()
                    : "A geração falhou ou a fonte mudou. Prepare novamente.");
      },
      context);
}
void ComicWorkspace::preview() {
  if (!prepared_ || !service_.is_current(*prepared_))
    throw std::runtime_error("A fonte mudou; prepare novamente");
  if (preview_job_) {
    g_subprocess_send_signal(preview_job_, SIGTERM);
    g_object_unref(preview_job_);
    preview_job_ = nullptr;
  }
  preview_job_ =
      spawn({"python3", (resources() / "publicacao.py").string(),
             "--servir-edicao", output_.string(), "--controle-previa",
             (output_.parent_path() / "previa-servidor.json").string()});
  status_.set_text(
      "Prévia web do comic aberta pelo fluxo de publicação existente.");
}
void ComicWorkspace::draw(const Cairo::RefPtr<Cairo::Context> &cr, int width,
                          int height) {
  cr->set_source_rgb(.10, .13, .17);
  cr->paint();
  auto *p = panel();
  if (!p)
    return;
  canvas_width_ =
      zoom_factor_ > 0
          ? 900 * zoom_factor_
          : std::max(1., std::min(double(width) - 32,
                                  (double(height) - 32) * p->ratio));
  canvas_height_ = canvas_width_ / p->ratio;
  canvas_left_ = (width - canvas_width_) / 2;
  canvas_top_ = (height - canvas_height_) / 2;
  cr->save();
  cr->rectangle(canvas_left_, canvas_top_, canvas_width_, canvas_height_);
  cr->clip();
  color(cr, p->background);
  cr->paint();
  if (image_) {
    cr->save();
    const double scale = std::max(canvas_width_ / image_->get_width(),
                                  canvas_height_ / image_->get_height());
    cr->translate(
        canvas_left_ + (canvas_width_ - image_->get_width() * scale) / 2,
        canvas_top_ + (canvas_height_ - image_->get_height() * scale) / 2);
    cr->scale(scale, scale);
    Gdk::Cairo::set_source_pixbuf(cr, image_, 0, 0);
    cr->paint();
    cr->restore();
  }
  for (std::size_t i = 0; i < p->elements.size(); ++i) {
    const auto &e = p->elements[i];
    const double x = canvas_left_ + e.x / 100 * canvas_width_,
                 y = canvas_top_ + e.y / 100 * canvas_height_,
                 w = e.w / 100 * canvas_width_, h = e.h / 100 * canvas_height_;
    cr->save();
    if (e.kind == "bubble") {
      if (e.shape == "oval") {
        cr->save();
        cr->translate(x + w / 2, y + h / 2);
        cr->scale(w / 2, h / 2);
        cr->arc(0, 0, 1, 0, 2 * M_PI);
        cr->restore();
      } else
        rounded(cr, x, y, w, h, e.shape == "caption" ? 2 : 18);
      color(cr, e.background);
      cr->fill_preserve();
      color(cr, e.ink);
      cr->set_line_width(e.shape == "shout" ? 3 : 1.4);
      if (e.shape == "cloud")
        cr->set_dash(std::vector<double>{3, 3}, 0);
      cr->stroke();
    }
    cr->rectangle(x + 5, y + 5, w - 10, h - 10);
    cr->clip();
    auto layout = canvas_.create_pango_layout(
        e.text + (e.gloss.empty() ? "" : "\n" + e.gloss));
    Pango::FontDescription font;
    font.set_family(e.font);
    font.set_absolute_size(e.size * canvas_width_ / 900 * Pango::SCALE);
    if (e.kind == "sfx")
      font.set_weight(Pango::Weight::BOLD);
    layout->set_font_description(font);
    layout->set_width(static_cast<int>(std::max(1., w - 14) * Pango::SCALE));
    layout->set_wrap(Pango::WrapMode::WORD_CHAR);
    layout->set_alignment(e.align == "center" ? Pango::Alignment::CENTER
                          : e.align == "end"  ? Pango::Alignment::RIGHT
                                              : Pango::Alignment::LEFT);
    int tw, th;
    layout->get_pixel_size(tw, th);
    const double ty = e.valign == "center" ? std::max(6., (h - th) / 2)
                      : e.valign == "end"  ? std::max(6., h - th - 6)
                                           : 6;
    cr->move_to(x + 7, y + ty);
    color(cr, e.ink);
    layout->show_in_cairo_context(cr);
    cr->restore();
    if (int(i) == element_index_) {
      cr->set_source_rgb(.35, .85, .75);
      cr->set_line_width(2);
      cr->rectangle(x, y, w, h);
      cr->stroke();
      cr->rectangle(x + w - 7, y + h - 7, 7, 7);
      cr->fill();
    }
  }
  if (dragging_) {
    cr->set_source_rgb(.95, .5, .7);
    cr->set_line_width(1);
    if (guide_.snap_x) {
      cr->move_to(canvas_left_ + canvas_width_ / 2, canvas_top_);
      cr->line_to(canvas_left_ + canvas_width_ / 2,
                  canvas_top_ + canvas_height_);
    }
    if (guide_.snap_y) {
      cr->move_to(canvas_left_, canvas_top_ + canvas_height_ / 2);
      cr->line_to(canvas_left_ + canvas_width_,
                  canvas_top_ + canvas_height_ / 2);
    }
    cr->stroke();
  }
  cr->restore();
}
} // namespace inde::ui
