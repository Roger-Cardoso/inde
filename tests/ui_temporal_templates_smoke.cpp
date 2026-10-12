#include "../src/ui/rendering_policy.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/temporal_library_page.hpp"
#include "ui/widget_probe.hpp"
#include <iostream>
using namespace inde;
using namespace inde::test::ui;
class Window final : public Gtk::ApplicationWindow {
public:
  Window(const std::filesystem::path &p, int &result, bool restart)
      : page_(service_), result_(result), restart_(restart) {
    service_.open(p);
    set_child(page_);
    add_css_class("inde-window");
    set_default_size(1000, 720);
    page_.refresh();
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL templates phase=" << phase_ << ": " << e.what()
                      << std::endl;
            hide();
            return false;
          }
        },
        800);
  }
  ~Window() override { timer_.disconnect(); }

private:
  application::ProjectService service_;
  ui::TemporalLibraryPage page_;
  int &result_;
  bool restart_;
  int phase_{};
  sigc::connection timer_;
  project::TemporalReference ref_;
  std::vector<project::TemporalTarget> targets_;
  std::vector<std::string> kinds_{"chinese",       "hebrew",
                                  "islamic-civil", "egyptian-civil",
                                  "maya-haab",     "ethiopic"};
  template <class T> T &field(const std::string &name) {
    auto *l = find<Gtk::Label>(page_, [&](auto &v) {
      return v.get_text().raw() == name && v.get_mnemonic_widget();
    });
    require(l, "Field missing: " + name);
    auto *w = dynamic_cast<T *>(l->get_mnemonic_widget());
    require(w, "Field type: " + name);
    return *w;
  }
  void snap(const std::string &name) {
    std::cout << "SNAPSHOT: " << name << std::endl;
  }
  void footer() {
    auto *b = find<Gtk::Button>(page_, [](auto &v) {
      return v.get_visible() && v.has_css_class("suggested-action");
    });
    require(b, "Footer save absent");
    graphene_rect_t r{};
    require(gtk_widget_compute_bounds(GTK_WIDGET(b->gobj()), GTK_WIDGET(gobj()),
                                      &r) &&
                r.origin.y >= 0 &&
                r.origin.y + r.size.height <= get_allocated_height(),
            "Footer clipped");
  }
  project::FictionalCalendar cal(const std::string &engine) {
    for (auto c : service_.worldbuilding().calendars())
      if (c.engine == engine)
        return c;
    throw std::runtime_error("Calendar absent " + engine);
  }
  void setup() {
    auto &n = service_.narrative();
    auto et = n.create_entity_type("qa-temporalperson", "QA Temporal Pessoa");
    auto x = n.create_entity(et.id, "QA Temporal Pessoa A"),
         y = n.create_entity(et.id, "QA Temporal Pessoa B");
    auto rt = n.create_relation_type("qa-temporal-link", "QA Temporal Ligação",
                                     "QA Temporal Inversa",
                                     project::RelationDirectionality::Directed);
    auto rel = n.create_relation(rt.id, x.id, y.id);
    auto ip =
        service_.create_intellectual_property("QA Temporal Universo", "", "");
    auto work = service_.create_work(ip.id, "QA Temporal Obra", "", "", "pt-BR",
                                     "draft");
    auto ns = service_.structures().create_narrative(work.id,
                                                     "QA Temporal Estrutura");
    auto u = service_.structures().create_unit(
        ns.id, "1", "QA Temporal Unidade", "", "", "");
    auto l = service_.structures().create_line(ns.id, "QA Temporal Linha", "");
    auto &w = service_.worldbuilding();
    for (auto [kind, id] :
         std::vector<std::pair<std::string, std::string>>{{"entity", x.id},
                                                          {"relation", rel.id},
                                                          {"unit", u.id},
                                                          {"line", l.id}})
      targets_.push_back(w.temporal_target(kind, id));
    auto axis = service_.planning().create_time_axis("QA Temporal Eixo");
    ref_.axis_id = axis.id;
    ref_.calendar_id = cal("chinese").id;
    ref_.label = "QA Temporal Data intercalar";
    ref_.year = 2023;
    ref_.month = 3;
    ref_.day = 1;
    ref_ = w.save_temporal_reference(ref_);
  }
  bool step() {
    std::cout << "STEP: " << phase_ << std::endl;
    auto &w = service_.worldbuilding();
    if (restart_) {
      if (phase_ == 0) {
        for (auto &k : kinds_)
          require(!cal(k).id.empty(), "Restart template lost");
        for (const auto &a : service_.planning().time_axes())
          if (a.name == "QA Temporal Eixo") {
            auto refs = w.temporal_references(a.id);
            require(refs.size() == 1, "Restart date lost");
            ref_ = refs.front();
          }
        require(!ref_.id.empty() && w.temporal_attachments(ref_.id).size() == 4,
                "Restart links lost");
        page_.date_links(ref_);
      }
      if (phase_ == 1) {
        footer();
        snap("templates-links-restart");
      }
      if (phase_ == 2) {
        result_ = 0;
        std::cout << "PASS temporal templates restart\n";
        hide();
        return false;
      }
      ++phase_;
      return true;
    }
    if (phase_ < 24) {
      auto k = kinds_.at(phase_ / 4);
      switch (phase_ % 4) {
      case 0:
        click(page_, "Usar modelo real…");
        break;
      case 1:
        click(page_, project::calendar_template(k).name);
        break;
      case 2:
        footer();
        require(field<Gtk::Entry>("Ano para conferir os meses").get_visible(),
                "Computed year absent");
        if (k == "hebrew" || k == "egyptian-civil" || k == "maya-haab" ||
            k == "ethiopic") {
          auto *s = find<Gtk::ScrolledWindow>(
              page_, [](auto &v) { return v.get_vexpand(); });
          require(s, "Main scroll absent");
          auto a = s->get_vadjustment();
          a->set_value(a->get_upper() - a->get_page_size());
        }
        snap(k + "-model");
        break;
      case 3:
        click(page_, "Salvar calendário e relógio");
        break;
      }
      ++phase_;
      return true;
    }
    if (phase_ == 24) {
      for (auto &k : kinds_)
        require(!cal(k).id.empty(), "Template not saved");
      setup();
      page_.date_links(ref_);
    }
    if (phase_ >= 25 && phase_ < 41) {
      int index = (phase_ - 25) / 4;
      auto target = targets_.at(index);
      switch ((phase_ - 25) % 4) {
      case 0:
        field<Gtk::ComboBoxText>("Tipo de elemento").set_active_id(target.kind);
        field<Gtk::Entry>("Buscar nome ou contexto").set_text(target.label);
        click(page_, "Buscar elementos");
        break;
      case 1: {
        auto *list =
            find<Gtk::ListBox>(page_, [](auto &v) { return v.get_mapped(); });
        require(list && list->get_row_at_index(0), "Result absent");
        require(!list->get_selected_row(), "Target automatically selected");
        click(page_, "Vincular data ao selecionado");
        require(w.temporal_attachments(ref_.id).size() ==
                    static_cast<std::size_t>(index),
                "Unselected write");
        list->select_row(*list->get_row_at_index(0));
        field<Gtk::Entry>("Significado da data").set_text("ocorrência");
        footer();
        snap(target.kind + "-selected");
        break;
      }
      case 2:
        click(page_, "Vincular data ao selecionado");
        break;
      case 3:
        require(w.temporal_attachments(ref_.id).size() ==
                    static_cast<std::size_t>(index + 1),
                "Link not persisted");
        require(w.target_dates(target) ==
                    std::vector<project::TemporalReference>{ref_},
                "Backlink wrong");
        break;
      }
    }
    if (phase_ == 41) {
      snap("four-links");
      page_.date_links();
    }
    if (phase_ == 42) {
      field<Gtk::ComboBoxText>("Tipo de elemento").set_active_id("unit");
      field<Gtk::Entry>("Buscar nome ou contexto").set_text(targets_[2].label);
      click(page_, "Buscar elementos");
    }
    if (phase_ == 43) {
      auto *list =
          find<Gtk::ListBox>(page_, [](auto &v) { return v.get_mapped(); });
      require(list && list->get_row_at_index(0), "Backlink result absent");
      list->select_row(*list->get_row_at_index(0));
      click(page_, "Consultar datas do selecionado");
    }
    if (phase_ == 44) {
      require(find<Gtk::Button>(page_,
                                [](auto &b) {
                                  return b.get_label().find(
                                             "QA Temporal Data intercalar") !=
                                         Glib::ustring::npos;
                                }),
              "Backlink UI absent");
      snap("backlinks");
    }
    if (phase_ == 45) {
      auto *b = find<Gtk::Button>(page_, [](auto &v) {
        return v.get_label().find("QA Temporal Data intercalar") !=
               Glib::ustring::npos;
      });
      require(b, "Backlink action absent");
      g_signal_emit_by_name(b->gobj(), "clicked");
    }
    if (phase_ == 46) {
      require(field<Gtk::ComboBoxText>("Mês (opcional)").get_active_id() == "3",
              "Intercalary selection lost");
      footer();
      snap("intercalary-date-edit");
      click(page_, "Salvar data e horário");
      require(w.temporal_references(ref_.axis_id).front() == ref_,
              "Unconfirmed shared-date edit");
    }
    if (phase_ == 47) {
      field<Gtk::Entry>("Ano").set_text("2024");
      require(field<Gtk::ComboBoxText>("Mês (opcional)").get_active_id() ==
                  "unknown",
              "Month silently reused in different year");
    }
    if (phase_ == 48) {
      page_.date_links(ref_);
    }
    if (phase_ == 49) {
      click(page_, "Desvincular");
      require(w.temporal_attachments(ref_.id).size() == 4,
              "Unconfirmed removal");
      auto *ack = find<Gtk::CheckButton>(page_, [](auto &v) {
        return v.get_label() == "Confirmar remoção apenas deste vínculo";
      });
      require(ack, "Removal confirmation absent");
      ack->set_active(true);
      click(page_, "Desvincular");
    }
    if (phase_ == 50) {
      require(w.temporal_attachments(ref_.id).size() == 3,
              "Confirmed removal failed");
      auto impact =
          service_.reversal_impact(service_.project_operations().front().id);
      require(impact.allowed, "Attachment reversal unavailable");
      service_.confirm_project_operation(impact);
      require(w.temporal_attachments(ref_.id).size() == 4,
              "Attachment reversal lost");
      page_.date_links(ref_);
    }
    if (phase_ == 51) {
      result_ = 0;
      std::cout << "PASS temporal templates six models, four explicit links, "
                   "no implicit selection, backlinks, footer\n";
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
  auto app = Gtk::Application::create("io.github.inde.temporal-templates-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  ui::configure_rendering_backend();
  int result = 1;
  Window *window{};
  app->signal_activate().connect([&] {
    ui::install_application_theme();
    window =
        new Window(argv[1], result, std::string(argv[2]) == "--verify-restart");
    app->add_window(*window);
    window->present();
  });
  char *fake[] = {argv[0], nullptr};
  app->run(1, fake);
  delete window;
  return result;
}
