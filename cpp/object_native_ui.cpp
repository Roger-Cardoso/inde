#include "object_native_ui.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/accessibility.hpp"
#include "object_composite_pilot.hpp"
#include "object_formula_hub.hpp"
#include "object_math_native.hpp"
#include "object_native_probe.hpp"
#include "object_table_design.hpp"
#include "object_text_projection.hpp"
#include <cerrno>
#include <charconv>
#include <cmath>
#include <fstream>
#include <glibmm/base64.h>
#include <map>
#include <mutex>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
namespace inde::ui {
namespace {
using V = persistence::json::Value;
using O = V::Object;
using A = V::Array;
std::string text(const V &v, const std::string &key) {
  return v.at(key).as_string();
}
double real(const V &v) {
  if (const auto *d = std::get_if<double>(&v.storage()))
    return *d;
  return static_cast<double>(v.as_integer());
}
bool has(const V &v, const std::string &key) {
  return std::holds_alternative<O>(v.storage()) && v.as_object().contains(key);
}
const V *find(const V &state, const char *list, const std::string &id) {
  for (const auto &v : state.at(list).as_array())
    if (text(v, "id") == id)
      return &v;
  return nullptr;
}
void clear(Gtk::Box &box) {
  while (auto *w = box.get_first_child())
    box.remove(*w);
}
Gtk::Label &label(Gtk::Box &box, const std::string &s) {
  auto *l = Gtk::make_managed<Gtk::Label>(s);
  l->set_wrap(true);
  l->set_xalign(0);
  box.append(*l);
  return *l;
}
Gtk::Button &button(Gtk::Box &box, const std::string &s,
                    const std::string &name, const std::function<void()> &fn) {
  auto *b = Gtk::make_managed<Gtk::Button>(s);
  b->set_name(name);
  b->signal_clicked().connect(fn);
  if (auto *l = dynamic_cast<Gtk::Label *>(b->get_child())) {
    l->set_wrap(true);
    l->set_max_width_chars(36);
  }
  box.append(*b);
  return *b;
}
void limit_choice(Gtk::ComboBoxText &combo) {
  for (auto *c : combo.get_cells())
    if (auto *t = dynamic_cast<Gtk::CellRendererText *>(c)) {
      t->property_ellipsize() = Pango::EllipsizeMode::END;
      t->property_max_width_chars() = 24;
      t->property_width_chars() = 10;
    }
  combo.signal_changed().connect(
      [&combo] { combo.set_tooltip_text(combo.get_active_text()); });
}
void private_directory(const std::filesystem::path &p) {
  if (::mkdir(p.c_str(), 0700) != 0 && errno != EEXIST)
    throw std::runtime_error(
        "Não foi possível criar a pasta dos documentos compostos.");
  struct stat st {};
  if (::lstat(p.c_str(), &st) != 0 || !S_ISDIR(st.st_mode) ||
      st.st_uid != ::geteuid() || (st.st_mode & 077) != 0)
    throw std::runtime_error("A pasta dos documentos compostos precisa ser "
                             "privada e não pode ser um link.");
}
// Native GTK layout, with proportional children and height-for-width. Used for
// both side-by-side objects and table columns; no fixed-width web canvas.
class FractionRow final : public Gtk::Widget {
  std::vector<Gtk::Widget *> children_;
  std::vector<double> weights_;
  int gap_;
  sigc::connection disposal_;
  void release_children() {
    for (auto *c : children_)
      c->unparent();
    children_.clear();
  }

public:
  FractionRow(std::vector<Gtk::Widget *> c, std::vector<double> w, int gap)
      : Glib::ObjectBase("IndeObjectFractionRow"), Gtk::Widget(),
        children_(std::move(c)), weights_(std::move(w)), gap_(gap) {
    disposal_ = signal_destroy().connect([this] { release_children(); });
    for (auto *v : children_)
      v->set_parent(*this);
    set_hexpand(true);
  }
  ~FractionRow() override {
    disposal_.disconnect();
    release_children();
  }

protected:
  Gtk::SizeRequestMode get_request_mode_vfunc() const override {
    return Gtk::SizeRequestMode::HEIGHT_FOR_WIDTH;
  }
  void measure_vfunc(Gtk::Orientation o, int size, int &minimum, int &natural,
                     int &mb, int &nb) const override {
    minimum = natural = 0;
    mb = nb = -1;
    const int n = static_cast<int>(children_.size());
    for (int i = 0; i < n; ++i) {
      int a = 0, b = 0, c = 0, d = 0;
      const int width =
          size < 0 ? -1
                   : std::max(1, static_cast<int>(
                                     (size - gap_ * (n - 1)) *
                                     weights_[static_cast<std::size_t>(i)]));
      children_[static_cast<std::size_t>(i)]->measure(
          o, o == Gtk::Orientation::VERTICAL ? width : -1, a, b, c, d);
      if (o == Gtk::Orientation::HORIZONTAL) {
        minimum += a;
        natural += b;
      } else {
        minimum = std::max(minimum, a);
        natural = std::max(natural, b);
      }
    }
    if (o == Gtk::Orientation::HORIZONTAL) {
      minimum += gap_ * std::max(n - 1, 0);
      natural += gap_ * std::max(n - 1, 0);
    }
  }
  void size_allocate_vfunc(int width, int height, int) override {
    int x = 0;
    const auto n = children_.size();
    for (std::size_t i = 0; i < n; ++i) {
      int w = i + 1 == n
                  ? width - x
                  : std::max(1, static_cast<int>(
                                    (width - gap_ * static_cast<int>(n - 1)) *
                                    weights_[i]));
      int minimum = 0, natural = 0, mb = -1, nb = -1;
      children_[i]->measure(Gtk::Orientation::VERTICAL, w, minimum, natural, mb,
                            nb);
      const auto align = children_[i]->get_valign();
      const int h =
          align == Gtk::Align::FILL ? height : std::min(height, natural);
      const int y = align == Gtk::Align::END      ? height - h
                    : align == Gtk::Align::CENTER ? (height - h) / 2
                                                  : 0;
      graphene_point_t p{static_cast<float>(x), static_cast<float>(y)};
      gtk_widget_allocate(children_[i]->gobj(), w, h, -1,
                          gsk_transform_translate(nullptr, &p));
      x += w + gap_;
    }
  }
  void snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot> &s) override {
    for (auto *c : children_)
      gtk_widget_snapshot_child(gobj(), c->gobj(), s->gobj());
  }
};
struct Runtime {
  std::mutex mutex;
  std::unique_ptr<objects::pilot::Session> session;
};
struct Life {
  bool alive = true;
};
struct Work {
  std::shared_ptr<Runtime> runtime;
  std::shared_ptr<Life> life;
  V input, result;
  std::string context;
  std::function<void(const V &)> done;
};
const std::map<std::string, std::string> messages = {
    {"stale_reading_drag", "O documento mudou durante a movimentação. Escolha o bloco e o destino novamente."},
    {"png_read_failed", "Não foi possível ler o arquivo escolhido. Escolha a figura novamente."},
    {"png_byte_limit", "A figura deve ser um PNG com até 8 MiB."},
    {"png_dimensions_limit", "Use uma figura com até 8192 pixels por lado e 16 milhões de pixels no total."},
    {"invalid_png", "O arquivo escolhido não é um PNG válido."},
    {"png_crc_mismatch", "O PNG possui dados inconsistentes. Escolha outra cópia da figura."},
    {"invalid_png_chunk", "A estrutura do PNG está incompleta. Escolha outra cópia da figura."},
    {"invalid_png_end", "O arquivo PNG está incompleto ou contém dados após o fim da imagem."},
    {"invalid_png_header", "O cabeçalho do PNG não é válido. Escolha outra figura."},
    {"png_decode_failed", "Não foi possível decodificar esse PNG. Escolha outra cópia da figura."},
    {"animated_png_not_supported", "Essa figura é um PNG animado. Use uma versão estática."},
    {"authorial_requires_alignment",
     "Restaure a apresentação pela ordem de leitura antes de mudar a sequência "
     "autoral."},
    {"owner_text_move_refused", "Esse movimento dividiria um grupo textual "
                                "protegido. Escolha outro destino."},
    {"formula_symbol_immutable",
     "A referência dessa variável já está em uso. Conserve sua identificação."},
    {"formula_cycle",
     "Há uma dependência circular. Confira as referências da fórmula."},
    {"formula_reference_missing", "A fórmula referenciada não foi encontrada."},
    {"formula_variable_missing", "A variável referenciada não foi encontrada."},
    {"formula_division_zero", "O cálculo tenta dividir por zero."},
    {"formula_identity", "Use uma identificação única, começando por letra ou "
                         "sublinhado, sem espaços."},
    {"formula_cell_already_bound",
     "A célula já contém uma fórmula. Edite-a ou retire seu uso primeiro."},
    {"formula_command_must_be_alone",
     "Uma alteração de uso único deve ficar sozinha, sem frase ou resultado."},
    {"formula_execution_would_break_dependency",
     "Essa alteração deixaria outro resultado inválido. Os valores foram "
     "preservados."},
    {"formula_cards_invalid", "Confira os campos e as condições preenchidas. "
                              "São permitidas até 12 condições."},
    {"invalid_table_design",
     "Confira as medidas, cores e proporções da tabela."},
    {"invalid_presentation",
     "Use largura entre 20 e 100% e confira os alinhamentos escolhidos."},
    {"invalid_table_dimensions", "Use de 1 a 64 linhas e de 1 a 32 colunas."},
    {"destination_inside_row", "Esse destino está dentro de um arranjo lado a "
                               "lado. Desfaça o arranjo antes de mover."},
    {"persistence_requires_reopen",
     "Reabra o trabalho para conferir o resultado da gravação antes de "
     "continuar."},
    {"alt_text_required", "Descreva a figura ou marque-a como decorativa."}};
} // namespace
struct ObjectDocumentEditor::Impl {
  ObjectDocumentEditor &owner;
  Gtk::Window &window;
  Glib::RefPtr<Gdk::Display> display;
  project::Document source_document;
  Gtk::Popover object_menu;
  Gtk::Box *menu_holder = nullptr;
  void close_object_menu() {
    object_menu.popdown();
    auto *scroll = dynamic_cast<Gtk::ScrolledWindow *>(object_menu.get_child());
    auto *child = scroll ? scroll->get_child() : object_menu.get_child();
    auto *viewport = dynamic_cast<Gtk::Viewport *>(child);
    if(viewport)child=viewport->get_child();
    if (child) {
      g_object_ref(child->gobj());
      if(viewport)viewport->unset_child();
      if(scroll)scroll->unset_child();
      object_menu.unset_child();
      child->set_visible(false);
      if (menu_holder)
        menu_holder->append(*child);
      g_object_unref(child->gobj());
    }
    if (object_menu.get_parent())
      object_menu.unparent();
    menu_holder = nullptr;
  }
  std::map<std::string, Gtk::Box *> menus;
  std::map<std::string, std::function<void()>> menu_updates;
  std::map<std::string, Gtk::Widget *> cards, reading_handles;
  Gtk::Widget *specialty_panel = nullptr;
  Gtk::Box specialty_host{Gtk::Orientation::VERTICAL, 8};
  Gtk::Button *tools_toggle = nullptr;
  sigc::connection drag_scroll, tool_follow;
  std::string dragging, drag_reference, restore_handle;
  bool drag_after = false;
  double drag_y = 0, drag_initial_scroll = 0, drag_delta = 0;
  std::int64_t drag_revision = 0;
  std::string specialty = "composition";
  std::optional<std::map<std::string, std::string>> visible_text;
  bool shown(const std::string &id) const {
    const auto *p = find(state, "paragraphs", id);
    return !p || !visible_text || visible_text->contains(text(*p, "paragraph"));
  }
  std::map<std::string, ObjectTextProjection *> paragraph_views;
  sigc::connection cursor_follow, focus_follow;
  Gtk::Revealer side_revealer;
  std::map<std::string, Gtk::Box *> tool_groups;
  Gtk::Box root{Gtk::Orientation::VERTICAL, 8},
      toolbar{Gtk::Orientation::HORIZONTAL, 6},
      sidebar{Gtk::Orientation::VERTICAL, 8},
      canvas{Gtk::Orientation::VERTICAL, 18},
      editor{Gtk::Orientation::VERTICAL, 10},
      edit_page{Gtk::Orientation::VERTICAL, 8},
      edit_actions{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::ScrolledWindow side_scroll, content_scroll, edit_scroll;
  Gtk::Box surface_row{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Stack stack;
  Gtk::Label status;
  Gtk::ComboBoxText objects, formulas, context;
  std::shared_ptr<Runtime> runtime = std::make_shared<Runtime>();
  std::shared_ptr<Life> life = std::make_shared<Life>();
  V state;
  std::filesystem::path project_path;
  bool busy = false, draft = false, rendering = false, render_pending = true, choosing_image = false, restoring_text_focus = false,
       draft_tools_open = false;
  std::size_t pending_actions = 0;
  sigc::connection chooser_response;
  std::string last_error, selected;
  Glib::RefPtr<Gtk::CssProvider> css = Gtk::CssProvider::create();
  std::map<std::string, Glib::RefPtr<Gdk::Texture>> images;
  std::map<std::string, Gtk::Entry *> fields;
  std::map<std::string, Gtk::ComboBoxText *> choices;
  std::map<std::string, Gtk::TextView *> texts;
  std::map<std::string, Gtk::CheckButton *> checks;
  struct Rule {
    Gtk::ComboBoxText *op;
    Gtk::Entry *value, *result;
    Gtk::Box *row;
  };
  std::vector<Rule> rules;
  struct Run {
    Gtk::ComboBoxText *kind, *formula;
    Gtk::TextView *literal;
    Gtk::Box *row;
  };
  std::vector<Run> runs;
  Gtk::Box *rule_box = nullptr, *run_box = nullptr;
  Gtk::Label *preview = nullptr;
  MathPreview *math_preview = nullptr;
  V draft_model;
  std::string editor_id, editor_target, editor_table, editor_cell, editor_kind;
  std::string image_bytes;
  Glib::RefPtr<Gtk::FileChooserNative> chooser;
  OverlayDialog *leave_dialog = nullptr; // Responses dispose the legacy adapter.
  Impl(application::ProjectService &service, ObjectDocumentEditor &w,
       Gtk::Window &parent, const std::string &document)
      : owner(w), window(parent), display(parent.get_display()) {
    const auto *project = service.current();
    if (!project)
      throw std::runtime_error(
          "Abra um projeto e selecione um documento na Escrita.");
    const auto doc = service.writing().document(document);
    if (!doc)
      throw std::runtime_error("Selecione um documento na Escrita.");
    if (document.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQ"
                                   "RSTUVWXYZ0123456789-_") !=
        std::string::npos)
      throw std::runtime_error("Identidade de documento inválida.");
    project_path = project->path();
    const auto folder = project_path / "documents" / "composed";
    private_directory(folder);
    try {
      runtime->session = std::make_unique<objects::pilot::Session>(
          folder / document, project->manifest().project_id, *doc,
          objects::pilot::DocumentBinding::Live);
    } catch (const std::exception &e) {
      if (std::string(e.what()) == "native_document_source_changed")
        throw std::runtime_error(
            "O texto de origem mudou na Escrita. As duas versões foram "
            "preservadas; a reconciliação ainda precisa ser implementada.");
      throw;
    }
    state = runtime->session->request(O{{"op", "view"}}).at("state");
    selected = text(state, "selection");
    source_document = *doc;
    focus_follow = window.property_focus_widget().signal_changed().connect(
        [this] { follow_cursor(); });
    owner.set_orientation(Gtk::Orientation::VERTICAL);
    owner.append(root);
    root.set_vexpand(true);
    root.set_hexpand(true);
    object_menu.set_name("native-object-menu");
    object_menu.add_css_class("context-menu");
    object_menu.set_has_arrow(false);
    object_menu.set_position(Gtk::PositionType::BOTTOM);
    root.append(toolbar);
    tools_toggle = &button(toolbar, "Ferramentas", "native-tools", [this] { owner.toggle_tools(); });
    auto history_keys = Gtk::EventControllerKey::create();
    history_keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    history_keys->signal_key_pressed().connect(
        [this](guint key, guint, Gdk::ModifierType mods) {
          if (owner.text_composing && owner.text_composing())
            return false;
          if (focused_text())
            return false;
          if (busy || draft || choosing_image ||
              (mods & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType{})
            return false;
          const bool redo =
              (key == GDK_KEY_y || key == GDK_KEY_Y ||
               ((key == GDK_KEY_z || key == GDK_KEY_Z) &&
                (mods & Gdk::ModifierType::SHIFT_MASK) != Gdk::ModifierType{}));
          const bool undo = (key == GDK_KEY_z || key == GDK_KEY_Z) && !redo;
          if (!undo && !redo)
            return false;
          if (state.at(redo ? "can_redo" : "can_undo").as_boolean())
            command(redo ? "redo" : "undo");
          return true;
        },
        false);
    root.add_controller(history_keys);
    root.append(status);
    status.set_wrap(true);
    status.set_xalign(0);
    status.set_name("native-status");
    root.append(surface_row);
    surface_row.set_vexpand(true);
    side_scroll.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    side_scroll.set_child(sidebar);
    sidebar.set_margin(6);
    side_scroll.set_size_request(280, -1);
    side_revealer.set_child(side_scroll);
    side_revealer.set_transition_type(Gtk::RevealerTransitionType::SLIDE_LEFT);
    side_revealer.set_reveal_child(false);
    stack.set_hexpand(true);
    stack.set_vexpand(true);
    side_revealer.set_hexpand(false);
    side_scroll.set_name("native-tools-panel");
    surface_row.append(stack);
    surface_row.append(side_revealer);
    canvas.set_margin(20);
    canvas.set_name("native-sheet-content");
    content_scroll.set_child(canvas);
    if (auto *viewport = dynamic_cast<Gtk::Viewport *>(canvas.get_parent()))
      viewport->set_scroll_to_focus(false);
    content_scroll.set_name("native-sheet-scroll");
    content_scroll.set_policy(Gtk::PolicyType::AUTOMATIC,
                              Gtk::PolicyType::AUTOMATIC);
    stack.add(content_scroll, "document");
    editor.set_name("native-editor-surface");
    editor.set_halign(Gtk::Align::FILL);
    editor.set_hexpand(true);
    editor.set_size_request(280, -1);
    edit_scroll.set_name("native-edit-scroll");
    editor.set_margin(18);
    edit_scroll.set_child(editor);
    edit_scroll.set_vexpand(true);
    edit_page.append(edit_scroll);
    edit_actions.set_margin(12);
    edit_page.append(edit_actions);
    stack.add(edit_page, "editor");
    button(toolbar, "Salvar documento", "native-save", [this] {
      guard([this] {
        if (owner.save_document)
          owner.save_document();
        else
          command("save");
      });
    });
    button(toolbar, "Desfazer objeto", "native-undo",
           [this] { command("undo"); });
    button(toolbar, "Refazer objeto", "native-redo",
           [this] { command("redo"); });
    button(toolbar, "Retomar trabalho", "native-working",
           [this] { command("reopen_working", {{"discard_unsaved", true}}); });
    button(toolbar, "Reabrir salvo…", "native-reopen",
           [this] { confirm_reopen(); });
    limit_choice(objects);
    limit_choice(formulas);
    limit_choice(context);
    label(sidebar, "Ferramentas da especialidade");
    context.append("text", "Texto");
    context.append("structure", "Estrutura");
    context.append("composition", "Composição");
    context.set_visible(false);
    context.signal_changed().connect([this] {
      if (!rendering)
        command("set_context", {{"context", context.get_active_id().raw()}});
    });
    label(sidebar, "Objeto selecionado");
    objects.set_name("native-selection");
    sidebar.append(objects);
    objects.signal_changed().connect([this] {
      if (!rendering && !objects.get_active_id().empty()) {
        selected = objects.get_active_id();
        command("select", {{"id", selected}});
      }
    });
    auto group = [&](const std::string &id) -> Gtk::Box & {
      auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
      sidebar.append(*box);
      tool_groups[id] = box;
      return *box;
    };
    sidebar.append(specialty_host);
    auto &structure_tools = group("structure");
    label(structure_tools, "Ordem de leitura");
    label(structure_tools, "Arraste o círculo do bloco até a borda do vizinho. Texto e objetos usam a mesma sequência; blocos de texto indivisíveis são movidos juntos.");
    button(structure_tools, "Mover bloco acima na leitura", "native-reading-up", [this] { move_step(-1); });
    button(structure_tools, "Mover bloco abaixo na leitura", "native-reading-down", [this] { move_step(1); });
    button(structure_tools, "Escolher posição do bloco…", "native-reading-position", [this] { layout_editor(); });
    button(structure_tools, "Restaurar apresentação pela leitura", "native-reading-align", [this] { command("restore", {}, "composition"); });
    auto &math_tools = group("math");
    button(math_tools, "Inserir equação", "native-new-equation",
           [this] { equation_editor(); });
    auto &table_tools = group("tables");
    button(table_tools, "Inserir tabela", "native-new-table",
           [this] { table_editor(); });
    auto &composition_tools = group("composition");
    button(composition_tools, "Inserir figura PNG…", "native-new-image",
           [this] { pick_image(); });
    auto &formula_tools = group("formulas");
    button(formula_tools, "Inserir fórmula / cartão", "native-new-formula",
           [this] { formula_picker(); });
    label(formula_tools, "Biblioteca de fórmulas");
    formulas.set_name("native-formula-library");
    formula_tools.append(formulas);
    button(formula_tools, "Editar fórmula escolhida", "native-formula-edit",
           [this] {
             if (!formulas.get_active_id().empty())
               formula_editor(formulas.get_active_id());
           });
    button(formula_tools, "Variáveis e valores atuais", "native-variables",
           [this] { variables_editor(); });
    button(formula_tools, "Usar fórmula escolhida", "native-formula-attach",
           [this] { attach_formula(); });
    button(composition_tools, "Mover / colocar lado a lado", "native-layout",
           [this] { layout_editor(); });
    button(table_tools, "Aparência da tabela", "native-table-style",
           [this] { appearance_editor(); });
    button(table_tools, "Linhas e colunas", "native-table-structure",
           [this] { axes_editor(); });
    button(composition_tools, "Retirar selecionado", "native-remove",
           [this] { command("remove", {{"id", selected}}, "structure"); });
    button(composition_tools, "Restaurar itens retirados", "native-restore",
           [this] { removed_editor(); });
    button(composition_tools, "Restaurar apresentação pela leitura",
           "native-align-reading",
           [this] { command("restore", {}, "composition"); });
    Gtk::StyleContext::add_provider_for_display(
        window.get_display(), css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    for (auto &[id, box] : tool_groups)
      box->set_visible(id == specialty);
    render();
  }
  ObjectTextProjection *focused_text() const {
    // A real window close can destroy the native GtkWindow before the C++
    // workspace and its queued focus/layout callbacks are disposed.
    if (!window.gobj() || !GTK_IS_WINDOW(window.gobj()))
      return nullptr;
    const auto *focused = window.get_focus();
    if (!focused)
      return nullptr;
    for (const auto &[id, view] : paragraph_views)
      if (GTK_WIDGET(view->gobj()) == focused->gobj())
        return view;
    return nullptr;
  }
  void reveal_cursor(ObjectTextProjection &view) {
    graphene_rect_t bounds{};
    if (!gtk_widget_compute_bounds(GTK_WIDGET(view.gobj()),
                                   GTK_WIDGET(canvas.gobj()), &bounds))
      return;
    const auto buffer = view.get_buffer();
    Gdk::Rectangle caret;
    view.get_iter_location(buffer->get_iter_at_mark(buffer->get_insert()),
                           caret);
    const double y = static_cast<double>(bounds.origin.y) + caret.get_y();
    const auto adjustment = content_scroll.get_vadjustment();
    const double top = adjustment->get_value(),
                 page = adjustment->get_page_size();
    if (y < top + 12.)
      adjustment->set_value(std::max(0., y - 12.));
    else if (y + caret.get_height() > top + page - 12.)
      adjustment->set_value(
          std::min(std::max(0., adjustment->get_upper() - page),
                   y + caret.get_height() - page + 12.));
  }
  void follow_cursor() {
    if (!focused_text())
      return;
    if (cursor_follow.connected())
      return;
    const auto lifetime = life;
    // Coalesce layout/focus notifications. A short main-loop timer also runs
    // when GTK still has layout idles pending for a growing text viewport.
    cursor_follow = Glib::signal_timeout().connect(
        [this, lifetime] {
          if (!lifetime->alive)
            return false;
          if (auto *focused = focused_text()) {
            // Layout notifications can run inside a frame-clock phase. Do not
            // cancel/restart adjustment animations reentrantly from that phase.
            if (auto *viewport =
                    dynamic_cast<Gtk::Viewport *>(canvas.get_parent()))
              viewport->set_scroll_to_focus(false);
            const auto adjustment = content_scroll.get_vadjustment();
            adjustment->set_value(adjustment->get_value());
            reveal_cursor(*focused);
          }
          return false;
        },
        1);
  }
  ~Impl() {
    life->alive = false;
    if (leave_dialog) {
      delete leave_dialog;
      leave_dialog = nullptr;
    }
    drag_scroll.disconnect();
    tool_follow.disconnect();
    cursor_follow.disconnect();
    focus_follow.disconnect();
    close_object_menu();
    chooser_response.disconnect();
    if (chooser)
      chooser->hide();
    Gtk::StyleContext::remove_provider_for_display(display, css);
  }
  void error(const std::string &code) {
    last_error = code;
    g_warning("INDE OBJ-01: operação recusada: %s", code.c_str());
    auto at = messages.find(code);
    status.set_text(at == messages.end()
                        ? "Não foi possível aplicar. Confira os campos e as "
                          "referências; o trabalho anterior foi preservado."
                        : at->second);
    status.add_css_class("error");
    status.set_tooltip_text(code);
  }
  void defer_action(std::function<void()> fn) {
    ++pending_actions;
    root.set_sensitive(false);
    if (owner.editing_changed)
      owner.editing_changed(true);
    const auto lifetime = life;
    Glib::signal_idle().connect_once([this, lifetime, fn] {
      if (!lifetime->alive)
        return;
      --pending_actions;
      root.set_sensitive(!busy && pending_actions == 0);
      if (owner.editing_changed)
        owner.editing_changed(busy || draft || choosing_image ||
                              pending_actions != 0);
      try {
        fn();
      } catch (const std::exception &e) {
        error(e.what());
      }
    });
  }
  void command(const std::string &op, O args = {},
               const std::string &required = {},
               std::function<void(const V &)> after = {}) {
    if (busy)
      return;
    if (std::any_of(paragraph_views.begin(), paragraph_views.end(),
                    [](const auto &p) { return p.second->pending_input(); })) {
      status.set_text("Aguarde a confirmação da entrada de texto.");
      return;
    }
    if (owner.text_composing && owner.text_composing()) {
      status.set_text(
          "Confirme a composição de texto antes de alterar objetos.");
      return;
    }
    if (draft && op != "preview_formula_hub" && op != "edit_cell" &&
        op != "edit_formula_text" && op != "create_equation" &&
        op != "edit_equation" && op != "edit_formula" &&
        op != "create_table_sized" && op != "create_formula_hub" &&
        op != "edit_formula_hub" && op != "edit_table_design" &&
        op != "import" && op != "move" && op != "set_presentation" &&
        op != "arrange_objects" && op != "stack" && op != "insert_table_axis" &&
        op != "remove_table_axis" && op != "restore_table_axis" &&
        op != "restore_removed" && op != "reset_presentation" &&
        op != "reopen") {
      status.set_text("Aplique ou cancele a edição aberta antes de continuar.");
      return;
    }
    args.emplace("op", op);
    args.emplace("expected", state.at("revision"));
    busy = true;
    if (owner.editing_changed)
      owner.editing_changed(true);
    last_error.clear();
    root.set_sensitive(false);
    auto *work = new Work{
        runtime,
        life,
        V(std::move(args)),
        V{},
        required,
        [this, op, after](const V &result) {
          busy = false;
          if (owner.editing_changed)
            owner.editing_changed(draft || choosing_image);
          root.set_sensitive(true);
          if (has(result, "state"))
            state = result.at("state");
          if (!result.at("accepted").as_boolean()) {
            restore_handle.clear();
            error(text(result, "error"));
            return;
          }
          if (op != "preview_formula_hub" && owner.commit_document) {
            try {
              std::lock_guard lock(runtime->mutex);
              const auto now = runtime->session->current_document();
              if (!project::same_document_state(source_document, now, false)) {
                if (op == "move" || op == "undo" || op == "redo")
                  source_document = owner.commit_document(now);
                runtime->session->synchronize_document(source_document);
                state =
                    runtime->session->request(O{{"op", "view"}}).at("state");
              }
            } catch (const std::exception &e) {
              restore_handle.clear();
              error(e.what());
              return;
            }
          }
          if (after)
            after(result);
          if (op != "preview_formula_hub")
            render();
        }};
    auto *task = g_task_new(
        nullptr, nullptr,
        +[](GObject *, GAsyncResult *result, gpointer) {
          auto *work =
              static_cast<Work *>(g_task_get_task_data(G_TASK(result)));
          if (work->life->alive)
            work->done(work->result);
        },
        nullptr);
    g_task_set_task_data(
        task, work, +[](gpointer p) { delete static_cast<Work *>(p); });
    g_task_run_in_thread(
        task, +[](GTask *t, gpointer, gpointer data, GCancellable *) {
          auto &work = *static_cast<Work *>(data);
          try {
            std::lock_guard lock(work.runtime->mutex);
            if (!work.context.empty()) {
              const auto view =
                  work.runtime->session->request(O{{"op", "view"}}).at("state");
              if (text(view, "context") != work.context) {
                auto changed = work.runtime->session->request(
                    O{{"op", "set_context"},
                      {"expected", view.at("revision")},
                      {"context", work.context}});
                if (!changed.at("accepted").as_boolean()) {
                  work.result = changed;
                  g_task_return_boolean(t, TRUE);
                  return;
                }
                auto input = work.input.as_object();
                input["expected"] = changed.at("state").at("revision");
                work.input = std::move(input);
              }
            }
            work.result = work.runtime->session->request(work.input);
          } catch (const std::exception &e) {
            work.result = O{{"accepted", false}, {"error", e.what()}};
          }
          g_task_return_boolean(t, TRUE);
        });
    g_object_unref(task);
  }
  std::string title(const std::string &id) const {
    for (const auto *kind :
         {"paragraphs", "figures", "tables", "equations", "formula_uses"})
      if (const auto *v = find(state, kind, id)) {
        if (std::string(kind) == "paragraphs")
          return Glib::ustring(text(*v, "text")).substr(0, 48);
        if (std::string(kind) == "figures")
          return "Figura: " + text(*v, "caption");
        if (std::string(kind) == "tables")
          return "Tabela: " + text(*v, "title");
        if (std::string(kind) == "equations")
          return "Equação: " + text(*v, "label");
        if (const auto *f = find(state, "formulas", text(*v, "object")))
          return "Fórmula: " + text(*f, "name");
      }
    return id;
  }
  void destinations(Gtk::ComboBoxText &c, const std::string &active = "") {
    c.append("", "Fim do documento");
    for (const auto &id : state.at("visual_order").as_array())
      if (shown(id.as_string()))
        c.append(id.as_string(), title(id.as_string()));
    c.set_active_id(active);
  }
  std::vector<std::string> reading_unit(const std::string &id) const {
    for(const auto &g:state.at("reading_groups").as_array()) {
      std::vector<std::string> ids;
      bool found=false;
      for(const auto &v:g.as_array()) {ids.push_back(v.as_string());found=found || v.as_string()==id;}
      if(found)return ids;
    }
    return {id};
  }
  bool leading_handle(const std::string &id) const {
    for(const auto &member:reading_unit(id))if(shown(member))return member==id;
    return false;
  }
  void move_relative(const std::string &id,const std::string &reference,bool after,bool keep_focus=false) {
    if(!owner.can_leave()) {status.set_text("Conclua a edição em andamento antes de mover blocos.");return;}
    const auto &active=state.at("author_order").as_array();
    if(std::none_of(active.begin(),active.end(),[&](const V &v){return v.as_string()==id;}) ||
       std::none_of(active.begin(),active.end(),[&](const V &v){return v.as_string()==reference;})) {
      error("stale_reading_drag");return;
    }
    const auto unit=reading_unit(id);
    auto destination=reading_unit(reference);
    // A row is one drop destination: never insert between its two children.
    for(const auto &row:state.at("rows").as_array()) {
      const auto &children=row.at("children").as_array();
      if(std::any_of(children.begin(),children.end(),[&](const V &v){return std::find(destination.begin(),destination.end(),v.as_string())!=destination.end();}))
        for(const auto &v:children)if(std::find(destination.begin(),destination.end(),v.as_string())==destination.end())destination.push_back(v.as_string());
    }
    if(std::any_of(destination.begin(),destination.end(),[&](const auto &v){return std::find(unit.begin(),unit.end(),v)!=unit.end();}))return;
    const auto &order=state.at("author_order").as_array();
    std::string before;
    bool passed=false;
    for(const auto &v:order) {
      const auto key=v.as_string();
      const bool member=std::find(destination.begin(),destination.end(),key)!=destination.end();
      if(!after && member) {before=key;break;}
      if(member)passed=true;
      else if(after && passed && std::find(unit.begin(),unit.end(),key)==unit.end()) {before=key;break;}
    }
    if(keep_focus)restore_handle=id;
    command("move",{{"id",id},{"before",before},{"intent","authorial"}},"structure");
  }
  void move_step(int direction,bool keep_focus=false) {
    if(selected.empty()) {status.set_text("Escolha um bloco na folha antes de mover.");return;}
    const auto unit=reading_unit(selected);
    const auto &order=state.at("author_order").as_array();
    std::size_t first=order.size(),last=0;
    for(std::size_t i=0;i<order.size();++i)
      if(std::find(unit.begin(),unit.end(),order[i].as_string())!=unit.end()) {first=std::min(first,i);last=i;}
    if(first==order.size())return;
    if(direction<0) {
      for(std::size_t i=first;i>0;--i)if(shown(order[i-1].as_string())) {move_relative(selected,order[i-1].as_string(),false,keep_focus);return;}
    } else {
      for(std::size_t i=last+1;i<order.size();++i)if(shown(order[i].as_string())) {move_relative(selected,order[i].as_string(),true,keep_focus);return;}
    }
    status.set_text("O bloco já está no limite da leitura visível.");
  }
  void cancel_drag() {
    dragging.clear();drag_reference.clear();drag_scroll.disconnect();
    for(auto &[id,w]:cards) {w->remove_css_class("reading-drop-before");w->remove_css_class("reading-drop-after");}
  }
  void update_drag(double canvas_y) {
    const auto unit=reading_unit(dragging);
    drag_reference.clear();
    double closest=1e20;
    for(const auto &v:state.at("visual_order").as_array()) {
      const auto id=v.as_string();
      if(!cards.contains(id) || std::find(unit.begin(),unit.end(),id)!=unit.end())continue;
      auto *w=cards.at(id);
      w->remove_css_class("reading-drop-before");w->remove_css_class("reading-drop-after");
      graphene_rect_t b{};
      if(!gtk_widget_compute_bounds(w->gobj(),GTK_WIDGET(canvas.gobj()),&b))continue;
      const double middle=b.origin.y+b.size.height/2.;
      const double distance=std::abs(canvas_y-middle);
      if(distance<closest) {closest=distance;drag_reference=id;drag_after=canvas_y>middle;}
    }
    if(!drag_reference.empty())cards.at(drag_reference)->add_css_class(drag_after?"reading-drop-after":"reading-drop-before");
  }
  void render() {
    render_pending = true;
    if (std::any_of(paragraph_views.begin(), paragraph_views.end(),
                    [](const auto &p) { return p.second->pending_input(); }))
      return;
    if (draft || (owner.text_composing && owner.text_composing()))
      return;
    const auto scroll_x = content_scroll.get_hadjustment()->get_value();
    const auto scroll_y = content_scroll.get_vadjustment()->get_value();
    auto *focused = focused_text();
    std::string focused_paragraph, focused_handle;
    if(window.gobj() && GTK_IS_WINDOW(window.gobj()))
      if(auto *focus=window.get_focus())
        for(const auto &[id,handle]:reading_handles)
          if(focus==handle || gtk_widget_is_ancestor(focus->gobj(),handle->gobj()))focused_handle=id;
    for (const auto &[id, view] : paragraph_views)
      if (view == focused)
        focused_paragraph = id;
    rendering = true;
    selected = text(state, "selection");
    if (!selected.empty() && !shown(selected))
      selected.clear();
    context.set_active_id(text(state, "context"));
    objects.remove_all();
    for (const auto &id : state.at("visual_order").as_array())
      if (shown(id.as_string()))
        objects.append(id.as_string(), title(id.as_string()));
    objects.set_active_id(selected);
    const auto old = formulas.get_active_id();
    formulas.remove_all();
    for (const auto &f : state.at("formulas").as_array())
      if (!f.at("consumed").as_boolean())
        formulas.append(text(f, "id"),
                        text(f, "name") + " · " + text(f, "display"));
    if (!formulas.set_active_id(old))
      formulas.set_active(0);
    rendering = false;
    side_scroll.get_hadjustment()->set_value(0);
    close_object_menu();
    menus.clear();
    menu_updates.clear();
    cards.clear();
    reading_handles.clear();
    dragging.clear(); drag_scroll.disconnect();
    paragraph_views.clear();
    clear(canvas);

    std::string styles =
        ".inde-native-item { padding: 8px; border: 1px solid "
        "transparent; border-radius: 8px; } .inde-native-selected "
        "{ border-color: #3b806b; } .inde-native-text { font-family: serif; "
        "font-size: 18px; } .reading-drop-before { border-top: 3px solid #3b806b; } .reading-drop-after { border-bottom: 3px solid #3b806b; }";
    std::set<std::string> paired;
    for (const auto &id : state.at("visual_order").as_array()) {
      const auto key = id.as_string();
      if (!shown(key) || paired.contains(key))
        continue;
      const V *row = nullptr;
      for (const auto &r : state.at("rows").as_array())
        if (r.at("children").as_array()[0].as_string() == key) {
          row = &r;
          break;
        }
      if (row) {
        std::vector<Gtk::Widget *> children;
        const auto &pair = row->at("children").as_array();
        if (!shown(pair[0].as_string()) || !shown(pair[1].as_string())) {
          canvas.append(*item(key, styles));
          continue;
        }
        for (const auto &c : pair) {
          paired.insert(c.as_string());
          children.push_back(item(c.as_string(), styles));
        }
        auto *view = Gtk::make_managed<FractionRow>(
            children,
            std::vector<double>{real(row->at("first")),
                                real(row->at("second"))},
            static_cast<int>(real(row->at("gap"))));
        canvas.append(*view);
      } else
        canvas.append(*item(key, styles));
    }
    css->load_from_data(styles);
    status.remove_css_class("error");
    status.set_text(
        text(state, "persistence_status") == "uncertain"
            ? "Gravação incerta: reabra o trabalho antes de editar."
        : state.at("dirty").as_boolean()
            ? "Trabalho protegido para recuperação · ainda não salvo"
            : "Salvo e conferido");
    stack.set_visible_child("document");
    content_scroll.get_hadjustment()->set_value(scroll_x);
    content_scroll.get_vadjustment()->set_value(scroll_y);
    if (!focused_paragraph.empty())
      owner.focus_text(-1);
    if(!restore_handle.empty()) {focused_handle=restore_handle;restore_handle.clear();}
    if(!focused_handle.empty() && reading_handles.contains(focused_handle) && reading_handles.at(focused_handle)->get_visible())
      reading_handles.at(focused_handle)->grab_focus();
    render_pending = false;
  }
  void format_paragraph(Gtk::Label &l, const std::string &paragraph) {
    if (visible_text)
      return; // A filtered interval has a different offset map.
    const auto row =
        std::find_if(source_document.paragraphs.rows.begin(),
                     source_document.paragraphs.rows.end(),
                     [&](const auto &r) { return r.id == paragraph; });
    if (row == source_document.paragraphs.rows.end())
      return;
    const auto body = l.get_text().raw();
    auto *attrs = pango_attr_list_new();
    using Style = project::DocumentTextStyle;
    for (const auto &f : source_document.formatting) {
      const auto begin = std::max(row->start, f.start_offset),
                 end = std::min(row->end, f.end_offset);
      if (begin >= end)
        continue;
      PangoAttribute *a = nullptr;
      switch (f.style) {
      case Style::Bold:
        a = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
        break;
      case Style::Italic:
      case Style::Quote:
        a = pango_attr_style_new(PANGO_STYLE_ITALIC);
        break;
      case Style::Underline:
        a = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
        break;
      case Style::Strikethrough:
        a = pango_attr_strikethrough_new(true);
        break;
      case Style::FontFamily:
        a = pango_attr_family_new(f.value.c_str());
        break;
      case Style::FontSize:
        try {
          a = pango_attr_size_new(static_cast<int>(
              std::clamp(std::stod(f.value), 6., 160.) * PANGO_SCALE));
        } catch (const std::exception &) {
        }
        break;
      case Style::Foreground: {
        GdkRGBA c{};
        if (gdk_rgba_parse(&c, f.value.c_str()))
          a = pango_attr_foreground_new(static_cast<guint16>(c.red * 65535),
                                        static_cast<guint16>(c.green * 65535),
                                        static_cast<guint16>(c.blue * 65535));
        break;
      }
      case Style::AlignLeft:
        l.set_xalign(0);
        break;
      case Style::AlignCenter:
        l.set_xalign(.5f);
        break;
      case Style::AlignRight:
        l.set_xalign(1);
        break;
      case Style::AlignJustify:
        l.set_justify(Gtk::Justification::FILL);
        break;
      case Style::Heading:
      case Style::Subheading:
      case Style::Heading2:
      case Style::Heading3:
      case Style::Heading4:
      case Style::Heading5:
      case Style::Heading6:
        a = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
        break;
      }
      if (a) {
        a->start_index = static_cast<guint>(
            g_utf8_offset_to_pointer(body.c_str(),
                                     static_cast<glong>(begin - row->start)) -
            body.c_str());
        a->end_index = static_cast<guint>(
            g_utf8_offset_to_pointer(body.c_str(),
                                     static_cast<glong>(end - row->start)) -
            body.c_str());
        pango_attr_list_insert(attrs, a);
      }
    }
    gtk_label_set_attributes(l.gobj(), attrs);
    pango_attr_list_unref(attrs);
  }
  Gtk::Widget *item(const std::string &id, std::string &styles) {
    auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    box->add_css_class("inde-native-item");
    if (id == selected)
      box->add_css_class("inde-native-selected");
    box->set_name("native-object-" + id);
    auto *reading_row=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL,6);
    reading_row->set_hexpand(true);
    cards[id] = reading_row;
    box->set_focusable(true);
    auto focus = Gtk::EventControllerFocus::create();
    focus->signal_enter().connect([this, id] {
      if(!restoring_text_focus) {
        selected = id;
        rendering = true; objects.set_active_id(id); rendering = false;
      }
      if (auto *viewport = dynamic_cast<Gtk::Viewport *>(canvas.get_parent()))
        viewport->set_scroll_to_focus(!focused_text());
      follow_cursor();
    });
    box->add_controller(focus);
    box->set_tooltip_text("Clique direito para editar, mover ou retirar. "
                          "Shift+F10 pelo teclado.");
    auto *handle = Gtk::make_managed<Gtk::Button>("●");
    handle->set_name("native-reading-handle-" + id);
    set_accessible_label(*handle, "Reordenar bloco · " + title(id));
    handle->set_halign(Gtk::Align::START);
    handle->set_tooltip_text("Arraste para reordenar a leitura. Ctrl+↑/↓ move o bloco; Escape cancela.");
    handle->set_cursor("grab");
    handle->set_valign(Gtk::Align::START);
    handle->set_margin_top(8);
    reading_row->append(*handle);
    reading_row->append(*box);
    reading_handles[id] = handle;
    handle->set_visible(specialty == "structure" && leading_handle(id));
    auto drag = Gtk::GestureDrag::create();
    drag->set_button(1);
    drag->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    drag->signal_drag_begin().connect([this, id, handle](double x, double y) {
      if (!owner.can_leave() || specialty != "structure") return;
      selected = id; dragging = id; drag_reference.clear();
      drag_revision = state.at("revision").as_integer();
      graphene_point_t a{static_cast<float>(x), static_cast<float>(y)}, b{};
      if (!gtk_widget_compute_point(GTK_WIDGET(handle->gobj()), GTK_WIDGET(canvas.gobj()), &a, &b)) {dragging.clear();return;}
      drag_y = b.y; drag_delta = 0;
      drag_initial_scroll = content_scroll.get_vadjustment()->get_value();
      handle->grab_focus();
      drag_scroll.disconnect();
      drag_scroll = Glib::signal_timeout().connect([this] {
        if(dragging.empty())return false;
        const auto a=content_scroll.get_vadjustment();
        const double pointer=drag_y+drag_delta-drag_initial_scroll;
        const double step=pointer<26?-18:pointer>a->get_page_size()-26?18:0;
        if(step)a->set_value(std::clamp(a->get_value()+step,a->get_lower(),std::max(a->get_lower(),a->get_upper()-a->get_page_size())));
        update_drag(drag_y+drag_delta+a->get_value()-drag_initial_scroll);
        return true;
      },60);
    });
    drag->signal_drag_update().connect([this](double, double y) {
      if(dragging.empty())return;
      drag_delta=y;
      update_drag(drag_y + y + content_scroll.get_vadjustment()->get_value()-drag_initial_scroll);
    });
    drag->signal_drag_end().connect([this](double x, double y) {
      const auto id = dragging, reference = drag_reference;
      const auto revision = drag_revision;
      const bool after = drag_after;
      cancel_drag();
      if(!id.empty() && !reference.empty() && std::hypot(x,y)>=6)
        defer_action([this,id,reference,after,revision] {
          if(state.at("revision").as_integer()!=revision) {error("stale_reading_drag");return;}
          move_relative(id,reference,after,true);
        });
    });
    drag->signal_cancel().connect([this](Gdk::EventSequence *) {cancel_drag();});
    handle->add_controller(drag);
    auto handle_keys = Gtk::EventControllerKey::create();
    handle_keys->signal_key_pressed().connect([this,id](guint key,guint,Gdk::ModifierType mods) {
      if(key==GDK_KEY_Escape) {cancel_drag();return true;}
      if((mods & Gdk::ModifierType::CONTROL_MASK)!=Gdk::ModifierType{} && (key==GDK_KEY_Up || key==GDK_KEY_Down)) {
        selected=id;move_step(key==GDK_KEY_Up?-1:1,true);return true;
      }
      return false;
    },false);
    handle->add_controller(handle_keys);
    auto *actions = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    menus[id] = actions;
    // Menu content is parented in an invisible holder until requested.
    actions->set_visible(false);
    box->append(*actions);
    const auto show_menu = [this, box, actions, id](double x, double y, bool pointer) {
      if (!owner.can_leave())
        return;
      if(pointer) {
        if(const auto *p=find(state,"paragraphs",id)) {
          const auto at=paragraph_views.find(text(*p,"paragraph"));
          if(at!=paragraph_views.end()) {
            graphene_point_t a{static_cast<float>(x),static_cast<float>(y)},b{};
            if(gtk_widget_compute_point(GTK_WIDGET(box->gobj()),GTK_WIDGET(at->second->gobj()),&a,&b))
              at->second->prepare_context(b.x,b.y);
          }
        }
      }
      Gdk::Rectangle anchor(static_cast<int>(x),static_cast<int>(y),1,1);
      if(const auto *p=find(state,"paragraphs",id)) {
        const auto at=paragraph_views.find(text(*p,"paragraph"));
        if(at!=paragraph_views.end()) {
          auto *view=at->second;
          Gtk::TextIter a,b;
          if(view->get_buffer()->get_selection_bounds(a,b)) {
            b.backward_char();
            Gdk::Rectangle rect;
            view->get_iter_location(b,rect);
            int wx{},wy{};
            view->buffer_to_window_coords(Gtk::TextWindowType::WIDGET,rect.get_x(),rect.get_y()+rect.get_height()+4,wx,wy);
            graphene_point_t from{static_cast<float>(wx),static_cast<float>(wy)},to{};
            if(gtk_widget_compute_point(GTK_WIDGET(view->gobj()),GTK_WIDGET(box->gobj()),&from,&to))
              anchor=Gdk::Rectangle(static_cast<int>(to.x),static_cast<int>(to.y),1,1);
          }
        }
      }
      selected = id;
      if(menu_updates.contains(id))menu_updates.at(id)();
      rendering = true;
      objects.set_active_id(id);
      rendering = false;
      close_object_menu();
      menu_holder = box;
      g_object_ref(actions->gobj());
      if (auto *parent = dynamic_cast<Gtk::Box *>(actions->get_parent()))
        parent->remove(*actions);
      auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
      scroll->set_policy(Gtk::PolicyType::NEVER,Gtk::PolicyType::AUTOMATIC);
      scroll->set_max_content_height(360);
      scroll->set_min_content_width(280);
      // With horizontal scrolling disabled GTK measures the child minimum;
      // wrapped captions still need an explicit readable widget width.
      scroll->set_size_request(280,-1);
      scroll->set_propagate_natural_height(true);
      scroll->set_child(*actions);
      scroll->set_name("native-context-scroll");
      object_menu.set_child(*scroll);
      g_object_unref(actions->gobj());
      actions->set_visible(true);
      object_menu.set_parent(*box);
      object_menu.set_pointing_to(anchor);
      object_menu.popup();
    };
    auto click = Gtk::GestureClick::create();
    click->set_button(3);
    click->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    click->signal_pressed().connect([show_menu, gesture = click->gobj()](
                                        int, double x, double y) {
      show_menu(x, y, true);
      if (gtk_gesture_is_recognized(GTK_GESTURE(gesture)))
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    });
    box->add_controller(click);
    auto keys = Gtk::EventControllerKey::create();
    keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    keys->signal_key_pressed().connect(
        [show_menu](guint key, guint, Gdk::ModifierType mods) {
          if (key == GDK_KEY_Menu ||
              (key == GDK_KEY_F10 &&
               (mods & Gdk::ModifierType::SHIFT_MASK) != Gdk::ModifierType{})) {
            show_menu(8, 8, false);
            return true;
          }
          return false;
        },
        false);
    box->add_controller(keys);
    const auto action = [this, actions](const std::string &caption,
                                        const std::string &name,
                                        std::function<void()> fn) {
      button(*actions, caption, name, [this, fn] {
        object_menu.popdown();
        defer_action(fn);
      });
    };
    std::vector<std::function<void()>> update;
    std::function<void()> prepare_legacy;
    if(find(state,"paragraphs",id) && owner.paragraph_actions) {
      auto *legacy=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,2);
      actions->append(*legacy);
      auto legacy_updates=std::make_shared<std::vector<std::function<void()>>>();
      prepare_legacy=[this,legacy,id,legacy_updates] {
        if(!legacy->get_first_child()) {
      std::size_t index=0;
      for(const auto &a:owner.paragraph_actions()) {
        Gtk::Separator *separator=nullptr;
        if(a.separator_before) {separator=Gtk::make_managed<Gtk::Separator>();legacy->append(*separator);}
        auto &b=button(*legacy,a.label.raw(),"native-writing-action-"+std::to_string(index++)+"-"+id,[this,a] {
          object_menu.popdown();
          defer_action([a] {if(a.enabled && (!a.visible || a.visible()) && (!a.available || a.available()) && a.activate)a.activate();});
        });
        b.add_css_class("context-menu-item");
        set_accessible_label(b,a.label);
        if (!a.icon_name.empty()) {
          auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 9);
          auto *icon = Gtk::make_managed<Gtk::Image>();
          icon->set_from_icon_name(a.icon_name);
          row->append(*icon);
          auto *caption = Gtk::make_managed<Gtk::Label>(a.label);
          caption->set_xalign(0);
          caption->set_wrap(true);
          caption->set_max_width_chars(30);
          row->append(*caption);
          b.set_child(*row);
        }
        legacy_updates->push_back([&b,separator,a] {
          const bool visible=!a.visible || a.visible();
          b.set_visible(visible); if(separator)separator->set_visible(visible);
          b.set_sensitive(a.enabled && (!a.available || a.available()));
        });
      }
        }
        for(const auto &refresh:*legacy_updates)refresh();
      };
      actions->append(*Gtk::make_managed<Gtk::Separator>());
    }
    for(const auto direction:{-1,1}) {
      auto &b=button(*actions,direction<0?"Mover bloco acima na leitura":"Mover bloco abaixo na leitura",
        "native-context-reading-"+std::string(direction<0?"up-":"down-")+id,[this,id,direction] {
          selected=id;object_menu.popdown();defer_action([this,direction]{move_step(direction);});
        });
      update.push_back([this,&b]{b.set_visible(specialty=="structure");});
    }
    menu_updates[id]=[update,prepare_legacy]{if(prepare_legacy)prepare_legacy();for(const auto &fn:update)fn();};
    action("Posição e alinhamento…", "native-object-layout-" + id,
           [this] { layout_editor(); });
    action("Retirar objeto", "native-object-remove-" + id,
           [this, id] { command("remove", {{"id", id}}, "structure"); });
    if (const auto *p = find(state, "paragraphs", id)) {
      const auto paragraph = text(*p, "paragraph");
      action("Escrever neste parágrafo", "native-paragraph-edit-" + id,
             [this, paragraph] {
               if (owner.edit_paragraph)
                 owner.edit_paragraph(paragraph);
             });
      if (owner.paragraph_view) {
        auto *view = owner.paragraph_view(paragraph);
        if (!view)
          throw std::runtime_error("Parágrafo sem origem na Escrita.");
        if (auto *projection = dynamic_cast<ObjectTextProjection *>(view)) {
          projection->layout_changed = [this, lifetime = life, projection] {
            if (lifetime->alive && focused_text() == projection)
              follow_cursor();
          };
          // TextView is GtkScrollable: a scrolled host propagates its validated
          // content height, including wrapping, instead of allocating zero
          // height before the first focus/layout pass.
          auto *host = Gtk::make_managed<Gtk::ScrolledWindow>();
          host->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::NEVER);
          host->set_propagate_natural_height(true);
          host->set_min_content_height(32);
          host->set_hexpand(true);
          host->set_child(*projection);
          box->append(*host);
          paragraph_views[paragraph] = projection;
        } else
          box->append(*view);
      } else {
        auto &l = label(*box, visible_text ? visible_text->at(paragraph)
                                           : text(*p, "text"));
        l.set_selectable(true);
        l.add_css_class("inde-native-text");
        format_paragraph(l, paragraph);
      }
    } else if (const auto *e = find(state, "equations", id)) {
      auto *m = Gtk::make_managed<MathPreview>();
      m->set_name("native-math-" + id);
      m->set_source(text(*e, "source"), text(*e, "mode") == "inline");
      box->append(*m);
      if (!m->error().empty())
        label(*box, m->error()).add_css_class("error");
      action("Editar equação", "native-equation-edit-" + id,
             [this, id] { equation_editor(id); });
    } else if (const auto *t = find(state, "tables", id)) {
      table_view(*box, *t, styles);
      action("Aparência da tabela…", "native-object-table-style-" + id,
             [this] { appearance_editor(); });
      action("Linhas e colunas…", "native-object-table-axes-" + id,
             [this] { axes_editor(); });
    } else if (const auto *u = find(state, "formula_uses", id))
      formula_view(*box, text(*u, "object"), id, actions);
    else if (const auto *f = find(state, "figures", id)) {
      const auto resource = text(*f, "resource");
      try {
        if (!images.contains(resource)) {
          std::lock_guard lock(runtime->mutex);
          const auto r = runtime->session->request(
              O{{"op", "image"}, {"resource", resource}});
          const auto data = Glib::Base64::decode(text(r, "data"));
          auto bytes = Glib::Bytes::create(data.data(), data.size());
          images[resource] = Gdk::Texture::create_from_bytes(bytes);
        }
        auto *picture = Gtk::make_managed<Gtk::Picture>(images.at(resource));
        picture->set_can_shrink(true);
        picture->set_content_fit(Gtk::ContentFit::CONTAIN);
        picture->set_size_request(-1, 180);
        picture->set_alternative_text(text(*f, "alt"));
        box->append(*picture);
        label(*box, text(*f, "caption"));
      } catch (const std::exception &e) {
        label(*box, e.what());
      }
      action("Duplicar figura", "native-duplicate-" + id, [this, id] {
        command("duplicate", {{"id", id}, {"before", ""}}, "structure");
      });
    }
    if (state.at("presentation").as_object().contains(id)) {
      const auto &p = state.at("presentation").at(id);
      const auto horizontal = text(p, "horizontal");
      box->set_halign(horizontal == "end"      ? Gtk::Align::END
                      : horizontal == "center" ? Gtk::Align::CENTER
                                               : Gtk::Align::START);
      box->set_halign(Gtk::Align::FILL);
      const auto vertical = text(p, "vertical");
      box->set_valign(vertical == "end"      ? Gtk::Align::END
                      : vertical == "center" ? Gtk::Align::CENTER
                                             : Gtk::Align::START);
      const auto ta = text(p, "text_align");
      for (auto *w = box->get_first_child(); w; w = w->get_next_sibling())
        if (auto *l = dynamic_cast<Gtk::Label *>(w)) {
          l->set_xalign(ta == "center" ? .5f : ta == "right" ? 1.f : 0.f);
          l->set_justify(ta == "center"    ? Gtk::Justification::CENTER
                         : ta == "right"   ? Gtk::Justification::RIGHT
                         : ta == "justify" ? Gtk::Justification::FILL
                                           : Gtk::Justification::LEFT);
        } else if (auto *v = dynamic_cast<Gtk::TextView *>(w)) {
          v->set_justification(ta == "center"    ? Gtk::Justification::CENTER
                               : ta == "right"   ? Gtk::Justification::RIGHT
                               : ta == "justify" ? Gtk::Justification::FILL
                                                 : Gtk::Justification::LEFT);
        }
      const double fraction = real(p.at("width")) / 100.;
      if (fraction < .999 && horizontal == "center") {
        auto *left = Gtk::make_managed<Gtk::Box>();
        auto *right = Gtk::make_managed<Gtk::Box>();
        return Gtk::make_managed<FractionRow>(
            std::vector<Gtk::Widget *>{left, reading_row, right},
            std::vector<double>{(1 - fraction) / 2, fraction,
                                (1 - fraction) / 2},
            0);
      }
      if (fraction < .999) {
        auto *space = Gtk::make_managed<Gtk::Box>();
        auto *slot = Gtk::make_managed<FractionRow>(
            horizontal == "end" ? std::vector<Gtk::Widget *>{space, reading_row}
                                : std::vector<Gtk::Widget *>{reading_row, space},
            horizontal == "end" ? std::vector<double>{1 - fraction, fraction}
                                : std::vector<double>{fraction, 1 - fraction},
            0);
        return slot;
      }
    }
    box->set_hexpand(true);
    return reading_row;
  }
  void formula_view(Gtk::Box &box, const std::string &id,
                    const std::string &target, Gtk::Box *actions = nullptr) {
    const auto *f = find(state, "formulas", id);
    if (!f)
      return;
    const auto control = [this, actions, &box](const std::string &caption,
                                               const std::string &name,
                                               std::function<void()> fn) {
      button(actions ? *actions : box, caption, name, [this, actions, fn] {
        if (!actions) {
          fn();
          return;
        }
        object_menu.popdown();
        defer_action(fn);
      });
    };
    if (f->at("command").as_boolean()) {
      label(box, "Alteração de uso único · " + text(*f, "name"));
      control("Aplicar alteração e retirar comando", "native-execute-" + id,
              [this, id] { command("execute_formula", {{"id", id}}, "text"); });
    } else {
      std::string out;
      if (state.at("formula_text").as_object().contains(target)) {
        for (const auto &r :
             state.at("formula_text").at(target).at("runs").as_array())
          if (text(r, "kind") == "text")
            out += text(r, "text");
          else if (const auto *value =
                       find(state, "formulas", text(r, "formula")))
            out += value->at("valid").as_boolean() ? text(*value, "display")
                                                   : "[conferir fórmula]";
      } else
        out = text(*f, "display");
      auto &l = label(box, out);
      l.set_selectable(true);
      l.add_css_class("inde-native-text");
      control("Editar frase ao redor do resultado", "native-phrase-" + target,
              [this, target] { phrase_editor(target); });
    }
    control("Editar fórmula", "native-edit-formula-" + id,
            [this, id, target] { formula_editor(id, target); });
    if (!f->at("valid").as_boolean())
      label(box, "Confira a fonte e as referências da fórmula.")
          .add_css_class("error");
  }
  void table_view(Gtk::Box &box, const V &t, std::string &styles) {
    const auto id = text(t, "id");
    const auto &a = t.at("appearance");
    auto caption = [&] {
      if (a.at("show_title").as_boolean()) {
        auto &l = label(box, text(t, "title"));
        l.set_xalign(text(a, "caption_align") == "right"    ? 1.f
                     : text(a, "caption_align") == "center" ? .5f
                                                            : 0.f);
      }
    };
    if (text(a, "caption_side") == "top")
      caption();
    const auto &rows = t.at("row_ids").as_array(),
               &cols = t.at("column_ids").as_array();
    std::vector<double> weights;
    for (const auto &w : a.at("column_weights").as_array())
      weights.push_back(real(w) / 100.);
    if (weights.empty())
      weights.assign(cols.size(), 1. / static_cast<double>(cols.size()));
    std::size_t ri = 0;
    for (const auto &r : rows) {
      std::vector<Gtk::Widget *> cells;
      for (const auto &c : cols) {
        const V *cell = nullptr;
        for (const auto &v : t.at("cells").as_array())
          if (text(v, "row") == r.as_string() &&
              text(v, "column") == c.as_string()) {
            cell = &v;
            break;
          }
        if (!cell)
          continue;
        const auto cid = text(*cell, "id");
        auto *b = Gtk::make_managed<Gtk::Button>();
        b->set_name("native-cell-" + cid);
        auto *l = Gtk::make_managed<Gtk::Label>();
        l->set_wrap(true);
        l->set_selectable(false);
        l->set_xalign(0);
        l->set_hexpand(true);
        std::string value = text(*cell, "text");
        if (state.at("formula_cells").as_object().contains(cid)) {
          const auto fid = state.at("formula_cells").at(cid).as_string();
          if (const auto *f = find(state, "formulas", fid))
            value = text(*f, "display");
          if (state.at("formula_text").as_object().contains(cid)) {
            value.clear();
            for (const auto &run :
                 state.at("formula_text").at(cid).at("runs").as_array())
              if (text(run, "kind") == "text")
                value += text(run, "text");
              else if (const auto *f =
                           find(state, "formulas", text(run, "formula")))
                value += text(*f, "display");
          }
        }
        l->set_text(value.empty() ? " " : value);
        b->set_child(*l);
        b->set_hexpand(true);
        b->signal_clicked().connect([this, id, cid] {
          command("select_cell", {{"id", id}, {"cell", cid}}, {},
                  [this, id, cid](const V &) { cell_editor(id, cid); });
        });
        const bool header = ri == 0 && t.at("header").as_boolean();
        const auto custom = a.at("cells").as_object().find(cid);
        const V *override = custom == a.at("cells").as_object().end()
                                ? nullptr
                                : &custom->second;
        const auto get = [&](const char *key, const std::string &fallback) {
          return override && has(*override, key) ? text(*override, key)
                                                 : fallback;
        };
        const auto align = get("align", "left"),
                   vertical = get("vertical", text(a, "cell_vertical"));
        l->set_xalign(align == "right" ? 1.f : align == "center" ? .5f : 0.f);
        l->set_justify(align == "justify" ? Gtk::Justification::FILL
                                          : Gtk::Justification::LEFT);
        l->set_valign(vertical == "bottom"   ? Gtk::Align::END
                      : vertical == "middle" ? Gtk::Align::CENTER
                                             : Gtk::Align::START);
        auto *attrs = pango_attr_list_new();
        pango_attr_list_insert(
            attrs, pango_attr_line_height_new(real(a.at("line_height"))));
        gtk_label_set_attributes(l->gobj(), attrs);
        pango_attr_list_unref(attrs);
        const auto name = "cell-" + cid;
        b->add_css_class(name);
        const auto fill = get("fill",
                              text(a, header ? "header_fill"
                                      : a.at("zebra").as_boolean() && ri % 2
                                          ? "band_fill"
                                          : "fill_color")),
                   color = get("color",
                               text(a, header ? "header_text" : "text_color"));
        const bool bold = override && has(*override, "bold")
                              ? override->at("bold").as_boolean()
                              : header && a.at("header_bold").as_boolean();
        const bool italic = override && has(*override, "italic") &&
                            override->at("italic").as_boolean();
        const auto border = text(a, "border_kind");
        styles +=
            "." + name + " { background: " + fill + "; color: " + color +
            "; font-family: " +
            (text(a, "font") == "serif"  ? "serif"
             : text(a, "font") == "mono" ? "monospace"
                                         : "sans-serif") +
            "; font-size: " + std::to_string(real(a.at("font_size"))) +
            "px; font-weight: " + (bold ? "bold" : "normal") +
            "; font-style: " + (italic ? "italic" : "normal") +
            "; padding: " + std::to_string(real(a.at("padding_y"))) + "px " +
            std::to_string(real(a.at("padding_x"))) +
            "px; border-radius: 0; border: " +
            std::to_string(border == "none" ? 0 : real(a.at("border_width"))) +
            "px " + text(a, "border_style") + " " + text(a, "border_color") +
            "; " +
            (border == "horizontal"
                 ? "border-left-width: 0; border-right-width: 0;"
                 : "") +
            " }";
        if (border == "outer") {
          styles +=
              "." + name + " { border-top-width: " +
              std::to_string(ri == 0 ? real(a.at("border_width")) : 0) +
              "px; border-bottom-width: " +
              std::to_string(ri + 1 == rows.size() ? real(a.at("border_width"))
                                                   : 0) +
              "px; border-left-width: " +
              std::to_string(c.as_string() == cols.front().as_string()
                                 ? real(a.at("border_width"))
                                 : 0) +
              "px; border-right-width: " +
              std::to_string(c.as_string() == cols.back().as_string()
                                 ? real(a.at("border_width"))
                                 : 0) +
              "px; }";
        }
        auto height = real(a.at("min_row_height"));
        if (a.at("row_heights").as_object().contains(r.as_string()))
          height = real(a.at("row_heights").at(r.as_string()));
        b->set_size_request(35, static_cast<int>(height));
        cells.push_back(b);
      }
      box.append(*Gtk::make_managed<FractionRow>(cells, weights, 0));
      ++ri;
    }
    if (text(a, "caption_side") == "bottom")
      caption();
    button(box, "Aparência da tabela", "native-style-" + id, [this, id] {
      selected = id;
      appearance_editor();
    });
  }
  void begin_editor(const std::string &heading) {
    if (busy || draft)
      return;
    object_menu.popdown();
    draft = true;
    draft_tools_open = draft_tools_open || side_revealer.get_reveal_child();
    side_revealer.set_reveal_child(false);
    side_revealer.set_visible(false);
    toolbar.set_visible(false);
    if (owner.editing_changed)
      owner.editing_changed(true);
    status.set_text("Edição aberta · Aplique para registrar os campos ou "
                    "cancele para conservar a versão anterior.");
    fields.clear();
    choices.clear();
    texts.clear();
    checks.clear();
    rules.clear();
    runs.clear();
    clear(editor);
    clear(edit_actions);
    label(editor, heading).add_css_class("title-2");
    stack.set_visible_child("editor");
    edit_scroll.get_vadjustment()->set_value(0);
    sidebar.set_sensitive(false);
    toolbar.set_sensitive(false);
    preview = nullptr;
    math_preview = nullptr;
  }
  Gtk::Entry &entry(const std::string &name, const std::string &caption,
                    const std::string &value = "") {
    label(editor, caption);
    auto *v = Gtk::make_managed<Gtk::Entry>();
    gtk_accessible_update_property(GTK_ACCESSIBLE(v->gobj()),
                                   GTK_ACCESSIBLE_PROPERTY_LABEL,
                                   caption.c_str(), -1);
    v->set_text(value);
    v->set_name("native-field-" + name);
    editor.append(*v);
    fields[name] = v;
    return *v;
  }
  Gtk::ComboBoxText &
  choice(const std::string &name, const std::string &caption,
         const std::vector<std::pair<std::string, std::string>> &options,
         const std::string &value = "") {
    label(editor, caption);
    auto *v = Gtk::make_managed<Gtk::ComboBoxText>();
    limit_choice(*v);
    gtk_accessible_update_property(GTK_ACCESSIBLE(v->gobj()),
                                   GTK_ACCESSIBLE_PROPERTY_LABEL,
                                   caption.c_str(), -1);
    for (const auto &[id, t] : options)
      v->append(id, t);
    v->set_active_id(value);
    if (v->get_active_row_number() < 0)
      v->set_active(0);
    v->set_name("native-field-" + name);
    editor.append(*v);
    choices[name] = v;
    return *v;
  }
  Gtk::CheckButton &check(const std::string &name, const std::string &caption,
                          bool value) {
    auto *v = Gtk::make_managed<Gtk::CheckButton>(caption);
    v->set_active(value);
    v->set_name("native-field-" + name);
    editor.append(*v);
    checks[name] = v;
    return *v;
  }
  Gtk::TextView &source(const std::string &name, const std::string &caption,
                        const std::string &value) {
    label(editor, caption);
    auto *v = Gtk::make_managed<Gtk::TextView>();
    gtk_accessible_update_property(GTK_ACCESSIBLE(v->gobj()),
                                   GTK_ACCESSIBLE_PROPERTY_LABEL,
                                   caption.c_str(), -1);
    v->set_name("native-field-" + name);
    v->set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    v->get_buffer()->set_text(value);
    auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_min_content_height(120);
    scroll->set_child(*v);
    editor.append(*scroll);
    texts[name] = v;
    return *v;
  }
  std::string field(const std::string &name) const {
    return fields.at(name)->get_text();
  }
  std::string chosen(const std::string &name) const {
    return choices.at(name)->get_active_id();
  }
  std::string source_text(const std::string &name) const {
    return texts.at(name)->get_buffer()->get_text();
  }
  static std::string numeric_text(double value) {
    return objects::pilot::encode_json(V(value));
  }
  static double parse_number(std::string s) {
    if (s.find('.') == s.npos)
      std::replace(s.begin(), s.end(), ',', '.');
    double value = 0;
    auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    if (s.empty() || result.ec != std::errc{} ||
        result.ptr != s.data() + s.size() || !std::isfinite(value))
      throw std::runtime_error("invalid_table_design");
    return value;
  }
  double number(const std::string &name) const {
    return parse_number(field(name));
  }
  void guard(const std::function<void()> &fn) {
    try {
      fn();
    } catch (const std::exception &e) {
      error(e.what());
    }
  }
  void cancel() {
    if (busy)
      return;
    draft = false;
    side_revealer.set_visible(true);
    side_revealer.set_reveal_child(draft_tools_open);
    draft_tools_open = false;
    toolbar.set_visible(true);
    if (owner.editing_changed)
      owner.editing_changed(busy || choosing_image);
    last_error.clear();
    sidebar.set_sensitive(true);
    toolbar.set_sensitive(true);
    // Return to the existing sheet even if rebuilding must await input-method
    // delivery. A cancelled form must never remain the active workspace.
    stack.set_visible_child("document");
    render();
  }
  void finish() {
    draft = false;
    side_revealer.set_visible(true);
    side_revealer.set_reveal_child(draft_tools_open);
    draft_tools_open = false;
    toolbar.set_visible(true);
    if (owner.editing_changed)
      owner.editing_changed(busy || choosing_image);
    sidebar.set_sensitive(true);
    toolbar.set_sensitive(true);
    stack.set_visible_child("document");
  }
  void footer(const std::function<void()> &apply) {
    button(edit_actions, "Aplicar", "native-apply", [this, apply] {
      guard(apply);
    }).add_css_class("suggested-action");
    button(edit_actions, "Cancelar", "native-cancel", [this] { cancel(); });
  }
  void confirm_reopen() {
    begin_editor("Reabrir a versão salva?");
    label(editor, "O trabalho aplicado depois do último salvamento será "
                  "substituído pela versão salva. Cancele para conservá-lo.");
    footer([this] {
      command("reopen", {{"discard_unsaved", true}}, {},
              [this](const V &) { finish(); });
    });
  }
  void equation_editor(const std::string &id = "") {
    begin_editor(id.empty() ? "Inserir equação" : "Editar equação");
    const auto *e = find(state, "equations", id);
    entry("label", "Nome da equação", e ? text(*e, "label") : "Equação");
    choice("mode", "Apresentação",
           {{"block", "Em destaque"}, {"inline", "Compacta"}},
           e ? text(*e, "mode") : "block");
    auto &templates = choice("template", "Modelos",
                             {{"none", "Escolha um modelo"},
                              {"fraction", "Fração"},
                              {"root", "Raiz"},
                              {"sum", "Somatório"},
                              {"integral", "Integral"},
                              {"matrix", "Matriz"},
                              {"system", "Sistema"}},
                             "none");
    auto &s = source("source", "Fonte LaTeX",
                     e ? text(*e, "source")
                       : R"(x = \frac{-b\pm\sqrt{b^2-4ac}}{2a})");
    math_preview = Gtk::make_managed<MathPreview>();
    editor.append(*math_preview);
    preview = &label(editor, "");
    auto update = [this] {
      math_preview->set_source(source_text("source"),
                               chosen("mode") == "inline");
      preview->set_text(math_preview->error());
    };
    s.get_buffer()->signal_changed().connect(update);
    choices["mode"]->signal_changed().connect(update);
    templates.signal_changed().connect([this] {
      static const std::map<std::string, std::string> values = {
          {"fraction", R"(\frac{a+b}{c})"},
          {"root", R"(\sqrt{x^2+y^2})"},
          {"sum", R"(\sum_{i=1}^{n} i = \frac{n(n+1)}{2})"},
          {"integral", R"(\int_0^1 x^2\,dx = \frac{1}{3})"},
          {"matrix", R"(\begin{pmatrix} a & b \\ c & d \end{pmatrix})"},
          {"system", R"(\begin{aligned} x+y &= 3 \\ x-y &= 1 \end{aligned})"}};
      if (values.contains(chosen("template")))
        texts["source"]->get_buffer()->set_text(values.at(chosen("template")));
    });
    update();
    if (id.empty()) {
      auto &before = choice("before", "Inserir antes de", {}, "");
      destinations(before);
    }
    footer([this, id] {
      O args{{"source", source_text("source")},
             {"mode", chosen("mode")},
             {"label", field("label")}};
      if (id.empty())
        args.emplace("before", chosen("before"));
      else
        args.emplace("id", id);
      command(id.empty() ? "create_equation" : "edit_equation", args,
              id.empty() ? "structure" : "text",
              [this](const V &) { finish(); });
    });
  }
  void table_editor() {
    begin_editor("Inserir tabela");
    entry("title", "Título", "Tabela");
    entry("rows", "Linhas (1–64)", "2");
    entry("columns", "Colunas (1–32)", "3");
    check("header", "Primeira linha é cabeçalho", true);
    auto &b = choice("before", "Inserir antes de", {}, "");
    destinations(b);
    footer([this] {
      const auto r = number("rows"), c = number("columns");
      if (r != std::floor(r) || c != std::floor(c) || r < 1 || r > 64 ||
          c < 1 || c > 32)
        throw std::runtime_error("invalid_table_dimensions");
      command("create_table_sized",
              {{"title", field("title")},
               {"rows", static_cast<std::int64_t>(r)},
               {"columns", static_cast<std::int64_t>(c)},
               {"header", checks.at("header")->get_active()},
               {"before", chosen("before")}},
              "structure", [this](const V &) { finish(); });
    });
  }
  void pick_image() {
    if (busy || draft)
      return;
    chooser_response.disconnect();
    choosing_image = true;
    if (owner.editing_changed)
      owner.editing_changed(true);
    chooser = Gtk::FileChooserNative::create("Inserir figura PNG", window,
                                             Gtk::FileChooser::Action::OPEN,
                                             "Escolher", "Cancelar");
    auto filter = Gtk::FileFilter::create();
    filter->set_name("PNG estático");
    filter->add_mime_type("image/png");
    chooser->add_filter(filter);
    chooser->set_modal(true);
    chooser_response = chooser->signal_response().connect([this](int response) {
      choosing_image = false;
      if (owner.editing_changed)
        owner.editing_changed(draft || busy);
      chooser->hide();
      if (response != static_cast<int>(Gtk::ResponseType::ACCEPT))
        return;
      guard([this] {
        auto file = chooser->get_file();
        if (!file)
          return;
        const auto path = file->get_path();
        const auto size = std::filesystem::file_size(path);
        if (size > 8 * 1024 * 1024)
          throw std::runtime_error("png_byte_limit");
        std::ifstream in(path, std::ios::binary);
        if (!in)
          throw std::runtime_error("png_read_failed");
        image_bytes.assign(std::istreambuf_iterator<char>(in), {});
        if (in.bad() || image_bytes.size() != size)
          throw std::runtime_error("png_read_failed");
        begin_editor("Inserir figura");
        entry("caption", "Legenda",
              std::filesystem::path(path).filename().string());
        entry("alt", "Descrição para leitura alternativa");
        check("decorative", "Figura decorativa", false);
        auto &b = choice("before", "Inserir antes de", {}, "");
        destinations(b);
        footer([this] {
          command("import",
                  {{"data", Glib::Base64::encode(image_bytes)},
                   {"caption", field("caption")},
                   {"alt", field("alt")},
                   {"decorative", checks.at("decorative")->get_active()},
                   {"before", chosen("before")}},
                  "structure", [this](const V &) {
                    finish();
                    image_bytes.clear();
                  });
        });
      });
    });
    chooser->show();
  }
  void cell_editor(const std::string &table, const std::string &cell) {
    editor_table = table;
    editor_cell = cell;
    const auto *t = find(state, "tables", table);
    if (!t)
      return;
    const V *c = nullptr;
    for (const auto &v : t->at("cells").as_array())
      if (text(v, "id") == cell)
        c = &v;
    if (!c)
      return;
    begin_editor("Editar célula · " + text(*t, "title"));
    if (state.at("formula_cells").as_object().contains(cell)) {
      const auto fid = state.at("formula_cells").at(cell).as_string();
      formula_view(editor, fid, cell);
      button(editor, "Retirar uso da fórmula", "native-detach",
             [this, table, cell] {
               finish();
               command("detach_formula", {{"table", table}, {"cell", cell}},
                       "structure");
             });
      button(edit_actions, "Voltar", "native-cancel", [this] { cancel(); });
    } else {
      source("cell", "Texto da célula", text(*c, "text"));
      button(editor, "Inserir fórmula nesta célula", "native-cell-formula",
             [this] {
               const auto table = editor_table, cell = editor_cell;
               command("edit_cell",
                       {{"id", table},
                        {"cell", cell},
                        {"text", source_text("cell")}},
                       "text", [this, table, cell](const V &) {
                         finish();
                         formula_picker(table, cell);
                       });
             });
      auto &existing =
          choice("existing-formula", "Ou usar fórmula existente", {}, "");
      for (const auto &f : state.at("formulas").as_array())
        if (!f.at("command").as_boolean() && !f.at("consumed").as_boolean())
          existing.append(text(f, "id"), text(f, "name"));
      existing.set_active(0);
      button(editor, "Usar fórmula escolhida nesta célula",
             "native-cell-attach", [this, table, cell] {
               const auto id = chosen("existing-formula");
               if (id.empty())
                 return;
               command("edit_cell",
                       {{"id", table},
                        {"cell", cell},
                        {"text", source_text("cell")}},
                       "text", [this, table, cell, id](const V &) {
                         finish();
                         command("attach_formula",
                                 {{"id", id},
                                  {"before", ""},
                                  {"table", table},
                                  {"cell", cell}},
                                 "structure");
                       });
             });
      footer([this, table, cell] {
        command("edit_cell",
                {{"id", table}, {"cell", cell}, {"text", source_text("cell")}},
                "text", [this](const V &) { finish(); });
      });
    }
  }
  void formula_picker(const std::string &table = "",
                      const std::string &cell = "") {
    begin_editor("Escolher um cartão de fórmula");
    editor_table = table;
    editor_cell = cell;
    label(editor, "Preencha os cartões ou use a escrita livre. Referências "
                  "podem vir de outras fórmulas, inclusive de células.");
    for (const auto &[kind, caption] :
         std::vector<std::pair<std::string, std::string>>{
             {"condition", "Texto por condição"},
             {"value", "Guardar um valor"},
             {"calculate", "Calcular dois valores"},
             {"change", "Alterar valor à distância"},
             {"free", "Escrita livre"}})
      button(editor, caption, "native-kind-" + kind, [this, kind, table, cell] {
        cancel();
        formula_editor("", "", table, cell, kind);
      });
    button(edit_actions, "Cancelar", "native-cancel", [this] { cancel(); });
  }
  std::vector<std::pair<std::string, std::string>>
  references(bool variables_only = false) {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto &f : state.at("formulas").as_array()) {
      if (f.at("command").as_boolean() || f.at("consumed").as_boolean() ||
          text(f, "id") == editor_id)
        continue;
      const auto name = text(f, "name");
      if (!variables_only)
        out.emplace_back(name + ".resultado", name + " · resultado");
      for (const auto &v : f.at("variables").as_array())
        out.emplace_back(name + "." + text(v, "name"),
                         name + " · " + text(v, "name") + " = " +
                             text(v, "current_display"));
    }
    return out;
  }
  void operand(const std::string &key, const std::string &caption,
               const V &value) {
    auto options = references();
    options.insert(options.begin(), {"literal", "Valor preenchido"});
    choice(key + "-reference", caption, options,
           text(value, "kind") == "reference"
               ? text(value, "formula") + "." + text(value, "variable")
               : "literal");
    entry(key + "-value", "Valor",
          text(value, "kind") == "literal" ? text(value, "value") : "");
  }
  V operand_value(const std::string &key) {
    const auto ref = chosen(key + "-reference");
    if (ref == "literal")
      return O{
          {"kind", "literal"},
          {"value", (editor_kind == "calculate" ||
                     (choices.contains("type") && chosen("type") == "number"))
                        ? numeric_text(parse_number(field(key + "-value")))
                        : field(key + "-value")}};
    const auto dot = ref.find('.');
    return O{{"kind", "reference"},
             {"formula", ref.substr(0, dot)},
             {"variable", ref.substr(dot + 1)}};
  }
  void add_rule(const V &v) {
    if (std::count_if(rules.begin(), rules.end(),
                      [](const auto &r) { return r.op != nullptr; }) >= 12) {
      status.set_text("São permitidas até 12 condições.");
      return;
    }
    auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    auto *op = Gtk::make_managed<Gtk::ComboBoxText>();
    for (const auto &[k, t] : std::vector<std::pair<std::string, std::string>>{
             {"eq", "for igual a"},
             {"ne", "for diferente de"},
             {"lt", "for menor que"},
             {"le", "for menor ou igual a"},
             {"gt", "for maior que"},
             {"ge", "for maior ou igual a"}})
      op->append(k, t);
    op->set_active_id(text(v, "op"));
    auto *value = Gtk::make_managed<Gtk::Entry>(),
         *result = Gtk::make_managed<Gtk::Entry>();
    value->set_text(text(v, "value"));
    value->set_placeholder_text("Comparar com…");
    result->set_text(text(v, "text"));
    result->set_placeholder_text("Mostrar este texto…");
    value->set_hexpand(true);
    result->set_hexpand(true);
    row->append(*op);
    row->append(*value);
    row->append(*result);
    rule_box->append(*row);
    rules.push_back({op, value, result, row});
    const auto index = rules.size() - 1;
    op->set_name("native-rule-op-" + std::to_string(index));
    value->set_name("native-rule-value-" + std::to_string(index));
    result->set_name("native-rule-result-" + std::to_string(index));
    button(*row, "Subir", "native-rule-up-" + std::to_string(index),
           [this, row] { shift_rule(row, -1); });
    button(*row, "Descer", "native-rule-down-" + std::to_string(index),
           [this, row] { shift_rule(row, 1); });
    button(*row, "Retirar", "native-rule-remove-" + std::to_string(index),
           [this, row] {
             for (auto &r : rules)
               if (r.row == row) {
                 r.op = nullptr;
                 row->set_visible(false);
               }
           });
    update_comparisons();
  }
  void shift_rule(Gtk::Box *row, int delta) {
    auto at = std::find_if(rules.begin(), rules.end(),
                           [&](const auto &r) { return r.row == row; });
    if (at == rules.end())
      return;
    const auto i = std::distance(rules.begin(), at);
    auto j = i + delta;
    while (j >= 0 && j < static_cast<long>(rules.size()) &&
           !rules[static_cast<std::size_t>(j)].op)
      j += delta;
    if (j < 0 || j >= static_cast<long>(rules.size()))
      return;
    std::swap(rules[static_cast<std::size_t>(i)],
              rules[static_cast<std::size_t>(j)]);
    Gtk::Widget *previous = nullptr;
    for (const auto &r : rules) {
      if (previous)
        rule_box->reorder_child_after(*r.row, *previous);
      else
        rule_box->reorder_child_at_start(*r.row);
      previous = r.row;
    }
  }
  void update_comparisons() {
    if (!choices.contains("type"))
      return;
    const bool numeric = chosen("type") == "number";
    for (auto &r : rules)
      if (r.op) {
        const auto current = r.op->get_active_id();
        r.op->remove_all();
        r.op->append("eq", "for igual a");
        r.op->append("ne", "for diferente de");
        if (numeric) {
          r.op->append("lt", "for menor que");
          r.op->append("le", "for menor ou igual a");
          r.op->append("gt", "for maior que");
          r.op->append("ge", "for maior ou igual a");
        }
        if (!r.op->set_active_id(current))
          r.op->set_active_id("eq");
      }
  }
  V collect_model() {
    auto m = draft_model.as_object();
    const auto kind = text(draft_model, "kind");
    if (kind == "condition") {
      auto c = m.at("cards").as_object();
      c["title"] = field("title");
      c["label"] = field("label");
      c["variable"] = field("variable");
      c["type"] = chosen("type");
      c["entry"] = operand_value("entry");
      A list;
      for (const auto &r : rules)
        if (r.op)
          list.emplace_back(O{{"op", r.op->get_active_id().raw()},
                              {"value", chosen("type") == "number"
                                            ? numeric_text(parse_number(
                                                  r.value->get_text().raw()))
                                            : r.value->get_text().raw()},
                              {"text", r.result->get_text().raw()}});
      c["rules"] = std::move(list);
      c["otherwise"] = field("otherwise");
      m["cards"] = std::move(c);
    } else {
      m["title"] = field("title");
      if (kind == "value") {
        m["label"] = field("label");
        m["variable"] = field("variable");
        m["type"] = chosen("type");
        m["entry"] = operand_value("entry");
      } else if (kind == "calculate") {
        m["left"] = operand_value("left");
        m["right"] = operand_value("right");
        m["operation"] = chosen("operation");
      } else if (kind == "change") {
        m["type"] = chosen("type");
        const auto ref = chosen("target");
        const auto dot = ref.find('.');
        if (dot == std::string::npos)
          throw std::runtime_error("formula_variable_missing");
        m["target"] = O{{"formula", ref.substr(0, dot)},
                        {"variable", ref.substr(dot + 1)}};
        m["operation"] = chosen("operation");
        m["operand"] = operand_value("operand");
      } else
        m["source"] = source_text("source");
    }
    return m;
  }
  void formula_editor(const std::string &id = "",
                      const std::string &target = "",
                      const std::string &table = "",
                      const std::string &cell = "",
                      const std::string &kind = "free") {
    if (draft)
      cancel();
    begin_editor(id.empty() ? "Nova fórmula" : "Editar fórmula");
    editor_id = id;
    editor_target = target;
    editor_table = table;
    editor_cell = cell;
    const auto *f = find(state, "formulas", id);
    const V literal = O{{"kind", "literal"}, {"value", "100"}};
    if (f)
      draft_model = f->at("hub");
    else if (kind == "condition")
      draft_model =
          O{{"kind", kind},
            {"cards",
             O{{"version", 2},
               {"title", "Texto por condição"},
               {"variable", "Entrada"},
               {"label", "a entrada"},
               {"type", "number"},
               {"entry", literal},
               {"rules",
                A{V(O{{"op", "gt"}, {"value", "15"}, {"text", "preço caro"}})}},
               {"otherwise", "que pechincha"}}}};
    else if (kind == "value")
      draft_model = O{{"kind", kind},     {"title", "Vida do personagem"},
                      {"type", "number"}, {"variable", "HP"},
                      {"label", "Vida"},  {"entry", literal}};
    else if (kind == "calculate")
      draft_model = O{{"kind", kind},
                      {"title", "Cálculo"},
                      {"left", literal},
                      {"right", literal},
                      {"operation", "add"}};
    else if (kind == "change")
      draft_model = O{{"kind", kind},
                      {"title", "Alterar um valor"},
                      {"type", "number"},
                      {"target", O{{"formula", ""}, {"variable", ""}}},
                      {"operation", "set"},
                      {"operand", O{{"kind", "literal"}, {"value", "80"}}}};
    else
      draft_model = O{{"kind", "free"},
                      {"source", "resultado 0"},
                      {"title", "Fórmula escrita"}};
    if (text(draft_model, "kind") == "condition" &&
        draft_model.at("cards").at("version").as_integer() == 1) {
      auto c = draft_model.at("cards").as_object();
      c["version"] = 2;
      c["title"] = f ? text(*f, "name") : "Texto por condição";
      c["variable"] = "Entrada";
      draft_model = O{{"kind", "condition"}, {"cards", std::move(c)}};
    }
    const auto k = text(draft_model, "kind");
    editor_kind = k;
    std::string unique = "Formula";
    for (std::size_t n = 1;; ++n) {
      unique = "Formula" + std::to_string(n);
      bool exists = false;
      for (const auto &v : state.at("formulas").as_array())
        exists = exists || text(v, "name") == unique;
      if (!exists)
        break;
    }
    entry("name", "Nome curto para usar em outras fórmulas",
          f ? text(*f, "name") : unique)
        .set_editable(!f);
    const auto &m = k == "condition" ? draft_model.at("cards") : draft_model;
    entry("title", "Nome do cartão",
          has(m, "title") ? text(m, "title")
          : f             ? text(*f, "name")
                          : "Fórmula");
    if (k == "condition" || k == "value") {
      entry("label", "Esta informação representa…", text(m, "label"));
      entry("variable", "Nome da informação",
            has(m, "variable") ? text(m, "variable") : "Entrada");
      choice("type", "Tipo da entrada",
             {{"number", "Número"}, {"text", "Texto comparativo"}},
             text(m, "type"));
      operand("entry", "Entrada inicial", m.at("entry"));
    }
    if (k == "condition") {
      label(editor, "Quando a entrada…");
      rule_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
      editor.append(*rule_box);
      for (const auto &r : m.at("rules").as_array())
        add_rule(r);
      button(editor, "Adicionar condição", "native-add-condition", [this] {
        add_rule(O{{"op", "eq"}, {"value", ""}, {"text", ""}});
      });
      choices.at("type")->signal_changed().connect(
          [this] { update_comparisons(); });
      entry("otherwise", "Quando nenhuma condição corresponder, mostrar…",
            text(m, "otherwise"));
    }
    if (k == "calculate") {
      operand("left", "Primeiro valor", m.at("left"));
      choice("operation", "Operação",
             {{"add", "Somar"},
              {"subtract", "Subtrair"},
              {"multiply", "Multiplicar"},
              {"divide", "Dividir"}},
             text(m, "operation"));
      operand("right", "Segundo valor", m.at("right"));
    }
    if (k == "change") {
      choice("type", "Tipo do valor", {{"number", "Número"}, {"text", "Texto"}},
             text(m, "type"));
      const auto &t = m.at("target");
      choice("target", "Variável que será alterada", references(true),
             text(t, "formula") + "." + text(t, "variable"));
      choice("operation", "Como alterar",
             {{"set", "Definir como"},
              {"add", "Acrescentar"},
              {"subtract", "Diminuir"}},
             text(m, "operation"));
      operand("operand", "Novo valor ou quantidade", m.at("operand"));
      label(editor, "Ao aplicar a alteração, o valor ficará registrado na "
                    "fonte da variável e este comando sairá do documento. "
                    "Desfazer permite recuperá-lo.");
    }
    if (k == "free")
      source("source", "Fonte da fórmula", text(m, "source"));
    std::string prefix, suffix;
    bool simple = true;
    if (!target.empty() &&
        state.at("formula_text").as_object().contains(target)) {
      bool seen = false;
      int count = 0;
      for (const auto &run :
           state.at("formula_text").at(target).at("runs").as_array())
        if (text(run, "kind") == "result") {
          seen = true;
          if (text(run, "formula") != id || ++count != 1)
            simple = false;
        } else if (seen)
          suffix += text(run, "text");
        else
          prefix += text(run, "text");
    }
    if (!simple) {
      editor_target.clear();
      label(editor, "A frase reúne vários resultados e será preservada. Use "
                    "Editar frase para alterá-la.");
    }
    if (k != "change") {
      entry("prefix", "Texto antes do resultado", prefix);
      entry("suffix", "Texto depois do resultado", suffix);
    }
    if (id.empty() && cell.empty()) {
      auto &before = choice("before", "Inserir antes de", {}, "");
      destinations(before);
    }
    preview = &label(editor, "");
    button(editor, "Conferir resultado / alterações", "native-preview", [this] {
      guard([this] {
        command("preview_formula_hub",
                {{"id", editor_id},
                 {"name", field("name")},
                 {"model", collect_model()}},
                {}, [this](const V &r) {
                  const auto &p = r.at("preview");
                  std::string s = p.at("valid").as_boolean()
                                      ? text(p, "display")
                                      : "Confira a fonte e as referências.";
                  for (const auto &c : p.at("changes").as_array())
                    s += text(c, "formula") + "." + text(c, "variable") + ": " +
                         text(c, "before") + " → " + text(c, "after") + "\n";
                  preview->set_text(s);
                });
      });
    });
    if (f) {
      for (const auto &v : f->at("variables").as_array()) {
        label(editor, text(v, "name") + " · inicial " +
                          text(v, "initial_display") + " · atual " +
                          text(v, "current_display"));
      }
      if (k != "free")
        button(editor, "Editar em escrita livre", "native-advanced",
               [this, id] {
                 guard([this, id] {
                   const auto original =
                       objects::formula::hub::compile(collect_model());
                   const auto target = editor_target;
                   const auto prefix =
                       fields.contains("prefix") ? field("prefix") : "";
                   const auto suffix =
                       fields.contains("suffix") ? field("suffix") : "";
                   finish();
                   begin_editor("Escrita livre · fonte integral");
                   source("source", "Fonte da fórmula", original);
                   entry("prefix", "Texto antes do resultado", prefix);
                   entry("suffix", "Texto depois do resultado", suffix);
                   footer([this, id, target] {
                     command("edit_formula_hub",
                             {{"id", id},
                              {"model", O{{"kind", "free"},
                                          {"source", source_text("source")}}},
                              {"target", target},
                              {"prefix", field("prefix")},
                              {"suffix", field("suffix")}},
                             "text", [this](const V &) { finish(); });
                   });
                 });
               });
    }
    footer([this, id] {
      const bool change = text(draft_model, "kind") == "change";
      O args{{"model", collect_model()},
             {"prefix", change ? "" : field("prefix")},
             {"suffix", change ? "" : field("suffix")}};
      if (id.empty()) {
        args.emplace("name", field("name"));
        args.emplace("before", editor_cell.empty() ? chosen("before") : "");
        args.emplace("table", editor_table);
        args.emplace("cell", editor_cell);
      } else {
        args.emplace("id", id);
        args.emplace("target", editor_target);
      }
      command(id.empty() ? "create_formula_hub" : "edit_formula_hub", args,
              id.empty() ? "structure" : "text",
              [this](const V &) { finish(); });
    });
  }
  void variables_editor() {
    const auto id = formulas.get_active_id().raw();
    const auto *f = find(state, "formulas", id);
    if (!f)
      return;
    begin_editor("Variáveis · " + text(*f, "name"));
    for (const auto &v : f->at("variables").as_array()) {
      label(editor, text(v, "name") + " · inicial " +
                        text(v, "initial_display") + " · atual " +
                        text(v, "current_display"));
      if (v.at("overridden").as_boolean())
        button(editor, "Restaurar valor inicial de " + text(v, "name"),
               "native-reset-variable", [this, id, name = text(v, "name")] {
                 finish();
                 command("reset_formula_variable",
                         {{"id", id}, {"variable", name}}, "text");
               });
    }
    button(edit_actions, "Voltar", "native-cancel", [this] { cancel(); });
  }
  void attach_formula() {
    if (formulas.get_active_id().empty())
      return;
    command("attach_formula",
            {{"id", formulas.get_active_id().raw()},
             {"before", ""},
             {"table", ""},
             {"cell", ""}},
            "structure");
  }
  void add_run(const V &run) {
    auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    auto *kind = Gtk::make_managed<Gtk::ComboBoxText>(),
         *ref = Gtk::make_managed<Gtk::ComboBoxText>();
    kind->append("text", "Texto editável");
    kind->append("result", "Resultado ligado");
    kind->set_active_id(text(run, "kind"));
    auto *literal = Gtk::make_managed<Gtk::TextView>();
    literal->set_hexpand(true);
    literal->set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    literal->set_size_request(160, 64);
    if (text(run, "kind") == "text")
      literal->get_buffer()->set_text(text(run, "text"));
    for (const auto &f : state.at("formulas").as_array())
      if (!f.at("command").as_boolean() && !f.at("consumed").as_boolean())
        ref->append(text(f, "id"), text(f, "name"));
    if (text(run, "kind") == "result")
      ref->set_active_id(text(run, "formula"));
    else
      ref->set_active(0);
    row->append(*kind);
    row->append(*literal);
    row->append(*ref);
    auto show = [kind, literal, ref] {
      const bool writing = kind->get_active_id() == "text";
      literal->set_visible(writing);
      ref->set_visible(!writing);
    };
    kind->signal_changed().connect(show);
    show();
    run_box->append(*row);
    runs.push_back({kind, ref, literal, row});
    const auto index = runs.size() - 1;
    button(*row, "Subir", "native-run-up-" + std::to_string(index),
           [this, row] { shift_run(row, -1); });
    button(*row, "Descer", "native-run-down-" + std::to_string(index),
           [this, row] { shift_run(row, 1); });
    button(*row, "Retirar", "native-run-remove-" + std::to_string(index),
           [this, row] {
             for (auto &r : runs)
               if (r.row == row) {
                 r.kind = nullptr;
                 row->set_visible(false);
               }
           });
  }
  void shift_run(Gtk::Box *row, int delta) {
    auto at = std::find_if(runs.begin(), runs.end(),
                           [&](const auto &r) { return r.row == row; });
    if (at == runs.end())
      return;
    const auto i = std::distance(runs.begin(), at);
    auto j = i + delta;
    while (j >= 0 && j < static_cast<long>(runs.size()) &&
           !runs[static_cast<std::size_t>(j)].kind)
      j += delta;
    if (j < 0 || j >= static_cast<long>(runs.size()))
      return;
    std::swap(runs[static_cast<std::size_t>(i)],
              runs[static_cast<std::size_t>(j)]);
    Gtk::Widget *previous = nullptr;
    for (const auto &r : runs) {
      if (previous)
        run_box->reorder_child_after(*r.row, *previous);
      else
        run_box->reorder_child_at_start(*r.row);
      previous = r.row;
    }
  }
  void phrase_editor(const std::string &target) {
    if (draft)
      cancel();
    if (!state.at("formula_text").as_object().contains(target))
      return;
    begin_editor("Texto e resultados na mesma frase");
    run_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    editor.append(*run_box);
    for (const auto &r :
         state.at("formula_text").at(target).at("runs").as_array())
      add_run(r);
    button(editor, "Acrescentar trecho", "native-add-run",
           [this] { add_run(O{{"kind", "text"}, {"text", ""}}); });
    footer([this, target] {
      A values;
      for (const auto &r : runs)
        if (r.kind) {
          if (r.kind->get_active_id() == "text")
            values.emplace_back(
                O{{"kind", "text"},
                  {"text", r.literal->get_buffer()->get_text().raw()}});
          else
            values.emplace_back(
                O{{"kind", "result"},
                  {"formula", r.formula->get_active_id().raw()}});
        }
      command("edit_formula_text",
              {{"target", target}, {"runs", std::move(values)}}, "text",
              [this](const V &) { finish(); });
    });
  }
  void layout_editor() {
    if (selected.empty())
      return;
    begin_editor("Posição e alinhamento · " + title(selected));
    const auto id = selected;
    auto &before = choice("before", "Mover antes de", {}, "");
    destinations(before);
    choice("intent", "Ordem que será alterada",
           {{"visual", "Apresentação"},
            {"authorial", "Sequência autoral e leitura"}},
           "visual");
    button(editor, "Mover", "native-move", [this, id] {
      command("move",
              {{"id", id},
               {"before", chosen("before")},
               {"intent", chosen("intent")}},
              "structure", [this](const V &) { finish(); });
    });
    auto &other = choice("other", "Colocar lado a lado com", {}, "");
    for (const auto &v : state.at("visual_order").as_array())
      if (v.as_string() != id && shown(v.as_string()))
        other.append(v.as_string(), title(v.as_string()));
    other.set_active(0);
    choice("side", "Posição do selecionado",
           {{"left", "À esquerda"}, {"right", "À direita"}}, "right");
    entry("first", "Largura da primeira coluna (%)", "50");
    entry("gap", "Espaço entre colunas (px)", "16");
    button(editor, "Compor lado a lado", "native-pair", [this, id] {
      guard([this, id] {
        const auto first = number("first") / 100.;
        command("arrange_objects",
                {{"id", id},
                 {"other", chosen("other")},
                 {"side", chosen("side")},
                 {"first", first},
                 {"second", 1 - first},
                 {"gap", number("gap")}},
                "composition", [this](const V &) { finish(); });
      });
    });
    button(editor, "Voltar o selecionado à coluna única", "native-stack",
           [this, id] {
             command("stack", {{"id", id}}, "composition",
                     [this](const V &) { finish(); });
           });
    button(editor, "Restaurar tamanho e alinhamento padrão",
           "native-reset-placement", [this, id] {
             command("reset_presentation", {{"id", id}}, "composition",
                     [this](const V &) { finish(); });
           });
    V p = O{{"width", 100},
            {"horizontal", "start"},
            {"text_align", "left"},
            {"vertical", "start"}};
    if (state.at("presentation").as_object().contains(id))
      p = state.at("presentation").at(id);
    entry("width", "Largura do objeto (20–100%)",
          numeric_text(real(p.at("width"))));
    choice("horizontal", "Alinhamento horizontal",
           {{"start", "Esquerda"}, {"center", "Centro"}, {"end", "Direita"}},
           text(p, "horizontal"));
    choice("text_align", "Alinhamento do texto",
           {{"left", "Esquerda"},
            {"center", "Centro"},
            {"right", "Direita"},
            {"justify", "Justificado"}},
           text(p, "text_align"));
    choice("vertical", "Alinhamento vertical",
           {{"start", "Topo"}, {"center", "Centro"}, {"end", "Base"}},
           text(p, "vertical"));
    footer([this, id] {
      command("set_presentation",
              {{"id", id},
               {"width", number("width")},
               {"horizontal", chosen("horizontal")},
               {"text_align", chosen("text_align")},
               {"vertical", chosen("vertical")}},
              "composition", [this](const V &) { finish(); });
    });
  }
  void removed_editor() {
    begin_editor("Restaurar itens retirados");
    auto &list = choice("removed", "Item preservado", {}, "");
    for (const auto &v : state.at("removed_items").as_array())
      list.append(text(v, "id"), text(v, "kind") + ": " + text(v, "label"));
    list.set_active(0);
    auto &before = choice("before", "Restaurar antes de", {}, "");
    destinations(before);
    footer([this] {
      command("restore_removed",
              {{"id", chosen("removed")}, {"before", chosen("before")}},
              "structure", [this](const V &) { finish(); });
    });
  }
  void axes_editor() {
    const auto *t = find(state, "tables", selected);
    if (!t)
      return;
    begin_editor("Linhas e colunas · " + text(*t, "title"));
    const auto id = selected;
    for (const auto &axis : {std::string("row"), std::string("column")}) {
      const auto listkey = axis == "row" ? "row_ids" : "column_ids";
      auto &existing =
          choice(axis,
                 "Antes de / retirar " +
                     std::string(axis == "row" ? "linha" : "coluna"),
                 {}, "");
      existing.append("", "Fim");
      int n = 0;
      for (const auto &v : t->at(listkey).as_array())
        existing.append(v.as_string(), std::to_string(++n));
      existing.set_active_id("");
      button(editor, axis == "row" ? "Inserir linha" : "Inserir coluna",
             "native-insert-" + axis, [this, id, axis] {
               command("insert_table_axis",
                       {{"id", id}, {"axis", axis}, {"before", chosen(axis)}},
                       "structure", [this](const V &) { finish(); });
             });
      button(editor, axis == "row" ? "Retirar linha" : "Retirar coluna",
             "native-retire-" + axis, [this, id, axis] {
               command("remove_table_axis",
                       {{"id", id}, {"axis", axis}, {"axis_id", chosen(axis)}},
                       "structure", [this](const V &) { finish(); });
             });
      auto &removed =
          choice(axis + "-removed", "Retiradas e preservadas", {}, "");
      for (const auto &v :
           t->at("structure")
               .at(axis == "row" ? "removed_rows" : "removed_columns")
               .as_array())
        removed.append(text(v, "id"), text(v, "label").empty()
                                          ? "Item retirado"
                                          : text(v, "label"));
      removed.set_active(0);
      button(editor, axis == "row" ? "Restaurar linha" : "Restaurar coluna",
             "native-restore-" + axis, [this, id, axis] {
               command("restore_table_axis",
                       {{"id", id},
                        {"axis", axis},
                        {"axis_id", chosen(axis + "-removed")}},
                       "structure", [this](const V &) { finish(); });
             });
    }
    button(edit_actions, "Voltar", "native-cancel", [this] { cancel(); });
  }
  void appearance_editor() {
    const auto *t = find(state, "tables", selected);
    if (!t)
      return;
    begin_editor("Aparência da tabela");
    editor_id = selected;
    draft_model = t->at("appearance");
    entry("title", "Título da tabela", text(*t, "title"));
    check("header", "Primeira linha é cabeçalho", t->at("header").as_boolean());
    auto &preset = choice("preset", "Modelos de aparência",
                          {{"custom", "Personalizado"}}, "custom");
    for (const auto &p : state.at("table_design_presets").as_array())
      preset.append(text(p, "id"), text(p, "label"));
    preset.signal_changed().connect([this] {
      const auto id = chosen("preset");
      for (const auto &p : state.at("table_design_presets").as_array())
        if (text(p, "id") == id) {
          auto next = p.at("appearance").as_object();
          for (const auto *key : {"column_weights", "row_heights", "cells"})
            next[key] = draft_model.at(key);
          draft_model = next;
          update_appearance_fields();
        }
    });
    const std::map<std::string, std::string> captions = {
        {"font_size", "Tamanho da letra (10–32)"},
        {"line_height", "Espaçamento entre linhas (1–2,2)"},
        {"text_color", "Cor do texto (#RRGGBB)"},
        {"fill_color", "Fundo das células"},
        {"header_fill", "Fundo do cabeçalho"},
        {"header_text", "Texto do cabeçalho"},
        {"band_fill", "Fundo alternado"},
        {"border_color", "Cor das bordas"},
        {"border_width", "Espessura das bordas (0–4)"},
        {"padding_x", "Espaço horizontal interno (0–32)"},
        {"padding_y", "Espaço vertical interno (0–32)"},
        {"min_row_height", "Altura mínima das linhas (24–240)"}};
    choice("font", "Fonte",
           {{"sans", "Sem serifa"},
            {"serif", "Com serifa"},
            {"mono", "Monoespaçada"}},
           text(draft_model, "font"));
    choice("border_kind", "Bordas",
           {{"grid", "Grade"},
            {"horizontal", "Horizontais"},
            {"outer", "Contorno"},
            {"none", "Sem bordas"}},
           text(draft_model, "border_kind"));
    choice("border_style", "Traço das bordas",
           {{"solid", "Contínuo"},
            {"dashed", "Tracejado"},
            {"dotted", "Pontilhado"}},
           text(draft_model, "border_style"));
    choice("cell_vertical", "Posição vertical do conteúdo",
           {{"top", "Topo"}, {"middle", "Centro"}, {"bottom", "Base"}},
           text(draft_model, "cell_vertical"));
    choice("caption_align", "Alinhamento do título",
           {{"left", "Esquerda"}, {"center", "Centro"}, {"right", "Direita"}},
           text(draft_model, "caption_align"));
    choice("caption_side", "Posição do título",
           {{"top", "Acima"}, {"bottom", "Abaixo"}},
           text(draft_model, "caption_side"));
    for (const auto &[key, caption] : captions)
      entry(key, caption,
            std::holds_alternative<std::string>(draft_model.at(key).storage())
                ? text(draft_model, key)
                : numeric_text(real(draft_model.at(key))));
    for (const auto &[key, caption] :
         std::vector<std::pair<std::string, std::string>>{
             {"zebra", "Alternar fundo das linhas"},
             {"header_bold", "Cabeçalho em negrito"},
             {"show_title", "Mostrar título"}})
      check(key, caption, draft_model.at(key).as_boolean());
    label(editor,
          "Proporções das colunas: percentuais positivos que somam 100.");
    const auto &cols = t->at("column_ids").as_array();
    const auto &weights = draft_model.at("column_weights").as_array();
    for (std::size_t i = 0; i < cols.size(); ++i)
      entry("column-" + std::to_string(i), "Coluna " + std::to_string(i + 1),
            numeric_text(weights.empty()
                             ? 100. / static_cast<double>(cols.size())
                             : real(weights[i])));
    label(editor,
          "Alturas individuais: deixe vazio para usar a altura mínima.");
    for (const auto &row : t->at("row_ids").as_array())
      entry("height-" + row.as_string(),
            "Altura da linha " + row.as_string().substr(0, 8),
            draft_model.at("row_heights").as_object().contains(row.as_string())
                ? numeric_text(
                      real(draft_model.at("row_heights").at(row.as_string())))
                : "");
    const auto cell = text(state, "selected_cell");
    if (!cell.empty()) {
      label(editor, "Personalizar célula selecionada");
      choice("cell-align", "Alinhamento da célula",
             {{"inherit", "Usar padrão"},
              {"left", "Esquerda"},
              {"center", "Centro"},
              {"right", "Direita"},
              {"justify", "Justificado"}},
             "inherit");
      choice("cell-vertical", "Posição vertical da célula",
             {{"inherit", "Preservar"},
              {"top", "Topo"},
              {"middle", "Centro"},
              {"bottom", "Base"}},
             "inherit");
      check("cell-reset", "Usar novamente os padrões da tabela nesta célula",
            false);
      entry("cell-fill", "Fundo da célula (vazio para preservar)");
      entry("cell-color", "Cor da célula (vazio para preservar)");
      choice("cell-bold", "Negrito da célula",
             {{"inherit", "Preservar"}, {"yes", "Ativar"}, {"no", "Desativar"}},
             "inherit");
      choice("cell-italic", "Itálico da célula",
             {{"inherit", "Preservar"}, {"yes", "Ativar"}, {"no", "Desativar"}},
             "inherit");
    }
    auto *preview_box =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    editor.append(*preview_box);
    button(editor, "Conferir aparência sem aplicar", "native-table-preview",
           [this, cell, preview_box] {
             guard([this, cell, preview_box] {
               const auto a = collect_appearance(cell);
               auto t = find(state, "tables", editor_id)->as_object();
               std::vector<std::string> rows, columns;
               std::set<std::string> cells;
               for (const auto &r : t.at("row_ids").as_array())
                 rows.push_back(r.as_string());
               for (const auto &c : t.at("column_ids").as_array())
                 columns.push_back(c.as_string());
               for (const auto &c : t.at("cells").as_array())
                 cells.insert(text(c, "id"));
               objects::pilot::table_design::validate(a, rows, columns, cells);
               t["appearance"] = a;
               t["title"] = field("title");
               t["header"] = checks.at("header")->get_active();
               clear(*preview_box);
               std::string style;
               table_view(*preview_box, V(t), style);
               css->load_from_data(style);
             });
           });
    footer([this, cell] {
      const auto a = collect_appearance(cell);
      V placement = O{{"width", 100},
                      {"horizontal", "start"},
                      {"text_align", "left"},
                      {"vertical", "start"}};
      if (state.at("presentation").as_object().contains(editor_id))
        placement = state.at("presentation").at(editor_id);
      command("edit_table_design",
              {{"id", editor_id},
               {"title", field("title")},
               {"header", checks.at("header")->get_active()},
               {"appearance", a},
               {"placement", placement}},
              "composition", [this](const V &) { finish(); });
    });
  }
  V collect_appearance(const std::string &cell) {
    auto a = draft_model.as_object();
    for (const auto &[key, input] : fields) {
      if (a.contains(key)) {
        if (std::holds_alternative<std::string>(a.at(key).storage()))
          a[key] = input->get_text().raw();
        else
          a[key] = number(key);
      }
    }
    for (const auto &[key, c] : choices)
      if (a.contains(key))
        a[key] = c->get_active_id().raw();
    for (const auto &[key, c] : checks)
      if (a.contains(key))
        a[key] = c->get_active();
    const auto *table = find(state, "tables", editor_id);
    A weights;
    for (std::size_t i = 0; i < table->at("column_ids").as_array().size(); ++i)
      weights.emplace_back(number("column-" + std::to_string(i)));
    a["column_weights"] = std::move(weights);
    auto heights = a.at("row_heights").as_object();
    for (const auto &r : table->at("row_ids").as_array()) {
      const auto key = "height-" + r.as_string();
      if (field(key).empty())
        heights.erase(r.as_string());
      else
        heights[r.as_string()] = number(key);
    }
    a["row_heights"] = heights;
    if (!cell.empty()) {
      auto overrides = a.at("cells").as_object();
      auto style =
          overrides.contains(cell) ? overrides.at(cell).as_object() : O{};
      if (checks.at("cell-reset")->get_active())
        style.clear();
      if (chosen("cell-vertical") != "inherit")
        style["vertical"] = chosen("cell-vertical");
      if (chosen("cell-align") != "inherit")
        style["align"] = chosen("cell-align");
      if (!field("cell-fill").empty())
        style["fill"] = field("cell-fill");
      if (!field("cell-color").empty())
        style["color"] = field("cell-color");
      for (const auto *key : {"bold", "italic"})
        if (chosen(std::string("cell-") + key) != "inherit")
          style[key] = chosen(std::string("cell-") + key) == "yes";
      if (!style.empty())
        overrides[cell] = style;
      else
        overrides.erase(cell);
      a["cells"] = overrides;
    }
    return a;
  }
  void update_appearance_fields() {
    for (const auto &[key, v] : fields)
      if (has(draft_model, key))
        v->set_text(
            std::holds_alternative<std::string>(draft_model.at(key).storage())
                ? text(draft_model, key)
                : numeric_text(real(draft_model.at(key))));
    for (const auto &[key, c] : choices)
      if (has(draft_model, key))
        c->set_active_id(text(draft_model, key));
    for (const auto &[key, c] : checks)
      if (has(draft_model, key))
        c->set_active(draft_model.at(key).as_boolean());
  }
};
ObjectDocumentEditor::ObjectDocumentEditor(application::ProjectService &s,
                                           Gtk::Window &parent,
                                           const std::string &document)
    : impl_(std::make_unique<Impl>(s, *this, parent, document)) {}
