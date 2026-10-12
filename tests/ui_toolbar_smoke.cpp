#include "../src/ui/rendering_policy.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "ui/widget_probe.hpp"
#include <chrono>
#include <iostream>
namespace {
using namespace inde;
using namespace inde::test::ui;
using S = project::DocumentTextStyle;
class Window final : public Gtk::ApplicationWindow {
public:
  Window(const std::filesystem::path &fixture, int &result)
      : writing_(service_), result_(result) {
    service_.open(fixture);
    auto d = service_.writing().create_document("QA três fileiras");
    d.content = "Água 🙂 primeiro parágrafo.\nSegundo parágrafo autoral.";
    d.inline_commands = {{"@", "nota", "Nota QA", "Nota: água 🙂"}};
    d = service_.writing().update_document(d);
    id_ = d.id;
    host_.set_content(writing_);
    set_child(host_);
    set_default_size(1280, 900);
    writing_.install_actions(*this);
    writing_.refresh();
    writing_.reveal_document(id_);
    editor_ =
        find<Gtk::TextView>(writing_, [](auto &v) { return v.get_editable(); });
    require(editor_, "Editor absent");
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL toolbar phase=" << phase_ << ": " << e.what()
                      << std::endl;
            hide();
            return false;
          }
        },
        100);
  }
  ~Window() override { timer_.disconnect(); }

