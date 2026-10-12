#include "../src/ui/rendering_policy.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/ui_preferences.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "ui/widget_probe.hpp"
#include <iostream>
using namespace inde;
using namespace inde::test::ui;
class CompositionWindow final : public Gtk::ApplicationWindow {
public:
  CompositionWindow(const std::filesystem::path &path, bool restart,
                    int &result)
      : writing_(service_), restart_(restart), result_(result) {
    service_.open(path);
    prefs_ =
        ui::project_preference_group(service_.current()->manifest().project_id);
    if (!restart_) {
      auto d = service_.writing().create_document("QA — origem de Composição");
      d.content = "Água 🙂 conserva memória.\nSegundo parágrafo independente.";
      d.formatting = {{project::DocumentTextStyle::Bold, 0, 4, {}}};
      baseline_ = service_.writing().update_document(d);
      ui::set_ui_preference(prefs_, "qa-composition-source", d.id);
    } else
      baseline_ = *service_.writing().document(
          ui::ui_preference(prefs_, "qa-composition-source"));
    host_.set_content(writing_);
    set_child(host_);
    set_default_size(1000, 720);
    add_css_class("inde-window");
    writing_.install_actions(*this);
    writing_.refresh();
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL composition step=" << phase_ << ": " << e.what()
                      << std::endl;
            hide();
            return false;
          }
        },
        900);
  }
  ~CompositionWindow() override { timer_.disconnect(); }