ObjectDocumentEditor::~ObjectDocumentEditor() = default;
void ObjectDocumentEditor::synchronize_document(const project::Document &d) {
  if (!can_leave())
    throw std::runtime_error(
        "Aplique ou cancele a edição aberta antes de mudar de especialidade.");
  const auto before = impl_->state;
  const auto visible =
      visible_paragraphs ? visible_paragraphs(d) : impl_->visible_text;
  {
    std::lock_guard lock(impl_->runtime->mutex);
    impl_->runtime->session->synchronize_document(d);
    impl_->source_document = d;
    impl_->state =
        impl_->runtime->session->request(O{{"op", "view"}}).at("state");
  }
  impl_->visible_text = visible;
  const auto same = [&](const char *key) {
    return persistence::json::serialize(before.at(key)) ==
           persistence::json::serialize(impl_->state.at(key));
  };
  std::set<std::string> shown_paragraphs, displayed_paragraphs;
  for (const auto &p : impl_->state.at("paragraphs").as_array())
    if (impl_->shown(text(p, "id")))
      shown_paragraphs.insert(text(p, "paragraph"));
  for (const auto &[id, view] : impl_->paragraph_views)
    displayed_paragraphs.insert(id);
  if (!impl_->render_pending && same("visual_order") && same("rows") && same("presentation") &&
      displayed_paragraphs == shown_paragraphs)
    refresh_text_views();
  else
    impl_->render();
}
void ObjectDocumentEditor::refresh_text_views() {
  for (const auto &[id, view] : impl_->paragraph_views)
    view->refresh();
  impl_->follow_cursor();
}
void ObjectDocumentEditor::focus_text(int offset, bool reveal) {
  for (const auto &[id, view] : impl_->paragraph_views)
    if (view->contains(offset)) {
      struct FocusGuard {
        bool &flag; bool previous;
        explicit FocusGuard(bool &value):flag(value),previous(value) {flag=true;}
        ~FocusGuard() {flag=previous;}
      } guard(impl_->restoring_text_focus);
      view->focus_offset(offset);
      if (!reveal)
        return;
      impl_->reveal_cursor(*view);
      return;
    }
}
void ObjectDocumentEditor::set_specialty(const std::string &id) {
  impl_->specialty = id;
  for (auto &[key, box] : impl_->tool_groups)
    box->set_visible(key == id);
  for(auto &[key,handle]:impl_->reading_handles)handle->set_visible(id=="structure" && impl_->leading_handle(key));
  const bool classic=id=="text" || id=="semantic" || id=="linguistic" || id=="structure" || id=="bindings";
  impl_->specialty_host.set_visible(classic);
  impl_->side_scroll.set_size_request(classic?390:280,-1);
  impl_->side_scroll.get_vadjustment()->set_value(0);
  const std::map<std::string,std::string> names{{"text","Texto"},{"semantic","Semântica"},{"linguistic","Linguística"},{"structure","Estrutura"},{"bindings","Vínculos"},{"composition","Composição"},{"tables","Tabelas"},{"formulas","Fórmulas"},{"math","Matemática"}};
  impl_->tools_toggle->set_label("Ferramentas · "+names.at(id));
  show_tools(false);
}
void ObjectDocumentEditor::set_visible_text(
    std::optional<std::map<std::string, std::string>> visible) {
  if (impl_->visible_text == visible)
    return;
  bool same_members = bool(impl_->visible_text) == bool(visible);
  if (same_members && visible) {
    same_members = impl_->visible_text->size() == visible->size();
    for (const auto &[id, value] : *visible)
      same_members = same_members && impl_->visible_text->contains(id);
  }
  impl_->visible_text = std::move(visible);
  if (same_members)
    refresh_text_views();
  else
    impl_->render();
}
void ObjectDocumentEditor::show_tools(bool open) {
  if(open && (impl_->draft || impl_->busy || impl_->choosing_image || impl_->pending_actions)) {
    impl_->status.set_text("Conclua ou cancele a edição em andamento antes de abrir as ferramentas.");return;
  }
  impl_->side_revealer.set_reveal_child(open);
  if(!open)impl_->tool_follow.disconnect();
  if(tools_changed)tools_changed(open);
}
void ObjectDocumentEditor::toggle_tools() {show_tools(!impl_->side_revealer.get_reveal_child());}
void ObjectDocumentEditor::reveal_tool(Gtk::Widget &widget) {
  impl_->tool_follow.disconnect();
  auto *impl=impl_.get();
  const auto life=impl_->life;
  impl_->tool_follow=Glib::signal_timeout().connect([impl,life,&widget,tries=0]() mutable {
    if(!life->alive || !impl->side_revealer.get_reveal_child())return false;
    // Wait for allocation of the reused inspector; do not adjust from inside
    // the widget's layout or focus notification.
    if(++tries<2)return true;
    graphene_rect_t bounds{};
    if(!gtk_widget_compute_bounds(widget.gobj(),GTK_WIDGET(impl->sidebar.gobj()),&bounds))return false;
    const auto a=impl->side_scroll.get_vadjustment();
    a->set_value(std::clamp(static_cast<double>(bounds.origin.y)-6.,a->get_lower(),std::max(a->get_lower(),a->get_upper()-a->get_page_size())));
    return false;
  },30);
}
void ObjectDocumentEditor::set_specialty_panel(Gtk::Widget *panel) {
  if(impl_->specialty_panel)impl_->specialty_host.remove(*impl_->specialty_panel);
  impl_->specialty_panel=panel;
  if(panel)impl_->specialty_host.append(*panel);
}
void ObjectDocumentEditor::move_reading(int direction) {impl_->move_step(direction);}
bool ObjectDocumentEditor::can_leave() const {
  for (const auto &[id, view] : impl_->paragraph_views)
    if (view->pending_input())
      return false;
  return !(text_composing && text_composing()) && !impl_->draft &&
         !impl_->busy && !impl_->choosing_image &&
         impl_->pending_actions == 0 &&
         text(impl_->state, "persistence_status") != "uncertain";
}
bool ObjectDocumentEditor::request_leave(const std::string &action,
                                        std::function<void()> resume) {
  if (can_leave())
    return true;
  if (impl_->leave_dialog) {
    impl_->leave_dialog->present();
    return false;
  }
  if (impl_->busy || impl_->pending_actions || impl_->choosing_image) {
    impl_->status.set_text(impl_->choosing_image
        ? "Conclua ou cancele a escolha do arquivo antes de " + action + "."
        : "Há uma operação em andamento. Aguarde sua conclusão antes de " + action + ".");
    return false;
  }
  if (impl_->draft) {
    impl_->leave_dialog = new OverlayDialog(
        "Há uma edição de objeto aberta", impl_->window, true);
    auto *dialog = impl_->leave_dialog;
    dialog->set_secondary_text(
        "Os campos deste formulário ainda não foram aplicados. Você pode "
        "continuar editando ou descartar somente este rascunho para " + action +
        ". O texto e os objetos já aplicados serão conservados.");
    dialog->add_button("Continuar editando", Gtk::ResponseType::CANCEL);
    dialog->add_button("Descartar rascunho e continuar", Gtk::ResponseType::REJECT);
    dialog->signal_response().connect([this, resume](int response) {
      // OverlayDialog schedules its own deletion after delivering a response.
      impl_->leave_dialog = nullptr;
      impl_->defer_action([this, response, resume] {
        if (response == static_cast<int>(Gtk::ResponseType::REJECT)) {
          impl_->cancel();
          resume();
        }
      });
    });
    dialog->present();
  } else {
    impl_->status.set_text(text(impl_->state, "persistence_status") == "uncertain"
        ? "Confira a gravação em Reabrir salvo ou Retomar trabalho antes de " + action + "."
        : "Confirme a entrada de texto em andamento antes de " + action + ".");
  }
  return false;
}
void ObjectDocumentEditor::save_for_project(
    const std::filesystem::path &project) {
  if (project != impl_->project_path)
    return;
  if (impl_->draft || impl_->busy || impl_->choosing_image ||
      impl_->pending_actions != 0)
    throw std::runtime_error(
        "Aplique ou cancele a edição de Objetos antes de salvar o projeto.");
  {
    std::lock_guard lock(impl_->runtime->mutex);
    auto &s = *impl_->runtime->session;
    const auto state = s.request(O{{"op", "view"}}).at("state");
    const auto r =
        s.request(O{{"op", "save"}, {"expected", state.at("revision")}});
    if (!r.at("accepted").as_boolean())
      throw std::runtime_error(
          "Não foi possível confirmar a composição por objetos.");
    impl_->state = r.at("state");
  }
  impl_->render();
}
const V &ObjectNativeProbe::state(const ObjectDocumentEditor &w) {
  return w.impl_->state;
}
bool ObjectNativeProbe::busy(const ObjectDocumentEditor &w) {
  return w.impl_->busy || w.impl_->choosing_image ||
         w.impl_->pending_actions != 0;
}
bool ObjectNativeProbe::draft(const ObjectDocumentEditor &w) {
  return w.impl_->draft;
}
void ObjectNativeProbe::choose_image(ObjectDocumentEditor &w,
                                     const std::filesystem::path &p) {
  if (!w.impl_->chooser)
    throw std::runtime_error("Seletor de arquivo ausente");
  auto file = Gio::File::create_for_path(p.string());
  auto chooser = w.impl_->chooser;
  auto life = w.impl_->life;
  auto *impl = w.impl_.get();
  chooser->set_file(file);
  Glib::signal_timeout().connect(
      [chooser, file, life, impl, tries = 0]() mutable {
        if (!life->alive)
          return false;
        auto selected = chooser->get_file();
        if (selected && g_file_equal(selected->gobj(), file->gobj())) {
          g_signal_emit_by_name(chooser->gobj(), "response",
                                static_cast<int>(Gtk::ResponseType::ACCEPT));
          return false;
        }
        if (++tries >= 100) {
          impl->last_error = "native_file_chooser_selection_failed";
          g_signal_emit_by_name(chooser->gobj(), "response",
                                static_cast<int>(Gtk::ResponseType::CANCEL));
          return false;
        }
        return true;
      },
      50);
}
std::string ObjectNativeProbe::error(const ObjectDocumentEditor &w) {
  return w.impl_->last_error;
}
} // namespace inde::ui

