#include "../src/ui/rendering_policy.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/narrative_structure_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/temporal_context.hpp"
#include "inde/ui/temporal_reference_editor.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "ui/widget_probe.hpp"
#include <iostream>
using namespace inde;
using namespace inde::test::ui;
class Window final : public Gtk::ApplicationWindow {
public:
  Window(const std::filesystem::path &path, int &result, bool restart)
      : writing_(service_), planning_(service_), structure_(service_),
        result_(result), restart_(restart) {
    service_.open(path);
    if (restart_) {
      persistence::DocumentQuery q;
      q.search = "QA Fluxo temporal texto";
      for (const auto &d : service_.writing().documents(q))
        if (d.title == q.search)
          document_ = d.id;
      require(!document_.empty(), "Restart document missing");
      const auto d = *service_.writing().document(document_);
      require(d.semantics.annotations.front().dates.size() == 1,
              "Restart binding");
      const auto r = service_.worldbuilding().temporal_reference(
          d.semantics.annotations.front().dates.front().reference_id);
      require(r.qualifier == "between" && r.upper && r.upper->day == 18 &&
                  r.clock == std::vector<int>({12, 30}),
              "Restart date precision");
      annotation_ = d.semantics.annotations.front().id;
      axis_ = r.axis_id;
      reference_ = r.id;
      calendar_ = r.calendar_id;
    } else
      setup();
    add_css_class("inde-window");
    host_.set_content(writing_);
    set_child(host_);
    set_default_size(1000, 720);
    writing_.install_actions(*this);
    planning_.install_actions(*this);
    writing_.refresh();
    writing_.reveal_document(document_);
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL temporal workflow phase=" << phase_ << ": "
                      << e.what() << std::endl;
            hide();
            return false;
          }
        },
        2500);
  }
  ~Window() override { timer_.disconnect(); }