private:
  Gtk::Button &button(const std::string &label) {
    auto *b = find<Gtk::Button>(
        *pages(writing_, "editor").get_child_by_name("editor"),
        [&](auto &v) {
          const auto actual = v.get_label().raw();
          return actual == label ||
                 (label == "Recuperação" &&
                  actual.starts_with("Recuperação (") && actual.ends_with(')'));
        });
    require(b, "Missing " + label);
    return *b;
  }
  void choose(const std::string &id) {
    auto *c = find<Gtk::ComboBoxText>(writing_, [&](auto &c) {
      if(combo_has_id(c,id)) return true;
      return false;
    });
    require(c, "No combo option " + id);
    require(c->set_active_id(id), "Choice failed");
  }
  project::Document saved() {
    writing_.flush_changes();
    return *service_.writing().document(id_);
  }
  bool has(S style, const std::string &value = "") {
    auto d = saved();
    return std::any_of(d.formatting.begin(), d.formatting.end(), [&](auto &s) {
      return s.style == style && s.value == value;
    });
  }
  void select() {
    auto b = editor_->get_buffer();
    b->select_range(b->get_iter_at_offset(0), b->get_iter_at_offset(6));
  }
  Gtk::TextView &modal_text() {
    auto *v = find<Gtk::TextView>(host_, [](auto &v) {
      for (auto *p = v.get_parent(); p; p = p->get_parent())
        if (p->has_css_class("modal-body"))
          return true;
      return false;
    });
    require(v, "No modal text");
    return *v;
  }
  bool step() {
    if (last_phase_ != phase_) {
      last_phase_ = phase_;
      std::cout << "TRACE toolbar phase=" << phase_ << std::endl;
    }
    require(std::chrono::steady_clock::now() - started_ <
                std::chrono::seconds(60),
            "Toolbar timeout");
    auto b = editor_->get_buffer();
    if (phase_ == 0) {
      if (!get_mapped())
        return true;
      auto *row1 = button("← Voltar").get_parent();
      require(row1 && row1->get_mapped(), "Primary toolbar missing");
      require(count(writing_, [](auto &w) { return w.has_css_class("toolbar-surface") && w.get_mapped(); }) == 1,
              "More than one permanent editor toolbar");
      auto *document_actions = button("Salvar").get_parent();
      for (auto name :
           {"Abrir Unidade", "Detalhes", "Salvar", "Recuperação", "Excluir"})
        require(button(name).get_parent() == document_actions, "Document action group");
      require(document_actions != row1 && !document_actions->get_mapped(), "Document actions are permanent");
      auto *format = find<Gtk::MenuButton>(writing_, [](auto &v) { return v.get_label() == "Formatar"; });
      auto *context = find<Gtk::MenuButton>(writing_, [](auto &v) { return v.get_label() == "Ferramentas"; });
      require(format && context, "Missing progressive tool entry points");
      for (auto name : {"⟺", "⟹", "☰", "A", "B", "I", "U", "S", "⮐", "⮑",
                        "Limpar", "✂", "Localizar", "Consultar"})
        require(find<Gtk::Button>(*format->get_popover(), [&](auto &v) { return v.get_label() == name; }), "Missing format control");
      for (auto name : {"Elementos da história", "Marcações do texto", "Realçar marcações",
                        "Marcar Seleção", "Comentar", "Estatísticas", "Meta",
                        "Palavras repetidas", "Parágrafos Longos", "⌘"})
        require(find<Gtk::Button>(*context->get_popover(), [&](auto &v) { return v.get_label() == name; }), "Missing contextual control");
      require(!format->get_popover()->get_mapped() && !context->get_popover()->get_mapped(), "Tools should start closed");
      select();
      choose("Monospace");
      phase_ = 10;
    } else if (phase_ == 10) {
      if (!has(S::FontFamily, "Monospace"))
        return true;
      choose("Serif");
      phase_ = 11;
    } else if (phase_ == 11) {
      if (!has(S::FontFamily, "Serif"))
        return true;
      choose("heading6");
      phase_ = 12;
    } else if (phase_ == 12) {
      if (!has(S::Heading6))
        return true;
      auto *size = find<Gtk::Entry>(writing_, [](auto &s) {
        return GTK_IS_ENTRY(s.gobj()) && s.get_placeholder_text() == "6–96";
      });
      require(size, "No font size");
      size->set_text("18");
      phase_ = 13;
    } else if (phase_ == 13) {
      if (!has(S::FontSize, "18"))
        return true;
      click(writing_, "+");
      phase_ = 14;
    } else if (phase_ == 14) {
      if (!has(S::FontSize, "19"))
        return true;
      click(writing_, "−");
      phase_ = 15;
    } else if (phase_ == 15) {
      if (!has(S::FontSize, "18"))
        return true;
      click(writing_, "⟺");
      require(has(S::AlignCenter), "Center not persistent");
      click(writing_, "⮐");
      require(!has(S::AlignCenter), "Alignment undo failed");
      click(writing_, "⮑");
      require(has(S::AlignCenter), "Alignment redo failed");
      choose("body");
      phase_ = 16;
    } else if (phase_ == 16) {
      if (has(S::Heading6))
        return true;
      require(!has(S::Heading6) && has(S::FontSize, "18"),
              "Body erased character formatting");
      click(writing_, "A");
      phase_ = 1;
    } else if (phase_ == 1) {
      auto *e = find<Gtk::Entry>(host_, [](auto &e) {
        return GTK_IS_ENTRY(e.gobj()) &&
               e.get_placeholder_text() == "Cor #RRGGBB";
      });
      if (!e)
        return true;
      e->set_text("#88aaff");
      click(host_, "Aplicar cor");
      require(has(S::Foreground, "#88aaff"), "Color lost");
      select();
      click(writing_, "Comentar");
      ++phase_;
    } else if (phase_ == 2) {
      modal_text().get_buffer()->set_text("Comentário QA 🙂");
      click(host_, "Salvar anotação");
      auto d = saved();
      require(d.semantics.annotations.size() == 1, "Comment absent");
      require(d.content.find("Comentário") == std::string::npos,
              "Comment entered body");
      auto before = d;
      click(writing_, "⮐");
      require(saved().semantics.annotations.empty(), "Comment undo failed");
      click(writing_, "⮑");
      require(saved().semantics.annotations.size() == 1, "Comment redo failed");
      for (auto name : {"Trechos marcados", "Elementos da história", "Estatísticas", "Meta",
                        "Palavras repetidas", "Parágrafos Longos"}) {
        click(writing_, name);
        require(find<Gtk::Button>(writing_,
                                  [](auto &v) {
                                    return v.get_label() == "Fechar painel" &&
                                           v.get_visible();
                                  }),
                "Panel absent");
        click(writing_, name);
      }
      editor_->grab_focus();
      b->place_cursor(b->end());
      b->insert_at_cursor(" @nota");
      ++phase_;
    } else if (phase_ == 3) {
      auto *pop =
          find<Gtk::Popover>(writing_, [](auto &p) { return p.get_mapped(); });
      if (!pop)
        return true;
      auto *choose = find<Gtk::Button>(
          *pop, [](auto &v) { return v.get_label() == "Nota QA"; });
      require(choose, "No inline candidate");
      g_signal_emit_by_name(choose->gobj(), "clicked");
      require(b->get_text().raw().ends_with("Nota: água 🙂"),
              "Trigger not replaced literally");
      click(writing_, "⮐");
      require(b->get_text().raw().ends_with(" @nota"), "Inline undo failed");
      click(writing_, "⮑");
      require(b->get_text().raw().ends_with("Nota: água 🙂"),
              "Inline redo failed");
      click(writing_, "⌘");
      click(host_, "Gerenciar textos prontos");
      ++phase_;
    } else if (phase_ == 4) {
      click(host_, "Novo texto pronto");
      ++phase_;
    } else if (phase_ == 5) {
      auto *name = find<Gtk::Entry>(host_, [](auto &e) {
        return GTK_IS_ENTRY(e.gobj()) &&
               e.get_placeholder_text() == "Nome do modelo";
      });
      if (!name)
        return true;
      name->set_text("Novo literal QA");
      modal_text().get_buffer()->set_text("Texto QA não executável");
      click(host_, "Salvar modelo");
      require(saved().inline_commands.size() == 2, "Model not saved");
      click(writing_, "⮐");
      require(saved().inline_commands.size() == 1, "Model undo failed");
      click(writing_, "⮑");
      require(saved().inline_commands.size() == 2, "Model redo failed");
      auto before = saved();
      writing_.reset();
      writing_.refresh();
      writing_.reveal_document(id_);
      require(project::same_document_state(saved(), before),
              "Reopen lost styles/comments/templates");
      std::cout << "SNAPSHOT: toolbar\n" << std::flush;
      ++phase_;
      shot_ = std::chrono::steady_clock::now();
    } else {
      if (std::chrono::steady_clock::now() - shot_ < std::chrono::seconds(2))
        return true;
      writing_.reset();
      service_.writing().delete_document(id_);
      service_.close();
      result_ = 0;
      std::cout
          << "PASS: single toolbar and progressive tools, native "
             "font/size/headings/body/alignment/color, comments, direct tool "
             "panels, inline literal suggestion/manage, undo/redo and reopen\n"
          << std::flush;
      hide();
      return false;
    }
    return true;
  }
  application::ProjectService service_;
  ui::WritingWorkspace writing_;
  ui::ModalOverlayHost host_;
  Gtk::TextView *editor_{};
  sigc::connection timer_;
  int &result_;
  int phase_{}, last_phase_{-1};
  std::string id_;
  std::chrono::steady_clock::time_point started_{
      std::chrono::steady_clock::now()},
      shot_;
};
} // namespace
int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-fixture-writes" ||
      !std::getenv("INDE_CLIPBOARD_TEST_ISOLATED"))
    return 2;
  ui::configure_rendering_backend();
  auto app = Gtk::Application::create("io.github.inde.toolbarqa",
                                      Gio::Application::Flags::NON_UNIQUE);
  int result = 1;
  Window *w{};
  app->signal_activate().connect([&] {
    ui::install_application_theme();
    w = new Window(argv[1], result);
    app->add_window(*w);
    w->present();
  });
  app->run(1, argv);
  delete w;
  return result;
}
