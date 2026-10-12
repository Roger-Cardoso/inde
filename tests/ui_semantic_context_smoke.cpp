#include "../src/ui/rendering_policy.hpp"
#include "inde/application/text_semantic_service.hpp"
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
class Window final : public Gtk::ApplicationWindow {
public:
  Window(const std::filesystem::path &fixture, int &result)
      : writing_(service_), result_(result) {
    add_css_class("inde-window");
    service_.open(fixture);
    axis_ = service_.planning().create_time_axis("QA eixo da anotação");
    p10_ = service_.planning().create_time_point(axis_.id, 10, "QA início");
    p15_ = service_.planning().create_time_point(axis_.id, 15, "QA meio");
    p20_ = service_.planning().create_time_point(axis_.id, 20, "QA fim");
    auto d =
        service_.writing().create_document("QA contexto global e temporal");
    d.content = "Água 🙂 e narração autoral.";
    d.perspective = "Perspectiva livre, não convertida";
    d = application::TextSemanticService::create(
        d,
        *project::find_annotation_type(service_.writing().semantic_types(),
                                       "inde.semantic.narration", 1),
        0, 6, {});
    d = service_.writing().update_document(d);
    id_ = d.id;
    local_ = d.semantics.annotations.front().id;
    baseline_ = d;
    host_.set_content(writing_);
    set_child(host_);
    set_default_size(1280, 900);
    writing_.install_actions(*this);
    writing_.refresh();
    writing_.reveal_document(id_);
    editor_ =
        find<Gtk::TextView>(writing_, [](auto &v) { return v.get_editable(); });
    require(editor_, "No editor");
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL semantic context phase=" << phase_ << ": "
                      << e.what() << std::endl;
            hide();
            return false;
          }
        },
        100);
  }
  ~Window() override { timer_.disconnect(); }