private:
  application::ProjectService service_;
  ui::WritingWorkspace writing_;
  ui::PlanningWorkspace planning_;
  ui::NarrativeStructureWorkspace structure_;
  ui::ModalOverlayHost host_;
  int &result_;
  bool restart_;
  int phase_{};
  bool expanded_scrolled_{}, month_scrolled_{};
  sigc::connection timer_;
  std::string document_, annotation_, axis_, calendar_, reference_, entity_,
      relation_, unit_, line_, structure_id_, point_;
  void setup() {
    auto &w = service_.worldbuilding();
    auto c = w.create_calendar(project::calendar_template("gregorian"));
    calendar_ = c.id;
    auto a = service_.planning().create_time_axis("QA Fluxo temporal eixo");
    axis_ = a.id;
    auto base = *w.assign_calendar(a.id, c.id, 0, {});
    auto julian = w.create_calendar(project::calendar_template("julian"));
    auto cut = project::calendar_ordinal(c, {2000, 1, 15});
    auto date = project::terrestrial_date(
        julian, project::terrestrial_day(c, {2000, 1, 15}));
    w.adopt_calendar({{},
                      a.id,
                      julian.id,
                      cut,
                      cut - project::calendar_ordinal(julian, date),
                      "coexist"},
                     base, {});
    point_ = service_.planning()
                 .create_time_point(a.id,
                                    project::calendar_ordinal(c, {2000, 1, 10}),
                                    "QA Marco visual")
                 .id;
    auto et = service_.narrative().create_entity_type("qa-workflow-person",
                                                      "QA Fluxo pessoa");
    auto x = service_.narrative().create_entity(et.id, "QA Fluxo Lia"),
         y = service_.narrative().create_entity(et.id, "QA Fluxo Rui");
    entity_ = x.id;
    auto rt = service_.narrative().create_relation_type(
        "qa-workflow-rel", "QA Fluxo relação", "QA Fluxo inversa",
        project::RelationDirectionality::Directed);
    relation_ = service_.narrative().create_relation(rt.id, x.id, y.id).id;
    auto ip =
        service_.create_intellectual_property("QA Fluxo universo", "", "");
    auto work =
        service_.create_work(ip.id, "QA Fluxo obra", "", "", "pt-BR", "draft");
    auto ns =
        service_.structures().create_narrative(work.id, "QA Fluxo estrutura");
    structure_id_ = ns.id;
    line_ = service_.structures().create_line(ns.id, "QA Fluxo linha", "").id;
    unit_ = service_.structures()
                .create_unit(ns.id, "1", "QA Fluxo unidade", "", "", "")
                .id;
    auto d = service_.writing().create_document("QA Fluxo temporal texto");
    d.content = "Lia escreve. Água 🙂 e tempo autoral.";
    auto type =
        *project::find_annotation_type(project::builtin_annotation_types(),
                                       "inde.semantic.temporal-context", 1);
    d = application::TextSemanticService::create(d, type, 0, 3, {});
    annotation_ = d.semantics.annotations.front().id;
    document_ = service_.writing().update_document(d).id;
  }
  Gtk::Box &modal() {
    auto *box = find<Gtk::Box>(host_, [](auto &b) {
      return b.get_mapped() && b.has_css_class("modal-card");
    });
    require(box, "Modal absent");
    return *box;
  }
  template <class T> T &field(const std::string &name) {
    auto *l = find<Gtk::Label>(modal(), [&](auto &l) {
      return l.get_text().raw() == name && l.get_mnemonic_widget();
    });
    require(l, "Field missing: " + name);
    auto *v = dynamic_cast<T *>(l->get_mnemonic_widget());
    require(v, "Field type: " + name);
    return *v;
  }
  void choose(const std::string &id) {
    auto *c = find<Gtk::ComboBoxText>(modal(), [&](auto &c) {
      if(combo_has_id(c,id)) return true;
      return false;
    });
    require(c && c->set_active_id(id), "Choice absent: " + id);
  }
  void open_dates() {
    click(writing_, "Marcações do texto");
    auto *b = find<Gtk::Button>(writing_, [&](auto &b) {
      return b.has_css_class("entity-card") &&
             b.get_tooltip_text().raw().find(annotation_) != std::string::npos;
    });
    require(b, "Annotation card");
    g_signal_emit_by_name(b->gobj(), "clicked");
    click(host_, "Vincular data / horário…");
  }
  void create_draft(bool writing = true) {
    auto *create = find<Gtk::CheckButton>(modal(), [](auto &b) {
      return b.get_label() == "Criar nova data ou intervalo aqui";
    });
    require(create, "Create option");
    create->set_active(true);
    choose(axis_);
    field<Gtk::ComboBoxText>("Calendário da data").set_active_id(calendar_);
    field<Gtk::Entry>("Nome da data ou período")
        .set_text("QA Fluxo cena criada na escrita");
    auto *precision = find<Gtk::Expander>(modal(), [](auto &e) {
      return e.get_label() == "Precisão e incerteza";
    });
    require(precision, "Precision expander");
    precision->set_expanded(true);
    field<Gtk::ComboBoxText>("Quanto sabemos sobre a data").set_active_id("between");
    auto *picker =
        find<ui::CalendarPicker>(modal(), [](auto &) { return true; });
    require(picker, "Graphic date picker");
    picker->show_month({2000, 1, 1});
    click(*picker, "10");
    click(*picker, "18");
    click(*picker, "Editar início");
    auto *clock = find<Gtk::Expander>(*picker, [](auto &e) {
      return e.get_label().raw().starts_with("Adicionar horário");
    });
    require(clock, "Clock expander");
    clock->set_expanded(true);
    auto *hour = find<Gtk::Entry>(*clock, [](auto &e) {
      return e.get_tooltip_text().raw().starts_with("0 a 23");
    });
    require(hour, "Hour");
    hour->set_text("12");
    auto *minute = find<Gtk::Entry>(*clock, [](auto &e) {
      return e.get_tooltip_text().raw().starts_with("0 a 59");
    });
    require(minute, "Minute");
    minute->set_text("30");
    if (writing)
      choose("scene");
  }
  project::Document saved() {
    writing_.flush_changes();
    return *service_.writing().document(document_);
  }
  void snap(const std::string &name) {
    std::cout << "SNAPSHOT: " << name << std::endl;
  }
  void scroll_modal(bool bottom) {
    auto *scroll = find<Gtk::ScrolledWindow>(modal(), [](auto &w) {
      return w.get_mapped() && w.get_vadjustment()->get_upper() >
                                   w.get_vadjustment()->get_page_size();
    });
    require(scroll, "Scrollable modal content");
    auto adjustment = scroll->get_vadjustment();
    adjustment->set_value(
        bottom ? adjustment->get_upper() - adjustment->get_page_size() : 0);
    Glib::signal_timeout().connect_once(
        sigc::track_obj(
            [adjustment, bottom] {
              adjustment->set_value(bottom ? adjustment->get_upper() -
                                                 adjustment->get_page_size()
                                           : 0);
            },
            *scroll),
        150);
  }
  void footer(const std::string &label) {
    graphene_rect_t card{};
    require(gtk_widget_compute_bounds(GTK_WIDGET(modal().gobj()),
                                      GTK_WIDGET(gobj()), &card) &&
                card.origin.x >= 0 &&
                card.origin.x + card.size.width <= get_allocated_width(),
            "Modal exceeds window width");
    auto *b = find<Gtk::Button>(
        modal(), [&](auto &b) { return b.get_label().raw() == label; });
    require(b, "Footer");
    graphene_rect_t rect{};
    require(gtk_widget_compute_bounds(GTK_WIDGET(b->gobj()), GTK_WIDGET(gobj()),
                                      &rect) &&
                rect.origin.y >= 0 &&
                rect.origin.y + rect.size.height <= get_allocated_height(),
            "Footer clipped");
  }
  void attach_existing(const std::string &role) {
    field<Gtk::ComboBoxText>("Linha do tempo").set_active_id(axis_);
    choose(reference_);
    field<Gtk::Entry>("Significado da data").set_text(role);
  }
  bool step() {
    if (!get_mapped())
      return true;
    std::cout << "STEP " << phase_ << std::endl;
    if (restart_) {
      if (phase_ == 0) {
        open_dates();
        choose(axis_);
        choose(reference_);
        snap("workflow-restart-reference");
        ++phase_;
        return true;
      }
      click(host_, "Cancelar");
      result_ = 0;
      std::cout << "PASS temporal workflow restart\n";
      hide();
      return false;
    }
    switch (phase_) {
    case 0:
      open_dates();
      create_draft();
      snap("workflow-writing-expanded");
      break;
    case 1: {
      footer("Vincular data ao texto");
      if (!expanded_scrolled_) {
        scroll_modal(true);
        snap("workflow-writing-expanded-scrolled");
        expanded_scrolled_ = true;
        return true;
      }
      auto *note = find<Gtk::Entry>(modal(), [](auto &e) {
        return GTK_IS_ENTRY(e.gobj()) &&
               e.get_placeholder_text() == "Explicação autoral (opcional)";
      });
      require(note, "Last draft field");
      graphene_rect_t entry{}, action{};
      auto *confirm = find<Gtk::Button>(modal(), [](auto &b) {
        return b.get_label() == "Vincular data ao texto";
      });
      require(confirm &&
                  gtk_widget_compute_bounds(GTK_WIDGET(note->gobj()),
                                            GTK_WIDGET(gobj()), &entry) &&
                  gtk_widget_compute_bounds(GTK_WIDGET(confirm->gobj()),
                                            GTK_WIDGET(gobj()), &action) &&
                  entry.origin.y + entry.size.height <= action.origin.y - 14,
              "Last draft field inaccessible below footer");
      scroll_modal(false);
      auto *exp = find<Gtk::Expander>(modal(), [](auto &e) {
        return e.get_label().raw().find("— alterar…") != std::string::npos;
      });
      require(exp, "Compact summary absent");
      exp->set_expanded(false);
      auto *precision = find<Gtk::Expander>(modal(), [](auto &e) {
        return e.get_label() == "Precisão e incerteza";
      });
      require(precision, "Precision expander");
      precision->set_expanded(false);
      snap("workflow-writing-compact");
      break;
    }
    case 2:
      click(host_, "Cancelar");
      require(service_.worldbuilding().temporal_references(axis_).empty(),
              "Cancel created orphan");
      open_dates();
      create_draft();
      break;
    case 3:
      click(host_, "Vincular data ao texto");
      {
        auto d = saved();
        require(d.semantics.annotations.front().dates.size() == 1,
                "Creation not bound");
        reference_ = d.semantics.annotations.front().dates.front().reference_id;
        require(service_.worldbuilding().temporal_references(axis_).size() == 1,
                "Reference count");
        click(writing_, "⮐");
        require(saved().semantics.annotations.front().dates.empty(),
                "Atomic undo binding");
        require(service_.worldbuilding().temporal_references(axis_).size() == 1,
                "Undo silently erased reference");
        click(writing_, "⮑");
        require(saved().semantics.annotations.front().dates.size() == 1,
                "Atomic redo");
        const auto old_binding =
            saved().semantics.annotations.front().dates.front().id;
        const auto shared =
            service_.worldbuilding().temporal_reference(reference_);
        open_dates();
        auto *replace = find<Gtk::CheckButton>(modal(), [](auto &b) {
          return b.get_label() ==
                 "Usar outra data no lugar de um vínculo deste trecho";
        });
        require(replace, "Replace passage binding option");
        replace->set_active(true);
        choose(old_binding);
        choose(axis_);
        choose(reference_);
        choose("memory");
        click(host_, "Vincular data ao texto");
        auto replacement = saved();
        require(
            replacement.semantics.annotations.front().dates.size() == 1 &&
                replacement.semantics.annotations.front().dates.front().role ==
                    "memory",
            "Replacement duplicated binding");
        require(service_.worldbuilding().temporal_reference(reference_) ==
                    shared,
                "Replacement edited shared reference");
        click(writing_, "⮐");
        require(saved().semantics.annotations.front().dates.front().id ==
                    old_binding,
                "Replacement undo lost old binding identity");
        auto *text = find<Gtk::TextView>(
            writing_, [](auto &v) { return v.get_editable(); });
        require(text, "Editor");
        text->get_buffer()->insert(text->get_buffer()->end(),
                                   " Próximo autosave.");
        require(saved().content.ends_with(" Próximo autosave."),
                "Worker baseline after compound commit");
        open_dates();
        create_draft();
        auto external = *service_.writing().document(document_);
        const auto length = project::utf8_character_count(external.content);
        external =
            application::replace_selection({external, {length, length}}, length,
                                           length, " Alteração externa.")
                .document;
        (void)service_.writing().update_document(external);
        click(host_, "Vincular data ao texto");
        break;
      }
    case 4:
      require(service_.worldbuilding().temporal_references(axis_).size() == 1,
              "Stale document left orphan");
      snap("workflow-stale-keeps-draft");
      break;
    case 5:
      click(host_, "Cancelar");
      writing_.refresh();
      writing_.reveal_document(document_);
      host_.set_content(planning_);
      planning_.refresh();
      planning_.reveal_entity(entity_);
      break;
    case 6: {
      auto *b = find<Gtk::Button>(planning_, [](auto &b) {
        return b.get_label().raw().starts_with("Datas e horários (");
      });
      require(b, "Entity detail shortcut");
      g_signal_emit_by_name(b->gobj(), "clicked");
      attach_existing("Cena da personagem");
      snap("workflow-entity-context");
      break;
    }
    case 7:
      footer("Vincular data");
      click(host_, "Vincular data");
      require(service_.worldbuilding()
                      .target_dates(service_.worldbuilding().temporal_target(
                          "entity", entity_))
                      .size() == 1,
              "Entity date");
      ui::show_temporal_context(*this, service_, "relation", relation_);
      create_draft(false);
      field<Gtk::Entry>("Nome da data ou período")
          .set_text("QA Fluxo relação contextual");
      field<Gtk::Entry>("Significado da data").set_text("Período da relação");
      break;
    case 8:
      click(host_, "Vincular data");
      require(service_.worldbuilding()
                      .target_dates(service_.worldbuilding().temporal_target(
                          "relation", relation_))
                      .size() == 1,
              "Relation date");
      host_.set_content(structure_);
      structure_.refresh();
      break;
    case 9: {
      auto *combo =
          find<Gtk::ComboBoxText>(structure_, [](auto &) { return true; });
      require(combo, "Work combo");
      for (const auto &work : service_.catalog().works)
        if (work.title == "QA Fluxo obra")
          combo->set_active_id(work.id);
      auto *row = find<Gtk::ListBoxRow>(structure_, [](auto &r) {
        return find<Gtk::Label>(r, [](auto &l) {
          return l.get_text().raw().starts_with("QA Fluxo estrutura");
        });
      });
      require(row, "Structure row");
      auto *list = dynamic_cast<Gtk::ListBox *>(row->get_parent());
      require(list, "Structure list");
      g_signal_emit_by_name(list->gobj(), "row-activated", row->gobj());
      break;
    }
    case 10: {
      pages(structure_,"lines").set_visible_child("lines");
      auto *row = find<Gtk::ListBoxRow>(structure_, [](auto &r) {
        return find<Gtk::Label>(
            r, [](auto &l) { return l.get_text() == "QA Fluxo linha"; });
      });
      require(row, "Line detail");
      auto *list = dynamic_cast<Gtk::ListBox *>(row->get_parent());
      require(list, "Line list");
      list->select_row(*row);
      click(structure_, "Datas da linha selecionada…");
      attach_existing("Tempo da linha");
      snap("workflow-line-context");
      break;
    }
    case 11:
      click(host_, "Vincular data");
      {
        auto *row = find<Gtk::ListBoxRow>(structure_, [](auto &r) {
          return find<Gtk::Label>(
              r, [](auto &l) { return l.get_text() == "QA Fluxo unidade"; });
        });
        require(row, "Unit detail");
        auto *list = dynamic_cast<Gtk::ListBox *>(row->get_parent());
        require(list, "Unit list");
        list->select_row(*row);
        click(structure_, "Datas da parte selecionada…");
        attach_existing("Tempo da unidade");
        break;
      }
    case 12:
      click(host_, "Vincular data");
      host_.set_content(planning_);
      planning_.refresh();
      planning_.reveal_time_point(point_);
      break;
    case 13: {
      auto *menu = find<Gtk::MenuButton>(planning_, [](auto &b) {
        return b.get_tooltip_text() == "Ações da linha do tempo atual";
      });
      require(menu, "Axis menu");
      menu->popup();
      break;
    }
    case 14:
      click(planning_, "Ver mês do calendário…");
      break;
    case 15: {
      field<Gtk::ComboBoxText>("Apresentação do calendário").set_active_id("0");
      auto *picker =
          find<ui::CalendarPicker>(modal(), [](auto &) { return true; });
      require(picker, "Monthly picker");
      picker->show_month({2000, 1, 1});
      auto *cut = find<Gtk::Button>(*picker, [](auto &b) {
        return b.get_label().raw().find("†") != std::string::npos;
      });
      require(cut, "Cut not marked in cell");
      auto *day = find<Gtk::Button>(*picker, [](auto &b) {
        return b.get_label().raw().starts_with("16 ·");
      });
      require(day, "Interval not covered");
      g_signal_emit_by_name(day->gobj(), "clicked");
      snap("workflow-monthly-interval-cut");
      break;
    }
    case 16:
      footer("Fechar");
      if (!month_scrolled_) {
        scroll_modal(true);
        snap("workflow-monthly-day-details");
        month_scrolled_ = true;
        return true;
      }
      scroll_modal(false);
      field<Gtk::ComboBoxText>("Vigência da apresentação")
          .set_active_id("parallel");
      snap("workflow-monthly-parallel-filter");
      break;
    case 17:
      click(host_, "Fechar");
      click(planning_, "Novo ponto");
      break;
    case 18: {
      auto *selector =
          find<ui::IncrementalSelector>(modal(), [](auto &) { return true; });
      // New dated points use the main picker; complementary existing-point
      // selection is exercised below through the production helper.
      (void)selector;
      click(host_, "Fechar");
      auto *d =
          new ui::OverlayDialog("QA Seleção visual de marco", *this, true);
      d->add_button("Fechar", Gtk::ResponseType::CLOSE);
      auto *pick = Gtk::make_managed<ui::IncrementalSelector>(
          "Marcos", [](auto &, std::size_t) {
            return std::vector<ui::IncrementalSelection>{};
          });
      d->get_content_area()->append(*pick);
      ui::append_time_point_calendar(*pick, service_);
      d->present();
      auto *exp = find<Gtk::Expander>(modal(), [](auto &e) {
        return e.get_label() == "Escolher marco no calendário…";
      });
      require(exp, "Complementary picker");
      exp->set_expanded(true);
      field<Gtk::ComboBoxText>("Linha do tempo do marco").set_active_id(axis_);
      auto *picker =
          find<ui::CalendarPicker>(modal(), [](auto &) { return true; });
      require(picker, "Point calendar");
      picker->show_month({2000, 1, 1});
      auto *day = find<Gtk::Button>(*picker, [](auto &b) {
        return b.get_label().raw().starts_with("10 ·");
      });
      require(day, "Marked point day");
      g_signal_emit_by_name(day->gobj(), "clicked");
      snap("workflow-point-calendar");
      break;
    }
    case 19:
      click(host_, "QA Marco visual");
      {
        auto *selector =
            find<ui::IncrementalSelector>(modal(), [](auto &) { return true; });
        require(selector && selector->selected_id() == point_,
                "Visual point identity lost");
        click(host_, "Fechar");
        result_ = 0;
        std::cout << "PASS temporal workflow writing "
                     "atomic/cancel/stale/undo/redo/autosave, contextual "
                     "entity/relation/unit/line, compact summary, month "
                     "intervals/cuts/filter, visual points\n";
        hide();
        return false;
      }
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
  auto app = Gtk::Application::create("io.github.inde.temporalworkflowqa",
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
