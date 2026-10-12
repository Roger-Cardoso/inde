#include "../src/ui/rendering_policy.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/calendar_picker.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/temporal_library_page.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "ui/widget_probe.hpp"
#include <iostream>
using namespace inde;
using namespace inde::test::ui;
class Window final : public Gtk::ApplicationWindow {
public:
  Window(const std::filesystem::path &p, int &result, bool restart)
      : writing_(service_), page_(service_), result_(result),
        restart_(restart) {
    service_.open(p);
    if (restart_) {
      persistence::DocumentQuery q;
      q.search = "QA Texto Temporal";
      for (auto d : service_.writing().documents(q))
        if (d.title == "QA Texto Temporal")
          id_ = d.id;
      require(!id_.empty(), "Restart document missing");
      auto d = *service_.writing().document(id_);
      require(d.semantics.annotations.size() == 2, "Restart annotations");
      local_ = d.semantics.annotations[0].id;
      global_ = d.semantics.annotations[1].id;
      require(d.semantics.annotations[0].dates.size() == 1 &&
                  d.semantics.annotations[1].dates.size() == 1,
              "Restart dates");
      ref_ = service_.worldbuilding().temporal_reference(
          d.semantics.annotations[0].dates[0].reference_id);
      require(ref_.qualifier == "between" && ref_.upper &&
                  ref_.upper->year == 1991 &&
                  ref_.upper->clock == std::vector<int>{12, 30},
              "Restart interval lost");
      require(service_.worldbuilding().continuity_notices(d).size() == 1,
              "Restart advisory lost");
    } else
      setup();
    add_css_class("inde-window");
    host_.set_content(writing_);
    set_child(host_);
    set_default_size(1280, 900);
    writing_.install_actions(*this);
    writing_.refresh();
    writing_.reveal_document(id_);
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL temporal context phase=" << phase_ << ": "
                      << e.what() << std::endl;
            hide();
            return false;
          }
        },
        800);
  }
  ~Window() override { timer_.disconnect(); }