private:
  application::ProjectService service_;
  ui::WritingWorkspace writing_;
  ui::ModalOverlayHost host_;
  bool restart_;
  int &result_;
  int phase_{};
  sigc::connection timer_;
  project::Document baseline_;
  std::string prefs_, id_, occurrence_;
  ui::CompositionPage &page() {
    auto *p = find<ui::CompositionPage>(writing_, [](auto &) { return true; });
    require(p, "Composition page missing");
    return *p;
  }
  Gtk::TextView &editor() {
    auto *p = find<Gtk::TextView>(
        page(), [](auto &v) { return v.get_mapped() && v.get_editable(); });
    require(p, "Editable block missing");
    return *p;
  }
  void occurrence(int n = 1) {
    auto *b = find<Gtk::Button>(page(), [&](auto &b) {
      return b.get_mapped() && find<Gtk::Label>(b, [&](auto &l) {
               return l.get_text().raw().starts_with(std::to_string(n) + ". ");
             });
    });
    require(b, "Occurrence missing");
    g_signal_emit_by_name(b->gobj(), "clicked");
  }
  void command(const std::string &name) {
    auto *m = find<Gtk::MenuButton>(page(), [](auto &m) {
      return m.get_tooltip_text() ==
             "Operações da Montagem de texto / uso do bloco atual";
    });
    require(m, "Composition menu missing");
    m->popup();
    auto *b = find<Gtk::Button>(*m, [&](auto &b) {
      return find<Gtk::Label>(
          b, [&](auto &l) { return l.get_text().raw() == name; });
    });
    require(b && b->get_sensitive(), "Command missing: " + name);
    g_signal_emit_by_name(b->gobj(), "clicked");
  }
  bool step() {
    std::cout << "STEP: " << phase_ << std::endl;
    if (restart_) {
      if (phase_ == 0)
        writing_.restore_view(ui::parse_view_state(
            ui::ui_preference(prefs_, "qa-composition-view")));
      else if (phase_ == 1) {
        require(pages(writing_, "compositions").get_visible_child_name() ==
                    "compositions",
                "Writing page not restored");
        require(pages(page(), "edit").get_visible_child_name() == "edit",
                "Block editor not restored");
        require(editor().get_buffer()->get_text().raw().ends_with(
                    " Rascunho recuperável."),
                "Draft lost at restart");
        require(find<Gtk::Label>(page(),
                                 [](auto &l) {
                                   return l.get_text().raw().find(
                                              "Rascunho recuperado") !=
                                          std::string::npos;
                                 }),
                "Recovery not explained");
        require(
            !find<Gtk::CheckButton>(page(),
                                    [](auto &b) {
                                      return b.get_label().raw().starts_with(
                                                 "Conferi a leitura") &&
                                             b.get_active();
                                    }),
            "Inherited output approval");
        std::cout << "SNAPSHOT: composition-recovered-draft" << std::endl;
      } else if (phase_ == 2) {
        click(page(), "Descartar rascunho");
        require(find<Gtk::Label>(host_,[](auto &l){return l.get_mapped() && l.get_text().raw().find("Apenas o rascunho")!=std::string::npos;}),"Discard confirmation missing");
        auto *modal=find<Gtk::Box>(host_,[](auto &b){return b.get_mapped() && b.has_css_class("modal-scrim");});require(modal,"Discard modal layer missing");
        click(*modal,"Descartar rascunho");
        require(!editor().get_buffer()->get_text().raw().ends_with(
                    " Rascunho recuperável."),
                "Discard failed");
        result_ = 0;
        std::cout << "PASS composition actual restart, navigation, recovered "
                     "unapplied draft and explicit discard"
                  << std::endl;
        hide();
        return false;
      }
      ++phase_;
      return true;
    }
    switch (phase_) {
    case 0:
      click(writing_, "Composições…");
      break;
    case 1: {
      auto *e = find<Gtk::Entry>(page(), [](auto &e) {
        return e.get_placeholder_text() == "Nome da nova Montagem de texto";
      });
      require(e, "Name entry missing");
      e->set_text("QA — montagem independente");
      click(page(), "Criar Montagem de texto");
      id_ = service_.compositions().list().front().id;
      break;
    }
    case 2:
      click(page(), "Importar Documento…");
      break;
    case 3: {
      auto *s =
          find<ui::IncrementalSelector>(page(), [](auto &) { return true; });
      require(s, "Source selector missing");
      s->set_selected(baseline_.id, baseline_.title);
      break;
    }
    case 4:
      click(page(), "Importar inteiro");
      std::cout << "SNAPSHOT: composition-order" << std::endl;
      break;
    case 5:
      occurrence();
      break;
    case 6:
      command("Reutilizar uso do bloco");
      break;
    case 7:
      occurrence(2);
      command("Manter esta versão neste uso");
      break;
    case 8:
      occurrence();
      editor().get_buffer()->insert_at_cursor(" Aplicado conscientemente.");
      break;
    case 9: {
      writing_.flush_changes();
      auto c = service_.compositions().load(id_);
      require(c.value.drafts.size() == 1, "Draft not persisted");
      require(c.value.blocks.project(c.value.profile)
                      .text.find("Aplicado conscientemente") ==
                  std::string::npos,
              "Draft changed reading silently");
      std::cout << "SNAPSHOT: composition-draft" << std::endl;
      break;
    }
    case 10:
      click(page(), "Usar esta versão na montagem");
      require(find<Gtk::Label>(host_,[](auto &l){return l.get_mapped() && l.get_text().raw().find("usos vivos")!=std::string::npos;}),"Live-use impact confirmation missing");
      click(host_,"Confirmar nova versão");
      break;
    case 11: {
      auto c = service_.compositions().load(id_);
      auto &p = c.value.blocks.placements();
      require(c.value.drafts.empty() && p.size() == 2, "Apply not persisted");
      require(c.value.blocks.revision(p[0]).text !=
                  c.value.blocks.revision(p[1]).text,
              "Pinned occurrence changed");
      command("Ver origem congelada");
      std::cout << "SNAPSHOT: composition-origin" << std::endl;
      break;
    }
    case 12:
      click(page(), "Voltar à ordem");
      click(page(), "Ler Montagem de texto");
      break;
    case 13: {
      require(count(page(),
                    [](auto &w) {
                      return GTK_IS_TEXT_VIEW(w.gobj()) && w.get_mapped();
                    }) == 1,
              "Competing text panels");
      require(!find<Gtk::TextView>(
                  page(),
                  [](auto &v) { return v.get_mapped() && v.get_editable(); }),
              "Projection editable");
      std::cout << "SNAPSHOT: composition-reading" << std::endl;
      break;
    }
    case 14:
      command("Preparar saída independente…");
      break;
    case 15: {
      auto *b = find<Gtk::Button>(page(), [](auto &b) {
        return b.get_label() == "Criar Documento independente";
      });
      require(b && !b->get_sensitive(), "Output not gated");
      auto *a = find<Gtk::CheckButton>(page(), [](auto &b) {
        return b.get_label().raw().starts_with("Conferi a leitura");
      });
      require(a && !a->get_active(), "Approval inherited");
      a->set_active(true);
      // Exercise a concurrent change after preview; confirmation must fail
      // closed.
      auto c = service_.compositions().load(id_);
      c.value.name += " — externa";
      service_.compositions().save(c);
      const auto count = service_.writing().document_count();
      click(page(), "Criar Documento independente");
      require(service_.writing().document_count() == count,
              "Stale preview created a Document");
      require(!a->get_active(), "Failed output retained approval");
      require(find<Gtk::Label>(page(),
                               [](auto &l) {
                                 return l.get_text().raw().find("mudou") !=
                                        std::string::npos;
                               }),
              "Stale output unexplained");
      break;
    }
    case 16:
      command("Recarregar do Projeto…");
      break;
    case 17:
      click(host_, "Descartar local e recarregar");
      break;
    case 18:
      command("Preparar saída independente…");
      std::cout << "SNAPSHOT: composition-output" << std::endl;
      break;
    case 19: {
      auto *a = find<Gtk::CheckButton>(page(), [](auto &b) {
        return b.get_label().raw().starts_with("Conferi a leitura");
      });
      require(a && !a->get_active(), "Reprepare retained approval");
      a->set_active(true);
      click(page(), "Criar Documento independente");
      break;
    }
    case 20: {
      require(pages(writing_, "editor").get_visible_child_name() == "editor",
              "Materialized Document not opened");
      require(project::same_document_state(
                  *service_.writing().document(baseline_.id), baseline_),
              "Original source changed");
      click(writing_, "← Voltar");
      break;
    }
    case 21:
      click(writing_, "Composições…");
      break;
    case 22: {
      auto *b = find<Gtk::Button>(page(), [](auto &b) {
        return b.get_mapped() && find<Gtk::Label>(b, [](auto &l) {
                 return l.get_text().raw().starts_with(
                     "QA — montagem independente");
               });
      });
      require(b, "Persisted composition missing from queue");
      g_signal_emit_by_name(b->gobj(), "clicked");
      occurrence();
      break;
    }
    case 23:
      editor().get_buffer()->insert_at_cursor(" Rascunho recuperável.");
      break;
    case 24:
      writing_.flush_changes();
      ui::set_ui_preference(prefs_, "qa-composition-view",
                            ui::serialize_view(writing_.capture_view()));
      result_ = 0;
      std::cout << "PASS composition production UI: independent import, "
                   "draft/apply, pinned versions, origin, projection, stale "
                   "output rejection and atomic materialization"
                << std::endl;
      hide();
      return false;
    }
    ++phase_;
    return true;
  }
};
int main(int argc, char **argv) {
  if (argc != 3 || (std::string(argv[2]) != "--allow-fixture-writes" &&
                    std::string(argv[2]) != "--verify-restart"))
    return 2;
  auto app = Gtk::Application::create("io.github.inde.composition-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  ui::configure_rendering_backend();
  int result = 1;
  CompositionWindow *window{};
  app->signal_activate().connect([&] {
    ui::install_application_theme();
    window = new CompositionWindow(
        argv[1], std::string(argv[2]) == "--verify-restart", result);
    app->add_window(*window);
    window->present();
  });
  char *args[] = {argv[0], nullptr};
  app->run(1, args);
  delete window;
  return result;
}