private:
  project::Document saved() {
    writing_.flush_changes();
    return *service_.writing().document(id_);
  }
  void choose(const std::string &id) {
    auto *c = find<Gtk::ComboBoxText>(host_, [&](auto &c) {
      if(combo_has_id(c,id)) return true;
      return false;
    });
    require(c, "No combo ID " + id);
    require(c->set_active_id(id), "Choice failed");
  }
  void open(const std::string &id) {
    auto *card = find<Gtk::Button>(writing_, [&](auto &b) {
      return b.has_css_class("entity-card") &&
             b.get_tooltip_text().raw().find(id) != std::string::npos;
    });
    require(card, "No annotation card");
    g_signal_emit_by_name(card->gobj(), "clicked");
  }
  ui::IncrementalSelector &selector(const std::string &placeholder) {
    auto *s = find<ui::IncrementalSelector>(host_, [&](auto &v) {
      return find<Gtk::SearchEntry>(v, [&](auto &e) {
        return e.get_placeholder_text().raw() == placeholder;
      });
    });
    require(s, "No point selector " + placeholder);
    return *s;
  }
  void point(const std::string &placeholder, const std::string &label) {
    auto &s = selector(placeholder);
    auto *b = find<Gtk::Button>(s, [&](auto &b) {
      return find<Gtk::Label>(
          b, [&](auto &v) { return v.get_text().raw() == label; });
    });
    require(b, "No canonical point result");
    g_signal_emit_by_name(b->gobj(), "clicked");
  }
  void period(bool inverted = false) {
    choose(axis_.id);
    auto *p = find<Gtk::CheckButton>(host_, [](auto &v) {
      return v.get_label() == "Vincular um período fechado, com início e fim";
    });
    require(p, "No period choice");
    p->set_active(true);
    point("Pesquisar ponto inicial / pontual",
          inverted ? p20_.label : p10_.label);
    point("Pesquisar ponto final (somente período)",
          inverted ? p10_.label : p20_.label);
    require(find<Gtk::Label>(host_,
                             [](auto &v) {
                               return v.get_text().raw().starts_with(
                                   "Prévia: Eixo:");
                             }),
            "No temporal preview");
  }
  bool step() {
    require(std::chrono::steady_clock::now() - started_ <
                std::chrono::seconds(60),
            "Timeout");
    if (phase_ == 0) {
      if (!get_mapped())
        return true;
      editor_->get_buffer()->place_cursor(editor_->get_buffer()->end());
      click(writing_, "Marcações do texto");
      click(host_, "Ponto de vista do documento");
      click(host_, "Cancelar");
      require(saved().semantics.annotations.size() == 1,
              "Canceled global applied");
      click(host_, "Ponto de vista do documento");
      choose("third_person");
      choose("external");
      click(host_, "Salvar anotação");
      auto d = saved();
      require(d.semantics.annotations.size() == 2 && d.anchors.size() == 1 &&
                  d.perspective == baseline_.perspective,
              "Global created fake anchor or erased legacy");
      global_ = d.semantics.annotations.back().id;
      open(global_);
      require(!find<Gtk::Button>(host_,
                                 [](auto &b) {
                                   return b.get_label() ==
                                          "Reancorar na seleção atual";
                                 }),
              "Global reanchor shown");
      click(host_, "Voltar às camadas");
      open(local_);
      click(host_, "Vincular tempo da história");
      period();
      click(host_, "Cancelar");
      require(saved().semantics.annotations.front().fictional_time.empty(),
              "Cancel bound time");
      open(local_);
      click(host_, "Vincular tempo da história");
      period(true);
      click(host_, "Confirmar vínculo temporal");
      require(saved().semantics.annotations.front().fictional_time.empty(),
              "Inverted time bound");
      click(host_, "Fechar");
      click(writing_, "Marcações do texto");
      open(local_);
      click(host_, "Vincular tempo da história");
      period();
      std::cout << "SNAPSHOT: semantic-time\n" << std::flush;
      shot_ = std::chrono::steady_clock::now();
      ++phase_;
    } else if (phase_ == 1) {
      if (std::chrono::steady_clock::now() - shot_ < std::chrono::seconds(2))
        return true;
      click(host_, "Confirmar vínculo temporal");
      auto d = saved();
      require(d.semantics.annotations.front().fictional_time.size() == 1,
              "No time binding");
      click(host_, "Voltar ao texto");
      click(writing_, "⮐");
      require(saved().semantics.annotations.front().fictional_time.empty(),
              "Temporal undo");
      click(writing_, "⮑");
      require(saved().semantics.annotations.front().fictional_time.size() == 1,
              "Temporal redo");
      click(writing_, "Marcações do texto");
      choose("project");
      ++phase_;
    } else if (phase_ == 2) {
      // Project scope rebuild is deferred beyond ComboBox property
      // notification.
      if (!find<Gtk::ComboBoxText>(
              writing_, [](auto &c) { return c.get_active_id() == "project"; }))
        return true;
      auto *time = find<Gtk::Expander>(writing_, [](auto &e) {
        return e.get_label() == "Contexto no tempo da história (opcional)";
      });
      if (!time || !time->get_mapped())
        return true;
      time->set_expanded(true);
      choose(axis_.id);
      point("Pesquisar ponto ficcional do filtro", p15_.label);
      click(writing_, "Buscar camadas");
      require(find<Gtk::Label>(writing_,
                               [](auto &v) {
                                 return v.get_text().raw().find(
                                            "Consulta de anotações persistidas "
                                            "— 1 resultado(s)") !=
                                        std::string::npos;
                               }),
              "Temporal project filter mismatch");
      std::cout << "SNAPSHOT: semantic-time-uses\n" << std::flush;
      shot_ = std::chrono::steady_clock::now();
      ++phase_;
    } else {
      if (std::chrono::steady_clock::now() - shot_ < std::chrono::seconds(2))
        return true;
      for(const auto *name:{"Buscar camadas","Limpar filtros"}) {
        auto *button=find<Gtk::Button>(writing_,[&](auto &b){return b.get_label()==name;});
        graphene_rect_t bounds{};
        require(button && gtk_widget_compute_bounds(GTK_WIDGET(button->gobj()),GTK_WIDGET(gobj()),&bounds),"Cannot measure filter action");
        require(bounds.origin.y>=0 && bounds.origin.y+bounds.size.height<=get_height(),"Filter action outside physical viewport");
      }
      auto before = saved();
      writing_.reset();
      writing_.refresh();
      writing_.reveal_document(id_);
      require(project::same_document_state(saved(), before),
              "Reopen lost semantic context");
      writing_.reset();
      service_.writing().delete_document(id_);
      for (const auto &p : {p10_, p15_, p20_})
        service_.planning().delete_time_point(p.id);
      service_.planning().delete_time_axis(axis_.id);
      service_.close();
      result_ = 0;
      std::cout << "PASS: global UI without selection/fake anchor, cancel, "
                   "manual period preview/inversion, undo/redo, inclusive "
                   "project filter, reopen and teardown\n"
                << std::flush;
      hide();
      return false;
    }
    return true;
  }
  application::ProjectService service_;
  ui::WritingWorkspace writing_;
  ui::ModalOverlayHost host_;
  project::FictionalTimeAxis axis_;
  project::FictionalTimePoint p10_, p15_, p20_;
  project::Document baseline_;
  Gtk::TextView *editor_{};
  sigc::connection timer_;
  int &result_;
  int phase_{};
  std::string id_, local_, global_;
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
  auto app = Gtk::Application::create("io.github.inde.contextqa",
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