private:
  application::ProjectService service_;
  ui::WritingWorkspace writing_;
  ui::TemporalLibraryPage page_;
  ui::ModalOverlayHost host_;
  int &result_;
  bool restart_;
  int phase_{};
  bool invalid_checked_{};
  sigc::connection timer_;
  std::string id_, local_, global_;
  project::TemporalReference ref_;
  project::FictionalTimeAxis axis_;
  void setup() {
    auto &w = service_.worldbuilding();
    auto c = w.create_calendar(project::calendar_template("gregorian"));
    axis_ = service_.planning().create_time_axis("QA Texto Eixo");
    ref_.calendar_id = c.id;
    ref_.axis_id = axis_.id;
    ref_.year = 1990;
    ref_.month = 1;
    ref_.day = 1;
    ref_.label = "QA Cena antiga";
    ref_ = w.save_temporal_reference(ref_);
    auto birth = ref_;
    birth.id.clear();
    birth.year = 2000;
    birth.label = "QA Nascimento";
    birth = w.save_temporal_reference(birth);
    auto t = service_.narrative().create_entity_type("qa-text-person",
                                                     "Pessoa do teste");
    auto e = service_.narrative().create_entity(t.id, "Lia temporal");
    project::TemporalAttachment a;
    a.reference_id = birth.id;
    a.kind = "entity";
    a.target_id = e.id;
    a.role = "Nascimento declarado";
    a.meaning = "birth";
    w.attach_date(a, birth, w.temporal_target("entity", e.id));
    auto d = service_.writing().create_document("QA Texto Temporal");
    d.content = "Lia chegou ao passado. Água 🙂 e narração autoral.";
    using S = application::TextSemanticService;
    using K = project::SemanticFieldKind;
    auto types = project::builtin_annotation_types();
    d = S::create(d,
                  *project::find_annotation_type(
                      types, "inde.semantic.temporal-context", 1),
                  0, 3,
                  {{"participants", {K::Entity, {e.id}}},
                   {"exception", {K::Enum, {"time_travel"}}}});
    local_ = d.semantics.annotations.back().id;
    d = S::create_document_perspective(
        d, {{"person", {K::Enum, {"third_person"}}},
            {"focalization", {K::Enum, {"external"}}}});
    global_ = d.semantics.annotations.back().id;
    id_ = service_.writing().update_document(d).id;
  }
  project::Document saved() {
    writing_.flush_changes();
    return *service_.writing().document(id_);
  }
  void choose(const std::string &id) {
    auto *card = find<Gtk::Box>(
        host_, [](auto &b) { return b.has_css_class("modal-card"); });
    require(card, "No active modal card");
    auto *c = find<Gtk::ComboBoxText>(*card, [&](auto &c) {
      if(combo_has_id(c,id)) return true;
      return false;
    });
    require(c, "No combo ID " + id);
    require(c->set_active_id(id), "Choice failed");
  }
  void open(const std::string &id) {
    auto *b = find<Gtk::Button>(writing_, [&](auto &b) {
      return b.has_css_class("entity-card") &&
             b.get_tooltip_text().raw().find(id) != std::string::npos;
    });
    require(b, "Annotation card missing");
    g_signal_emit_by_name(b->gobj(), "clicked");
  }
  template <class T> T &field(const std::string &name) {
    auto *l = find<Gtk::Label>(page_, [&](auto &l) {
      return l.get_text().raw() == name && l.get_mnemonic_widget();
    });
    require(l, "Missing field " + name);
    auto *v = dynamic_cast<T *>(l->get_mnemonic_widget());
    require(v, "Wrong field type");
    return *v;
  }
  void snap(const std::string &name) {
    std::cout << "SNAPSHOT: " << name << std::endl;
  }
  void notices() {
    click(writing_, "Marcações do texto");
    click(host_, "Consultar avisos temporais do Documento");
  }
  bool step() {
    std::cout << "STEP: " << phase_ << std::endl;
    if (!get_mapped())
      return true;
    if (restart_) {
      if (phase_ == 0) {
        notices();
        snap("context-restart-warning");
      }
      if (phase_ == 1) {
        require(service_.worldbuilding().text_date_uses(ref_.id).size() == 2,
                "Restart projection lost");
        result_ = 0;
        std::cout << "PASS temporal context restart\n";
        hide();
        return false;
      }
      ++phase_;
      return true;
    }
    switch (phase_) {
    case 0:
      if (!invalid_checked_) {
        click(writing_, "Marcações do texto");
        open(local_);
        click(host_, "Vincular data / horário…");
        click(host_, "Vincular data ao texto");
        invalid_checked_ = true;
        return true; // validation retains/re-presents the draft after GTK response
      }
      require(saved().semantics.annotations[0].dates.empty(),
              "Incomplete picker applied");
      click(host_, "Cancelar");
      click(writing_, "Marcações do texto");
      open(local_);
      click(host_, "Vincular data / horário…");
      choose(axis_.id);
      click(host_, "Cancelar");
      require(saved().semantics.annotations[0].dates.empty(), "Cancel applied");
      click(writing_, "Marcações do texto");
      open(local_);
      click(host_, "Vincular data / horário…");
      choose(axis_.id);
      choose(ref_.id);
      choose("scene");
      snap("context-date-picker");
      break;
    case 1:
      click(host_, "Vincular data ao texto");
      require(saved().semantics.annotations[0].dates.size() == 1,
              "Date not bound");
      click(writing_, "⮐");
      require(saved().semantics.annotations[0].dates.empty(), "Date undo");
      click(writing_, "⮑");
      require(saved().semantics.annotations[0].dates.size() == 1, "Date redo");
      notices();
      snap("context-advisory");
      break;
    case 2:
      click(host_, "Voltar à escrita");
      require(service_.worldbuilding().continuity_notices(saved()).size() == 1,
              "Warning missing or blocked save");
      click(writing_, "Marcações do texto");
      open(global_);
      click(host_, "Vincular data / horário…");
      choose(axis_.id);
      choose(ref_.id);
      choose("reference");
      snap("context-perspective-date");
      break;
    case 3: {
      click(host_, "Vincular data ao texto");
      auto d = saved();
      require(d.semantics.annotations[1].dates.size() == 1 &&
                  d.anchors.size() == 1,
              "Perspective date added fake anchor");
      require(d.content == "Lia chegou ao passado. Água 🙂 e narração autoral.",
              "Date changed prose");
      host_.set_content(page_);
      page_.refresh();
      click(page_, "Uso, mudanças e datas…");
      break;
    }
    case 4:
      if (!find<Gtk::Button>(page_, [&](auto &b) {
            return b.get_label().raw() == "Datas e horários — " + axis_.name;
          }))
        return true;
      click(page_, "Datas e horários — " + axis_.name);
      break;
    case 5: {
      auto *b = find<Gtk::Button>(page_, [&](auto &b) {
        return b.get_label().raw().starts_with(ref_.label);
      });
      if (!b)
        return true; // wait for the deferred page rebuild, not a fixed delay
      g_signal_emit_by_name(b->gobj(), "clicked");
      break;
    }
    case 6: {
      auto *exp = find<Gtk::Expander>(page_, [](auto &e) {
        return e.get_label() == "Precisão, intervalo e incerteza";
      });
      if (!exp)
        return true;
      exp->set_expanded(true);
      field<Gtk::ComboBoxText>("Quanto sabemos sobre a data").set_active_id("between");
      auto *picker =
          find<ui::CalendarPicker>(page_, [](auto &) { return true; });
      require(picker, "Graphic picker missing");
      click(*picker, "Editar fim");
      picker->show_month({1991, 1, 1});
      click(*picker, "1");
      auto *clock = find<Gtk::Expander>(*picker, [](auto &e) {
        return e.get_label().raw().starts_with("Horário do fim");
      });
      require(clock, "Upper clock missing");
      clock->set_expanded(true);
      auto *minute = find<Gtk::Entry>(*clock, [](auto &e) {
        return e.get_tooltip_text().raw().starts_with("0 a 59");
      });
      require(minute, "Minute missing");
      minute->set_text("30");
      snap("context-interval-edit");
      break;
    }
    case 7: {
      click(page_, "Salvar data e horário");
      require(service_.worldbuilding().temporal_reference(ref_.id).qualifier ==
                  "exact",
              "Unacknowledged edit applied");
      auto *ack = find<Gtk::CheckButton>(page_, [](auto &c) {
        return c.get_label() ==
               "Confirmo atualizar a data consultada por seus vínculos";
      });
      require(ack, "Shared impact missing");
      ack->set_active(true);
      click(page_, "Salvar data e horário");
      require(service_.worldbuilding().temporal_reference(ref_.id).qualifier ==
                  "exact",
              "Incomplete upper clock applied");
      auto *hour = find<Gtk::Entry>(page_, [](auto &e) {
        return e.get_tooltip_text().raw().starts_with("0 a 23");
      });
      require(hour, "Hour missing");
      hour->set_text("12");
      auto *exp = find<Gtk::Expander>(page_, [](auto &e) {
        return e.get_label() == "Documentos que consultam esta data";
      });
      require(exp, "Impact titles missing");
      exp->set_expanded(true);
      auto *scroll =
          find<Gtk::ScrolledWindow>(page_, [](auto &) { return true; });
      if (scroll)
        scroll->get_vadjustment()->set_value(
            scroll->get_vadjustment()->get_upper());
      snap("context-shared-impact");
      break;
    }
    case 8:
      click(page_, "Salvar data e horário");
      break;
    case 9:
      ref_ = service_.worldbuilding().temporal_reference(ref_.id);
      require(ref_.qualifier == "between" && ref_.upper->year == 1991 &&
                  ref_.upper->clock == std::vector<int>{12, 30},
              "Interval not saved");
      require(service_.worldbuilding().text_date_uses(ref_.id).size() == 2,
              "Projection count");
      host_.set_content(writing_);
      writing_.refresh();
      writing_.reveal_document(id_);
      notices();
      snap("context-interval-warning");
      break;
    case 10:
      click(host_, "Voltar à escrita");
      require(saved().semantics.annotations[0].dates.size() == 1,
              "Date lost after reference edit");
      result_ = 0;
      std::cout
          << "PASS temporal context: explicit choices/cancel, passage/global "
             "perspective, undo/redo, advisory/exception/save, shared-impact "
             "guard, intervals and unchanged prose\n";
      hide();
      return false;
    }
    ++phase_;
    return true;
  }
};
int main(int argc, char **argv) {
  if (argc != 3 || !std::getenv("INDE_CLIPBOARD_TEST_ISOLATED"))
    return 2;
  const std::string mode = argv[2];
  if (mode != "--allow-fixture-writes" && mode != "--verify-restart")
    return 2;
  ui::configure_rendering_backend();
  auto app = Gtk::Application::create("io.github.inde.temporalcontextqa",
                                      Gio::Application::Flags::NON_UNIQUE);
  int result = 1;
  Window *w{};
  app->signal_activate().connect([&] {
    ui::install_application_theme();
    w = new Window(argv[1], result, mode == "--verify-restart");
    app->add_window(*w);
    w->present();
  });
  app->run(1, argv);
  delete w;
  return result;
}