namespace inde::ui {
bool ObjectNativeProbe::tools_open(const ObjectDocumentEditor &w) {
  return w.impl_->side_revealer.get_reveal_child();
}
void ObjectNativeProbe::reveal_object(ObjectDocumentEditor &w,
                                      const std::string &id) {
  Gtk::Widget *target = nullptr;
  std::function<void(Gtk::Widget &)> locate = [&](Gtk::Widget &v) {
    if (v.get_name().raw() == "native-object-" + id)
      target = &v;
    for (auto *c = v.get_first_child(); c; c = c->get_next_sibling())
      locate(*c);
  };
  locate(w);
  if (!target)
    throw std::runtime_error("Context object missing");
  // Focus may start viewport animation. Set the deterministic test position
  // afterwards, cancelling that animation rather than racing its next frame.
  target->grab_focus();
  graphene_rect_t bounds{};
  if (gtk_widget_compute_bounds(target->gobj(),
                                GTK_WIDGET(w.impl_->canvas.gobj()), &bounds)) {
    const auto a = w.impl_->content_scroll.get_vadjustment();
    a->set_value(std::clamp(static_cast<double>(bounds.origin.y) - 32., 0.,
                            std::max(0., a->get_upper() - a->get_page_size())));
  }
}
void ObjectNativeProbe::open_context(ObjectDocumentEditor &w,
                                     const std::string &id) {
  Gtk::Widget *target = nullptr;
  std::function<void(Gtk::Widget &)> locate = [&](Gtk::Widget &v) {
    if (v.get_name().raw() == "native-object-" + id)
      target = &v;
    for (auto *c = v.get_first_child(); c; c = c->get_next_sibling())
      locate(*c);
  };
  locate(w);
  if (!target)
    throw std::runtime_error("Context object missing");
  graphene_rect_t bounds{};
  if (!gtk_widget_compute_bounds(
          target->gobj(), GTK_WIDGET(w.impl_->content_scroll.gobj()), &bounds))
    throw std::runtime_error("Context object has no viewport bounds");
  const double left = std::max(0., -static_cast<double>(bounds.origin.x)),
               top = std::max(0., -static_cast<double>(bounds.origin.y));
  const double right = std::min(static_cast<double>(target->get_width()),
                                w.impl_->content_scroll.get_width() -
                                    static_cast<double>(bounds.origin.x));
  const double bottom = std::min(static_cast<double>(target->get_height()),
                                 w.impl_->content_scroll.get_height() -
                                     static_cast<double>(bounds.origin.y));
  if (right <= left || bottom <= top)
    throw std::runtime_error("Context object is outside the visible viewport");
  const double x = (left + right) / 2, y = (top + bottom) / 2;
  auto *controllers = gtk_widget_observe_controllers(target->gobj());
  for (guint i = 0; i < g_list_model_get_n_items(controllers); ++i) {
    auto *c = g_list_model_get_item(controllers, i);
    if (GTK_IS_GESTURE_CLICK(c) &&
        gtk_gesture_single_get_button(GTK_GESTURE_SINGLE(c)) == 3)
      g_signal_emit_by_name(c, "pressed", 1, x, y);
    g_object_unref(c);
  }
  g_object_unref(controllers);
}
} // namespace inde::ui
