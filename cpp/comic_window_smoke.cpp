#include "comic_ui.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/main_window.hpp"
#include "main_window_smoke.hpp"
#include "publication_ui.hpp"
#include <fstream>
#include <iostream>
namespace fs = std::filesystem;
namespace inde::ui {
struct ComicWorkspaceProbe {
  static void check(bool ok, const std::string &why) {
    if (!ok)
      throw std::runtime_error(why);
  }
  static void click(Gtk::Button &b) {
    g_signal_emit_by_name(b.gobj(), "clicked");
  }
  static void routes(MainWindow &w) {
    auto &shell = w.workspace_shell_;
    for (const auto &[button, target] :
         std::vector<std::pair<Gtk::ToggleButton *, WorkspaceId>>{
             {&shell.planning_button_, WorkspaceId::Planning},
             {&shell.review_button_, WorkspaceId::Review},
             {&shell.editorial_button_, WorkspaceId::Editorial},
             {&shell.graphs_button_, WorkspaceId::Graphs},
             {&shell.maps_button_, WorkspaceId::Cartography},
             {&shell.works_button_, WorkspaceId::Catalog}}) {
      click(*button);
      check(shell.current() == target,
            "Botão direto não abriu seu espaço: " + button->get_label().raw());
    }
  }
  static void navigation(MainWindow &w) {
    auto &shell = w.workspace_shell_;
    std::vector<std::string> labels;
    for (auto *child = shell.navigation_.get_first_child(); child;
         child = child->get_next_sibling()) {
      if (auto *menu = dynamic_cast<Gtk::MenuButton *>(child))
        labels.push_back(menu->get_label());
      else if (auto *button = dynamic_cast<Gtk::ToggleButton *>(child))
        labels.push_back(button->get_label());
    }
    check(labels == std::vector<std::string>{"Criar", "Planejar", "Revisar",
                                             "Manuscrito", "Cronologia",
                                             "Mapas", "Obras"},
          "Barra diferente do contrato");
    check(shell.writing_button_.get_label() == "Escrita" &&
              shell.comic_button_.get_label() == "Comic",
          "Menu Criar incorreto");
    check(shell.current() == WorkspaceId::Comic, "Comic não é o espaço ativo");
    check(!w.modal_host_.has_dialog(), "Comic abriu como diálogo");
    check(w.comic_workspace_.get_mapped(), "Workspace Comic não está visível");
  }
  static void choose(MainWindow &w, bool comic) {
    w.workspace_shell_.create_button_.popup();
    click(comic ? w.workspace_shell_.comic_button_
                : w.workspace_shell_.writing_button_);
    check(w.workspace_shell_.current() ==
              (comic ? WorkspaceId::Comic : WorkspaceId::Writing),
          "Escolha no menu Criar não navegou");
  }
  static ComicWorkspace &workspace(MainWindow &w) { return w.comic_workspace_; }
  static void edit(ComicWorkspace &p) {
    click(p.demo_);
    check(p.dirty_ && p.book_.panels.size() == 3,
          "Amostra nativa não criou rascunho");
    click(p.save_);
    check(!p.dirty_ && p.book_.revision == 1,
          "Salvar comic falhou: " + p.status_.get_text());
    auto reference = p.service_.books().front();
    check(!reference.assets.empty(),
          "Fixture sem arte para testar o histórico de recursos");
    auto asset = reference.assets.begin()->second;
    asset.id = project::new_uuid();
    p.checkpoint();
    p.book_.assets[asset.id] = asset;
    p.panel()->image = asset.id;
    p.changed();
    p.undo();
    check(!p.book_.assets.contains(asset.id),
          "Desfazer importação manteve recurso ativo");
    p.redo();
    check(p.book_.assets.at(asset.id).data == asset.data,
          "Refazer perdeu os bytes da ilustração");
    click(p.save_);
    p.text_.get_buffer()->set_text("Texto editado dentro do espaço Comic.");
    check(p.dirty_ && !p.undo_.empty(),
          "Texto não entrou no histórico de composição");
    p.undo();
    check(p.book_.panels[0].elements[0].text !=
              "Texto editado dentro do espaço Comic.",
          "Desfazer da comic falhou");
    p.redo();
    check(p.book_.panels[0].elements[0].text ==
              "Texto editado dentro do espaço Comic.",
          "Refazer da comic falhou");
    p.zoom_.set_active_id("125");
    p.inspector_.set_current_page(0);
  }
  static std::string view(ComicWorkspace &p) {
    return serialize_view(p.capture_view());
  }
  static std::string text(ComicWorkspace &p) {
    return p.text_.get_buffer()->get_text();
  }
  static void preedit(ComicWorkspace &p, const char *text) {
    g_signal_emit_by_name(p.text_.gobj(), "preedit-changed", text);
  }
  static void verify_draft(ComicWorkspace &p, const std::string &text) {
    check(p.dirty_ && p.text_.get_buffer()->get_text().raw() == text,
          "Trocar espaço descartou rascunho");
    check(p.zoom_.get_active_id() == "125", "Trocar espaço perdeu zoom");
  }
  static void save_and_generate(ComicWorkspace &p) {
    click(p.save_);
    check(!p.dirty_, "Salvar rascunho falhou: " + p.status_.get_text().raw());
    const auto count = p.panel()->elements.size();
    click(p.remove_element_);
    check(p.panel()->elements.size() + 1 == count, "Remoção falhou");
    p.undo();
    check(p.panel()->elements.size() == count,
          "Desfazer remoção falhou; elementos=" +
              std::to_string(p.panel()->elements.size()) +
              " undo=" + std::to_string(p.undo_.size()) +
              " ime=" + std::to_string(p.composing_) +
              " status=" + p.status_.get_text().raw());
    click(p.copy_document_);
    check(!p.panel()->elements.back().source_document.empty(),
          "Cópia perdeu origem");
    click(p.save_);
    click(p.generate_);
    check(p.job_, "Preparação falhou: " + p.status_.get_text());
    check(!p.can_leave(), "Geração permitiu fechar Projeto");
  }
  static bool done(ComicWorkspace &p) {
    if (p.job_)
      return false;
    check(p.preview_.get_sensitive(), p.status_.get_text());
    return true;
  }
  static fs::path output(ComicWorkspace &p) { return p.output_; }
  static ViewState context(MainWindow &w) { return w.capture_context(); }
  static void restore(MainWindow &w, const ViewState &v) {
    w.restore_context(v);
  }
  static void publication(MainWindow &w) {
    check(w.publication_page_ != nullptr, "Painel de publicação ausente");
    check(w.publication_page_->notebook_.get_n_pages() == 3,
          "Editor de comics continua dentro de Publicação");
  }
  static void reference(ComicWorkspace &p) {
    p.books_.set_active(0);
    p.zoom_.set_active_id("fit");
    p.inspector_.set_current_page(0);
  }
};
} // namespace inde::ui
Gtk::Button *find_button(Gtk::Widget &, const std::string &);
void capture_window(Gtk::Widget &, const fs::path &);
std::string file_hash(const fs::path &);
template <class T> T *comic_widget(Gtk::Widget &w) {
  if (auto *p = dynamic_cast<T *>(&w))
    return p;
  for (auto *c = w.get_first_child(); c; c = c->get_next_sibling())
    if (auto *p = comic_widget<T>(*c))
      return p;
  return nullptr;
}
int run_comic_window_smoke(const Glib::RefPtr<Gtk::Application> &app,
                           const fs::path &project, const fs::path &picture) {
  using Probe = inde::ui::ComicWorkspaceProbe;
  std::unique_ptr<inde::ui::MainWindow> window;
  inde::ui::WritingWorkspace *writing{};
  Gtk::TextView *editor{};
  Glib::RefPtr<Gtk::TextBuffer> buffer;
  inde::ui::ComicWorkspace *comic{};
  std::string source_text, writing_view, hash, id, draft;
  inde::ui::ViewState comic_context;
  int ticks = 0, stage = 0, frames = 0, result = 1;
  auto action = [&](const char *name) {
    auto a = window->lookup_action(name);
    if (!a)
      throw std::runtime_error(name);
    a->activate();
  };
  app->signal_activate().connect([&] {
    window = std::make_unique<inde::ui::MainWindow>(project);
    app->add_window(*window);
    window->present();
    Glib::signal_timeout().connect(
        [&] {
          try {
            if (++ticks > 180)
              throw std::runtime_error("Timeout do workspace Comic");
            if (stage == 0) {
              Probe::routes(*window);
              action("show-writing");
              writing = comic_widget<inde::ui::WritingWorkspace>(*window);
              Probe::check(writing, "Escrita ausente");
              std::ifstream f(project.parent_path() / "retrato.json");
              const auto snapshot = inde::persistence::json::parse(
                  std::string{std::istreambuf_iterator<char>(f), {}});
              id = snapshot.at("units")
                       .as_array()
                       .back()
                       .at("documents")
                       .as_array()
                       .front()
                       .at("id")
                       .as_string();
              writing->reveal_document(id);
              editor = comic_widget<Gtk::TextView>(*writing);
              Probe::check(editor, "Editor ausente");
              buffer = editor->get_buffer();
              source_text = buffer->get_text();
              buffer->select_range(buffer->get_iter_at_offset(2),
                                   buffer->get_iter_at_offset(8));
              writing_view = inde::ui::serialize_view(writing->capture_view());
              hash = file_hash(project / "data/project.sqlite3");
              stage = 10;
              return true;
            }
            if (stage == 10) {
              if (!writing->publication_ready())
                return true;
              action("show-comic");
              comic = &Probe::workspace(*window);
              Probe::navigation(*window);
              Probe::edit(*comic);
              draft = Probe::text(*comic);
              comic_context = Probe::context(*window);
              Probe::preedit(*comic, "仮");
              action("show-writing");
              Probe::navigation(*window);
              Probe::preedit(*comic, "");
              stage = 11;
              return true;
            }
            if (stage == 11) {
              if (comic->composing_input())
                return true;
              Probe::choose(*window, false);
              Probe::check(writing->current_document_id() == id &&
                               editor->get_buffer() == buffer &&
                               buffer->get_text().raw() == source_text,
                           "Comic mudou o Documento");
              Probe::choose(*window, true);
              Probe::verify_draft(*comic, draft);
              Probe::restore(*window, comic_context);
              Probe::verify_draft(*comic, draft);
              window->close();
              Probe::check(window->get_visible(),
                           "Fechou com rascunho da comic");
              stage = 12;
              return true;
            }
            if (stage == 12) {
              if (comic->composing_input())
                return true;
              Probe::save_and_generate(*comic);
              stage = 1;
              return true;
            }
            if (stage == 1) {
              if (!Probe::done(*comic))
                return true;
              const auto output = Probe::output(*comic);
              Probe::check(
                  fs::is_regular_file(output / "a-passagem/texto.html"),
                  "Leitor ausente");
              std::ofstream(picture.parent_path() / "comic-saida.txt")
                  << output.string();
              action("web-publication");
              stage = 2;
              return true;
            }
            if (stage == 2) {
              Probe::publication(*window);
              auto *back = find_button(*window, "Voltar ao INDE");
              Probe::check(back, "Retorno ausente");
              g_signal_emit_by_name(back->gobj(), "clicked");
              stage = 3;
              return true;
            }
            if (stage == 3) {
              Probe::navigation(*window);
              Probe::reference(*comic);
              stage = 4;
              return true;
            }
            if (stage == 4) {
              if (++frames < 3)
                return true;
              Probe::check(comic->get_width() > 1000,
                           "Comic ficou com largura de diálogo");
              capture_window(*window, picture);
              action("show-writing");
              frames = 0;
              stage = 5;
              return true;
            }
            if (++frames < 3)
              return true;
            Probe::check(
                writing->current_document_id() == id &&
                    editor->get_buffer() == buffer &&
                    buffer->get_text().raw() == source_text &&
                    inde::ui::serialize_view(writing->capture_view()) ==
                        writing_view &&
                    file_hash(project / "data/project.sqlite3") == hash,
                "Mudança de espaços alterou a sessão autoral");
            capture_window(*window, picture.parent_path() /
                                        "escrita-retorno-comic02.png");
            result = 0;
            std::cout
                << "COMIC-02 GTK: barra exata, Criar, workspace principal, "
                   "rascunho, contexto, zoom, desfazer/refazer, publicação e "
                   "mesma sessão de Escrita OK\n";
            window->close();
            app->quit();
            return false;
          } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            window->hide();
            app->quit();
            return false;
          }
        },
        250);
  });
  app->run();
  window.reset();
  return result;
}
