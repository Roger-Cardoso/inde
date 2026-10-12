#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/temporal_library_page.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/calendar_picker.hpp"
#include "inde/ui/project_operations.hpp"
#include <charconv>
#include <limits>
#include <memory>
namespace inde::ui {
namespace {
std::string text_role(const std::string &role) {
  static const std::map<std::string, std::string> names{
      {"scene", "Cena"},
      {"mention", "Menção"},
      {"memory", "Memória"},
      {"reference", "Referência"}};
  const auto found = names.find(role);
  return found == names.end() ? role : found->second;
}
Gtk::Label *label(Gtk::Box &box, const std::string &text,
                  const char *css = "") {
  auto *l = Gtk::make_managed<Gtk::Label>(text);
  l->set_xalign(0);
  l->set_wrap(true);
  if (*css)
    l->add_css_class(css);
  box.append(*l);
  return l;
}
Gtk::Entry *entry(Gtk::Box &box, const std::string &name,
                  const std::string &value = "", const std::string &hint = "") {
  auto *e = Gtk::make_managed<Gtk::Entry>();
  e->set_text(value);
  e->set_max_length(2048);
  append_labeled_form_field(box, name, *e, hint);
  return e;
}
std::int64_t integer(const Glib::ustring &value) {
  auto s = project::trim_world_text(value.raw());
  std::int64_t n{};
  auto [p, e] = std::from_chars(s.data(), s.data() + s.size(), n);
  if (s.empty() || e != std::errc{} || p != s.data() + s.size())
    throw std::runtime_error("Preencha um número inteiro válido");
  return n;
}
int small(const Glib::ustring &value, int max = 10000) {
  auto n = integer(value);
  if (n < 0 || n > max)
    throw std::runtime_error("Número fora do limite");
  return static_cast<int>(n);
}
Gtk::Button *button(Gtk::Box &b, const std::string &text,
                    const std::function<void()> &f) {
  auto *v = Gtk::make_managed<Gtk::Button>(text);
  if (auto *l = dynamic_cast<Gtk::Label *>(v->get_child())) {
    l->set_wrap(true);
    l->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
    l->set_max_width_chars(72);
  }
  v->signal_clicked().connect(f);
  b.append(*v);
  return v;
}
// Edits only the in-memory draft. Actions are contextual to one selection.
class Items final : public Gtk::Box {
public:
  using Rows = std::vector<std::vector<std::string>>;
  Items(const std::string &title, std::vector<std::string> fields, Rows rows,
        std::function<void(const Rows &)> update, std::size_t max)
      : Gtk::Box(Gtk::Orientation::VERTICAL, 8), rows_(std::move(rows)),
        update_(std::move(update)), max_(max) {
    g_object_set_data(G_OBJECT(gobj()),"inde-crud-draft",GINT_TO_POINTER(1));
    label(*this, title, "heading");
    scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scroll_.set_min_content_height(100);
    scroll_.set_max_content_height(180);
    scroll_.set_propagate_natural_height(true);
    scroll_.set_child(list_);
    append(scroll_);
    list_.add_css_class("boxed-list");
    for (const auto &f : fields)
      fields_.push_back(entry(*this, f));
    list_.signal_row_selected().connect([this](Gtk::ListBoxRow *r) {
      selected_ = r ? r->get_index() : -1;
      for (std::size_t i = 0; i < fields_.size(); ++i)
        fields_[i]->set_text(selected_ >= 0 ? rows_.at(selected_).at(i) : "");
    });
    controls_.set_orientation(Gtk::Orientation::HORIZONTAL);
    controls_.set_spacing(6);
    append(controls_);
    auto action = [this](int kind) {
      try {
        auto values = rows_;
        std::vector<std::string> fields;
        for (auto *e : fields_)
          fields.push_back(project::trim_world_text(e->get_text().raw()));
        int next = selected_;
        if (kind == 0) {
          if (values.size() >= max_)
            throw std::runtime_error("Lista atingiu o limite");
          values.push_back(fields);
          next = static_cast<int>(values.size()) - 1;
        } else {
          if (selected_ < 0)
            throw std::runtime_error("Selecione um item da lista");
          if (kind == 1)
            values.at(selected_) = fields;
          if (kind == 2) {
            values.erase(values.begin() + selected_);
            next = -1;
          }
          if (kind == 3 && selected_ > 0) {
            std::swap(values[selected_], values[selected_ - 1]);
            --next;
          }
          if (kind == 4 && selected_ + 1 < static_cast<int>(values.size())) {
            std::swap(values[selected_], values[selected_ + 1]);
            ++next;
          }
        }
        update_(values);
        rows_ = std::move(values);
        render(next);
        status_.set_text("");
      } catch (const std::exception &e) {
        status_.set_text(author::diagnostic(e.what()));
      }
    };
    for (auto [name, kind] :
         std::vector<std::pair<std::string, int>>{{"Adicionar", 0},
                                                  {"Atualizar", 1},
                                                  {"Remover", 2},
                                                  {"↑", 3},
                                                  {"↓", 4}}) {
      auto *v = button(controls_, name, [action, kind] { action(kind); });
      if (kind == 3 || kind == 4)
        set_accessible_label(*v, kind == 3 ? "Mover item para cima"
                                           : "Mover item para baixo");
    }
    status_.set_wrap(true);
    status_.set_xalign(0);
    append(status_);
    render(-1);
    auto keys=Gtk::EventControllerKey::create();
    keys->signal_key_pressed().connect([this,action](guint key,guint,Gdk::ModifierType modifiers) {
      const auto mask=static_cast<unsigned>(modifiers)&(GDK_CONTROL_MASK|GDK_SHIFT_MASK|GDK_ALT_MASK|GDK_SUPER_MASK|GDK_META_MASK|GDK_HYPER_MASK);
      if(key==GDK_KEY_Return && mask==GDK_CONTROL_MASK){action(selected_<0 ? 0 : 1);return true;}
      if(mask==0 && key==GDK_KEY_Insert){action(0);return true;}
      auto *window=dynamic_cast<Gtk::Window *>(get_root());
      const bool list_focus=window && window->get_focus() && (window->get_focus()==&list_ || gtk_widget_is_ancestor(window->get_focus()->gobj(),GTK_WIDGET(list_.gobj())));
      if(!list_focus)return false;
      if(mask==0 && key==GDK_KEY_Delete){action(2);return true;}
      if(mask==(GDK_ALT_MASK|GDK_SHIFT_MASK) && (key==GDK_KEY_Up || key==GDK_KEY_Down)){action(key==GDK_KEY_Up ? 3 : 4);return true;}
      if(mask==0 && key==GDK_KEY_F2 && !fields_.empty()){fields_.front()->grab_focus();return true;}
      return false;
    },false);add_controller(keys);
  }

private:
  void render(int selected) {
    while (auto *c = list_.get_first_child())
      list_.remove(*c);
    for (const auto &r : rows_) {
      auto *l = Gtk::make_managed<Gtk::Label>(r.front());
      l->set_xalign(0);
      l->set_margin(8);
      l->set_ellipsize(Pango::EllipsizeMode::END);
      list_.append(*l);
    }
    if (selected >= 0)
      list_.select_row(*list_.get_row_at_index(selected));
  }
  Rows rows_;
  std::function<void(const Rows &)> update_;
  std::size_t max_;
  int selected_{-1};
  Gtk::ScrolledWindow scroll_;
  Gtk::ListBox list_;
  Gtk::Box controls_;
  Gtk::Label status_;
  std::vector<Gtk::Entry *> fields_;
};
} // namespace
TemporalLibraryPage::TemporalLibraryPage(application::ProjectService &s)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 10), service_(s) {
  set_hexpand(true);
  set_vexpand(true);
  title_.add_css_class("title-1");
  title_.set_hexpand(true);
  title_.set_xalign(0);
  heading_.append(title_);
  heading_.append(back_);
  append(heading_);
  scroll_.set_child(body_);
  scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  scroll_.set_vexpand(true);
  body_.set_margin(8);
  append(scroll_);
  status_.set_xalign(0);
  status_.set_wrap(true);
  footer_.append(status_);
  footer_.append(save_);
  append(footer_);
  save_.add_css_class("suggested-action");
  save_.set_halign(Gtk::Align::START);
  back_.signal_clicked().connect([this] { later([this] { refresh(); }); });
}
void TemporalLibraryPage::later(const std::function<void()> &f) {
  Glib::signal_idle().connect_once(sigc::track_obj(
      [this, f] {
        try {
          f();
        } catch (const std::exception &e) {
          error(e);
        }
      },
      *this));
}
void TemporalLibraryPage::clear() {
  save_connection_.disconnect();
  back_.set_label("Calendários e relógios");
  back_.set_tooltip_text("");
  save_.set_visible(false);
  status_.set_text("");
  while (auto *w = body_.get_first_child())
    body_.remove(*w);
  scroll_.get_vadjustment()->set_value(0);
}
void TemporalLibraryPage::error(const std::exception &e) {
  status_.set_text("Não aplicado: " + std::string(author::diagnostic(e.what())));
}
void TemporalLibraryPage::refresh() {
  clear();
  title_.set_text("Tempo");
  back_.set_visible(false);
  if (!service_.current()) {
    label(body_, "Abra um Projeto para gerenciar seu tempo.");
    return;
  }
  label(body_,
        "Calendários, relógios e datas do mundo. Escolha um modelo pronto ou crie seu calendário e relógio; a aplicação à linha do tempo é uma escolha separada.");
  button(body_, "Usar modelo real…", [this] {
    later([this] {
      clear();
      title_.set_text("Modelos reais");
      back_.set_visible(true);
      label(body_,
            "Cada modelo declara sua variante, era e limites. Conversão "
            "de datas não determina observância religiosa ou fuso. Reformas "
            "históricas e adoções regionais não são presumidas.");
      for (const auto &kind : project::calendar_template_engines()) {
        auto c = project::calendar_template(kind);
        button(body_, c.name,
               [this, c] { later([this, c] { edit_calendar(c); }); });
      }
    });
  });
  button(body_, "Criar calendário e relógio próprios…",
         [this] { later([this] { edit_calendar(); }); });
  button(body_, "Uso, mudanças e datas…",
         [this] { later([this] { show_usage(); }); });
  button(body_, "Consultar datas de um elemento…",
         [this] { later([this] { date_links(); }); });
  try {
    const auto values = service_.worldbuilding().calendars();
    label(body_, "Calendários e relógios deste projeto", "heading");
    if (values.empty())
      label(
          body_,
          "Nenhum calendário e relógio cadastrado. Não é preciso ter uma linha do tempo para começar.",
          "dim-label");
    for (const auto &c : values)
      button(body_,
             c.name + (c.engine == "chinese" || c.engine == "hebrew"
                           ? " · meses dependem do ano"
                           : " · " + std::to_string(c.months.size()) +
                                 " meses/períodos"),
             [this, c] { later([this, c] { edit_calendar(c); }); });
  } catch (const std::exception &e) {
    error(e);
  }
}
void TemporalLibraryPage::edit_calendar(
    std::optional<project::FictionalCalendar> original) {
  clear();
  back_.set_label("Cancelar edição");
  back_.set_tooltip_text("Voltar sem salvar o rascunho deste calendário e relógio");
  back_.set_visible(true);
  title_.set_text(original ? "Editar cópia do calendário e relógio"
                           : "Novo calendário e relógio");
  auto c = std::make_shared<project::FictionalCalendar>(
      original.value_or(project::FictionalCalendar{}));
  if (c->id.empty()) {
    c->id = project::new_uuid();
    c->name = "Meu calendário";
    c->months = {{"Primeiro mês", 30}};
  }
  std::optional<std::string> parent;
  // A template is not a persisted parent. Existing IDs are checked against the
  // library.
  for (const auto &existing : service_.worldbuilding().calendars())
    if (existing.id == c->id)
      parent = c->id;
  label(body_, parent ? "Você edita uma cópia. Salvar cria uma revisão, sem "
                        "alterar datas ou a versão anterior."
                      : "Salvar cria o calendário e relógio no Projeto. Nenhuma linha do tempo ou fato será convertido automaticamente.");
  auto *name = entry(body_, "Nome do calendário e relógio", c->name);
  auto *engine = Gtk::make_managed<Gtk::ComboBoxText>();
  engine->append("custom", "Próprio — ciclo simples configurável");
  if (c->engine == "custom" || c->engine == "gregorian" ||
      c->engine == "julian") {
    engine->append("gregorian", "Gregoriano proléptico");
    engine->append("julian", "Juliano proléptico");
  }
  if (c->engine != "custom" && c->engine != "gregorian" &&
      c->engine != "julian")
    engine->append(c->engine, c->name);
  engine->set_active_id(c->engine);
  append_labeled_form_field(
      body_, "Regras do calendário", *engine,
      "Para mudar durações/ordem de um modelo real, escolha Próprio. Regras "
      "gregorianas incluem exceções de século; o ciclo simples não as imita.");
  auto *ruleinfo = label(body_, project::calendar_rule_description(*c));
  engine->signal_changed().connect([engine, c, ruleinfo] {
    ruleinfo->set_text(
        engine->get_active_id() == "custom" && c->engine != "custom"
            ? "Derivação própria: deixa de usar as regras e época reais, "
              "incluindo meses intercalares calculados. O rascunho passa a "
              "usar meses e ciclo autorais."
            : project::calendar_rule_description(*c));
  });
  auto *sections = Gtk::make_managed<Gtk::Stack>();
  sections->set_transition_type(Gtk::StackTransitionType::NONE);
  auto *switcher = Gtk::make_managed<Gtk::StackSwitcher>();
  switcher->set_stack(*sections);
  body_.append(*switcher);
  body_.append(*sections);
  auto *calendar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
  auto *clock = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
  sections->add(*calendar, "calendar", "Calendário");
  sections->add(*clock, "clock", "Relógio e unidades");
  Items::Rows months;
  for (const auto &m : c->months)
    months.push_back({m.name, std::to_string(m.days)});
  auto *monthitems = Gtk::make_managed<Items>(
      "Meses", std::vector<std::string>{"Nome do mês", "Dias do mês"}, months,
      [c](const auto &rows) {
        auto proposed = *c;
        proposed.months.clear();
        for (const auto &r : rows)
          proposed.months.push_back({r[0], small(r[1], 60)});
        // Draft edits may temporarily have no months or a leap month beyond the
        // list; save validates all rules.
        for (const auto &m : proposed.months)
          if (m.name.empty() || m.days < 1)
            throw std::runtime_error("Preencha nome e 1–60 dias");
        c->months = proposed.months;
      },
      32);
  calendar->append(*monthitems);
  const bool fixed = c->engine != "custom" && c->engine != "gregorian" &&
                     c->engine != "julian";
  auto *calendar_preview =
      Gtk::make_managed<Gtk::Expander>("Prévia gráfica das regras");
  auto *previewbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *previewpicker = Gtk::make_managed<CalendarPicker>(false, false);
  previewbox->append(*previewpicker);
  calendar_preview->set_child(*previewbox);
  calendar->append(*calendar_preview);
  auto refresh_preview = [=, this] {
    try {
      previewpicker->set_calendar(*c);
    } catch (const std::exception &e) {
      error(e);
    }
  };
  button(*previewbox, "Atualizar prévia das regras", refresh_preview);
  calendar_preview->property_expanded().signal_changed().connect([=] {
    if (calendar_preview->get_expanded())
      refresh_preview();
  });
  if (fixed)
    monthitems->set_visible(false);
  Items::Rows week;
  for (const auto &w : c->weekdays)
    week.push_back({w});
  auto *weekitems = Gtk::make_managed<Items>(
      "Semana (opcional)", std::vector<std::string>{"Nome do dia da semana"},
      week,
      [c](const auto &rows) {
        std::vector<std::string> values;
        for (const auto &r : rows) {
          if (r[0].empty())
            throw std::runtime_error("Nome do dia vazio");
          values.push_back(r[0]);
        }
        c->weekdays = values;
      },
      16);
  if (fixed) {
    auto *weekdetails =
        Gtk::make_managed<Gtk::Expander>("Nomes dos dias da semana");
    weekdetails->set_child(*weekitems);
    calendar->append(*weekdetails);
    weekdetails->set_visible(!c->weekdays.empty());
    engine->signal_changed().connect([engine, weekdetails, c] {
      weekdetails->set_visible(engine->get_active_id() == "custom" ||
                               !c->weekdays.empty());
    });
  } else
    calendar->append(*weekitems);
  auto *advanced =
      Gtk::make_managed<Gtk::Expander>("Bissextos do calendário próprio");
  auto *rules = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  advanced->set_child(*rules);
  calendar->append(*advanced);
  advanced->set_visible(c->engine == "custom");
  engine->signal_changed().connect([engine, advanced] {
    advanced->set_visible(engine->get_active_id() == "custom");
  });
  auto *cycle = entry(*rules, "Repetir a cada quantos anos",
                      std::to_string(c->leap_cycle),
                      "0: sem dia extra. Motor próprio deste corte: um dia "
                      "adicional por ciclo.");
  auto *phase =
      entry(*rules, "Primeiro ano do ciclo", std::to_string(c->leap_phase));
  auto *leapmonth = entry(*rules, "Mês que recebe o dia extra",
                          std::to_string(c->leap_month));
  label(*clock, "Subdivisões regulares, do dia para a menor unidade. Ex.: 24 "
                "horas por dia; 60 minutos por hora. Nomes e plurais são "
                "autorais. Sem fuso presumido.");
  Items::Rows units;
  for (const auto &u : c->clock)
    units.push_back({u.singular, u.plural, u.symbol, std::to_string(u.parts)});
  clock->append(*Gtk::make_managed<Items>(
      "Relógio e nomes de medidas",
      std::vector<std::string>{"Nome singular", "Nome plural", "Abreviação",
                               "Quantidade na unidade anterior"},
      units,
      [c](const auto &rows) {
        std::vector<project::TimeUnit> values;
        for (const auto &r : rows) {
          auto n = small(r[3]);
          if (r[0].empty() || r[1].empty() || n < 1)
            throw std::runtime_error("Preencha nomes e quantidade positiva");
          values.push_back({r[0], r[1], r[2], n});
        }
        c->clock = values;
      },
      8));
  label(*clock,
        "Mês e ano continuam dependentes do calendário; não são durações fixas "
        "em segundos. Ciclos sazonais, novas unidades maiores e regras "
        "intercalares gerais ficam no próximo corte.",
        "dim-label");
  if (parent) {
    auto *removal = Gtk::make_managed<Gtk::Expander>("Excluir este calendário e relógio…");
    auto *details = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    label(*details, "Exclui somente a versão cadastrada, não o rascunho. Calendários e relógios com datas, adoções, linhas do tempo ou revisões derivadas não podem ser excluídos. Confira o impacto antes de confirmar.");
    button(*details, "Excluir calendário e relógio…", [this, id = *parent] {
      if (auto *window = dynamic_cast<Gtk::Window *>(get_root()))
        confirm_project_removal(*window, service_, project::RemovalKind::CalendarSystem,
                                id, [this] {
                                  later([this] {
                                    refresh();
                                    status_.set_text("Calendário e relógio excluídos. Reversão disponível no histórico do Projeto.");
                                  });
                                });
    })->add_css_class("destructive-action");
    removal->set_child(*details);
    body_.append(*removal);
  }
  save_.set_label(parent ? "Salvar nova versão das regras" : "Salvar calendário e relógio");
  save_.set_visible(true);
  save_connection_ = save_.signal_clicked().connect(
      [this, c, name, engine, cycle, phase, leapmonth, parent] {
        try {
          auto value = *c;
          value.name = name->get_text();
          value.engine = engine->get_active_id();
          if (value.engine == "custom") {
            value.leap_cycle = small(cycle->get_text());
            value.leap_phase = small(phase->get_text(), 9999);
            value.leap_month = small(leapmonth->get_text(), 32);
            value.rule_revision.clear();
          } else if (value.engine == "gregorian" || value.engine == "julian") {
            value.leap_cycle = 4;
            value.leap_phase = 0;
            value.leap_month = 2;
          }
          (void)service_.worldbuilding().create_calendar(value, parent);
          later([this] {
            refresh();
            status_.set_text(
                "Calendário e relógio salvos. Versões e datas anteriores preservadas.");
          });
        } catch (const std::exception &e) {
          error(e);
        }
      });
}
void TemporalLibraryPage::show_usage() {
  clear();
  back_.set_visible(true);
  title_.set_text("Uso e mudanças");
  label(body_, "Uma adoção muda o calendário de apresentação a partir de uma data, sem mover os acontecimentos. Datas autorais preservam o calendário e relógio de origem e a precisão.");
  const auto axes = service_.planning().time_axes();
  if (axes.empty())
    label(body_, "Crie uma linha do tempo na Cronologia para registrar adoções e datas. A biblioteca de calendários e relógios independe das linhas do tempo.");
  for (const auto &axis : axes) {
    label(body_, axis.name, "heading");
    button(body_, "Adoções — " + axis.name,
           [this, id = axis.id] { later([this, id] { adoption(id); }); });
    button(body_, "Datas e horários — " + axis.name,
           [this, id = axis.id] { later([this, id] { references(id); }); });
    button(body_, "Conferir datas paralelas — " + axis.name,
           [this, id = axis.id] { later([this, id] { parallel_dates(id); }); });
  }
}
void TemporalLibraryPage::configure_axis(const std::string &axis) { adoption(axis); }
void TemporalLibraryPage::open_reference(const std::string &axis, const std::string &id) {
  auto ref = service_.worldbuilding().temporal_reference(id);
  if (ref.axis_id != axis) throw std::runtime_error("A data mudou de linha do tempo; atualize os usos.");
  edit_reference(axis, ref);
}
void TemporalLibraryPage::parallel_dates(const std::string &axis) {
  clear();
  back_.set_visible(true);
  title_.set_text("Datas paralelas");
  label(body_, "Escolha um marco existente para consultar suas apresentações. "
               "Adoções respeitam a vigência; aposentadoria interrompe a "
               "exibição paralela futura, sem apagar as datas anteriores.");
  auto *points = Gtk::make_managed<Gtk::ComboBoxText>();
  auto values = service_.planning().time_points(axis);
  for (const auto &p : values)
    points->append(p.id, p.label);
  append_labeled_form_field(body_, "Marco da cronologia", *points);
  auto *output = label(body_, values.empty() ? "Esta linha do tempo ainda não tem marcos."
                                             : "Selecione um marco.");
  points->signal_changed().connect([this, points, output, values] {
    try {
      for (const auto &p : values) {
        if (p.id != points->get_active_id().raw())
          continue;
        const auto dates = service_.worldbuilding().parallel_dates(p);
        std::string text;
        for (const auto &d : dates)
          text += (text.empty() ? "" : "\n") + d;
        output->set_text(text.empty() ? "Sem calendário declarado nesta linha do tempo."
                                      : text);
      }
    } catch (const std::exception &e) {
      error(e);
    }
  });
}
void TemporalLibraryPage::adoption(const std::string &axis) {
  clear();
  back_.set_visible(true);
  auto axes = service_.planning().time_axes();
  auto found_axis = std::find_if(axes.begin(), axes.end(), [&](const auto &a) { return a.id == axis; });
  if (found_axis == axes.end()) throw std::runtime_error("Linha do tempo não encontrado");
  title_.set_text("Calendário da linha do tempo — " + found_axis->name);
  auto base = service_.worldbuilding().binding(axis);
  if (!base) {
    const auto available_systems = service_.worldbuilding().calendars();
    if (available_systems.empty()) {
      label(body_, "Sem calendário: os pontos desta linha do tempo permanecem em ordem relativa. Para usar datas, cadastre um calendário e relógio na Calendários e relógios; o alinhamento será uma escolha sua.");
      return;
    }
    label(body_, "Esta linha do tempo ainda é uma ordem relativa. Somente confirme se deseja declarar que uma unidade desta linha do tempo mede um dia. Os ordinais e rótulos existentes não serão modificados.");
    auto *systems = Gtk::make_managed<Gtk::ComboBoxText>();
    for (const auto &c : available_systems)
      systems->append(c.id, c.name);
    append_labeled_form_field(body_, "Calendário inicial", *systems);
    auto *points = Gtk::make_managed<Gtk::ComboBoxText>();
    const auto available_points = service_.planning().time_points(axis);
    points->append("manual", "Informar uma ordem no modo avançado");
    for (const auto &p : available_points)
      points->append(p.id, p.label);
    append_labeled_form_field(body_, "Marco que corresponde à data", *points);
    auto *advanced = Gtk::make_managed<Gtk::Expander>(
        "Alinhamento avançado — ordem numérica");
    auto *advanced_box =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    advanced->set_child(*advanced_box);
    body_.append(*advanced);
    auto *anchor = entry(*advanced_box, "Ordem do marco de referência", "0");
    label(*advanced_box, "Use quando não há um marco cadastrado. A ordem não é "
                         "um ano nem uma data.");
    points->set_active_id("manual");
    advanced->set_expanded(available_points.empty());
    points->signal_changed().connect([=] {
      advanced->set_expanded(points->get_active_id() == "manual");
      for (const auto &p : available_points)
        if (p.id == points->get_active_id().raw())
          anchor->set_text(std::to_string(p.ordinal));
    });
    auto *picker = Gtk::make_managed<CalendarPicker>(false, false);
    body_.append(*picker);
    auto *ack = Gtk::make_managed<Gtk::CheckButton>(
        "Declaro uma unidade = um dia e conferi o alinhamento");
    body_.append(*ack);
    auto *preview = label(body_, "");
    auto update = [=, this] {
      ack->set_active(false);
      if (systems->get_active_id().empty()) {
        preview->set_text("Escolha um calendário e relógio inicial para conferir o alinhamento.");
        return;
      }
      try {
        auto c = service_.worldbuilding().calendar(systems->get_active_id());
        preview->set_text("Ordem " +
                          std::to_string(integer(anchor->get_text())) + " = " +
                          project::format_calendar_date(c, picker->date()));
      } catch (const std::exception &e) {
        preview->set_text(author::diagnostic(e.what()));
      }
    };
    anchor->signal_changed().connect(update);
    picker->signal_changed().connect(update);
    systems->signal_changed().connect([=, this] {
      try {
        picker->set_calendar(
            service_.worldbuilding().calendar(systems->get_active_id()));
        update();
      } catch (const std::exception &e) {
        error(e);
      }
    });
    update();
    save_.set_visible(true);
    save_.set_label("Confirmar calendário inicial");
    save_connection_ = save_.signal_clicked().connect([=, this] {
      try {
        if (!ack->get_active())
          throw std::runtime_error("Confirme a medida e a prévia");
        auto c = service_.worldbuilding().calendar(systems->get_active_id());
        auto n = integer(anchor->get_text());
        auto delta = project::calendar_ordinal(c, picker->date());
        if ((delta > 0 &&
             n < std::numeric_limits<std::int64_t>::min() + delta) ||
            (delta < 0 && n > std::numeric_limits<std::int64_t>::max() + delta))
          throw std::runtime_error("Alinhamento excede os limites");
        service_.worldbuilding().assign_calendar(axis, c.id, n - delta, {});
        later([this, axis] { adoption(axis); });
      } catch (const std::exception &e) {
        ack->set_active(false);
        error(e);
      }
    });
    return;
  }
  auto history = service_.worldbuilding().adoptions(axis);
  auto *cleanup = Gtk::make_managed<Gtk::Expander>(history.empty() ? "Desvincular calendário da linha do tempo…" : "Rever a última adoção…");
  auto *cleanup_body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  cleanup->set_child(*cleanup_body);
  body_.append(*cleanup);
  label(*cleanup_body, history.empty()
      ? "A linha do tempo ficará sem calendário. Os pontos manterão suas ordens relativas; fatos, datas autorais e vínculos de texto serão preservados. Não é necessário cadastrar outro calendário e relógio."
      : "Remova adoções da mais recente para a mais antiga. A última adoção deixa de formatar pontos a partir de sua ordem de corte. Ordens, fatos e datas autorais permanecem intactos. Depois de retirar as adoções, você poderá desvincular a origem.");
  if (!history.empty())
    label(*cleanup_body, "Última adoção: " + service_.worldbuilding().calendar(history.back().calendar_id).name + " · ordem " + std::to_string(history.back().from_ordinal));
  auto *cleanup_ack = Gtk::make_managed<Gtk::CheckButton>("Entendi a mudança de exibição; preservar fatos e datas");
  cleanup_body->append(*cleanup_ack);
  auto *cleanup_button = button(*cleanup_body, history.empty() ? "Desvincular calendário" : "Remover última adoção", [=, this] {
    try {
      if (!cleanup_ack->get_active()) throw std::runtime_error("Confira e reconheça a mudança antes de confirmar.");
      if (history.empty()) service_.worldbuilding().assign_calendar(axis, {}, 0, base);
      else service_.worldbuilding().remove_last_adoption(*base, history);
      later([this, axis] { adoption(axis); status_.set_text("Configuração atualizada; fatos, ordens e datas preservados."); });
    } catch (const std::exception &e) { cleanup_ack->set_active(false); error(e); }
  });
  cleanup_button->add_css_class("destructive-action");
  cleanup_button->set_sensitive(false);
  cleanup_ack->signal_toggled().connect([=] { cleanup_button->set_sensitive(cleanup_ack->get_active()); });
  auto current = *base;
  if (!history.empty()) {
    current.calendar_id = history.back().calendar_id;
    current.epoch_ordinal = history.back().epoch_ordinal;
  }
  auto old = service_.worldbuilding().calendar(current.calendar_id);
  label(body_, "Vigente: " + old.name +
                   ". A reforma é desta linha do tempo; contextos regionais e mapa não estão sendo alterados.");
  for (const auto &h : history)
    label(body_,
          service_.worldbuilding().calendar(h.calendar_id).name + " · ordem " +
              std::to_string(h.from_ordinal) + " · " +
              (h.mode == "retire" ? "aposentou antecessores" : "coexiste"),
          "dim-label");
  auto values = service_.worldbuilding().calendars();
  if (std::none_of(values.begin(), values.end(), [&](const auto &c) {
        return c.id != current.calendar_id;
      })) {
    label(body_, "Este é o único calendário e relógio cadastrado. Você pode desvinculá-lo acima sem criar outro. Para uma sucessão, cadastre o novo calendário e relógio na Calendários e relógios.");
    return;
  }
  auto *target = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &c : values)
    if (c.id != current.calendar_id)
      target->append(c.id, c.name);
  append_labeled_form_field(body_, "Novo calendário", *target);
  auto *cut_expander =
      Gtk::make_managed<Gtk::Expander>("Escolher data da mudança de calendário — " + old.name);
  auto *picker = Gtk::make_managed<CalendarPicker>(false, false);
  picker->set_calendar(old);
  cut_expander->set_child(*picker);
  cut_expander->set_expanded(true);
  body_.append(*cut_expander);
  if (!history.empty()) {
    try {
      picker->show_month(project::calendar_date(
          old, history.back().from_ordinal, current.epoch_ordinal));
    } catch (const std::exception &) {
    }
  }
  auto *align = Gtk::make_managed<Gtk::CheckButton>(
      "Alinhar manualmente as duas datas (calendários próprios)");
  body_.append(*align);
  auto *manual = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  body_.append(*manual);
  manual->set_visible(false);
  auto *next_picker = Gtk::make_managed<CalendarPicker>(false, false);
  manual->append(*next_picker);
  target->signal_changed().connect([=, this] {
    try {
      next_picker->set_calendar(
          service_.worldbuilding().calendar(target->get_active_id()));
    } catch (const std::exception &e) {
      error(e);
    }
  });
  align->signal_toggled().connect([=] {
    manual->set_visible(align->get_active());
    cut_expander->set_expanded(!align->get_active());
  });
  label(
      body_,
      "Modelos reais são alinhados pelo mesmo dia terrestre. Para um "
      "calendário próprio, declare qual data do sucessor corresponde ao corte.",
      "dim-label");
  auto *mode = Gtk::make_managed<Gtk::ComboBoxText>();
  mode->append("retire", "Substituir o anterior a partir desta data");
  mode->append("coexist", "Usar o novo mantendo o anterior disponível");
  mode->set_active_id("coexist");
  append_labeled_form_field(body_, "Como o calendário passa a ser usado", *mode);
  auto *preview = label(body_, "");
  auto *ack = Gtk::make_managed<Gtk::CheckButton>(
      "Conferi a prévia; preservar fatos e datas de origem");
  body_.append(*ack);
  auto calculate = [=, this] {
    if (target->get_active_id().empty())
      throw std::runtime_error("Escolha um sucessor");
    auto next = service_.worldbuilding().calendar(target->get_active_id());
    auto cut = picker->date();
    auto at = project::calendar_ordinal(old, cut, current.epoch_ordinal);
    project::CalendarDate nextdate;
    if (!align->get_active())
      nextdate =
          project::terrestrial_date(next, project::terrestrial_day(old, cut));
    else
      nextdate = next_picker->date();
    auto offset = project::calendar_ordinal(next, nextdate);
    if ((offset > 0 &&
         at < std::numeric_limits<std::int64_t>::min() + offset) ||
        (offset < 0 && at > std::numeric_limits<std::int64_t>::max() + offset))
      throw std::runtime_error("Alinhamento excede limites");
    return std::pair{
        project::CalendarAdoption{
            {}, axis, next.id, at, at - offset, mode->get_active_id().raw()},
        project::format_calendar_date(old, cut) + " → " +
            project::format_calendar_date(next, nextdate)};
  };
  auto update = [=, this] {
    ack->set_active(false);
    try {
      preview->set_text(calculate().second);
    } catch (const std::exception &e) {
      preview->set_text(author::diagnostic(e.what()));
    }
  };
  picker->signal_changed().connect(update);
  next_picker->signal_changed().connect(update);
  target->signal_changed().connect(update);
  mode->signal_changed().connect(update);
  align->signal_toggled().connect(update);
  update();
  save_.set_visible(true);
  save_.set_label("Confirmar adoção");
  save_connection_ = save_.signal_clicked().connect([=, this] {
    try {
      if (!ack->get_active())
        throw std::runtime_error("Confira a prévia e confirme");
      auto [a, text] = calculate();
      (void)text;
      service_.worldbuilding().adopt_calendar(a, *base, history);
      later([this, axis] {
        adoption(axis);
        status_.set_text("Adoção salva; nenhum ponto foi movido.");
      });
    } catch (const std::exception &e) {
      ack->set_active(false);
      error(e);
    }
  });
}
void TemporalLibraryPage::references(const std::string &axis) {
  clear();
  back_.set_visible(true);
  title_.set_text("Datas e horários autorais");
  label(body_,
        "Referências preservadas na versão escolhida. Ano ou mês sem dia são válidos; hora vazia não é meia-noite. Abra uma data para vincular explicitamente elementos da história, relações, unidades ou linhas narrativas.");
  button(body_, "Adicionar data ou horário…",
         [this, axis] { later([this, axis] { edit_reference(axis); }); });
  auto *search = Gtk::make_managed<Gtk::SearchEntry>();
  search->set_placeholder_text("Pesquisar referência ou data");
  body_.append(*search);
  auto rows =
      std::make_shared<std::vector<std::pair<Gtk::Button *, Glib::ustring>>>();
  for (const auto &r : service_.worldbuilding().temporal_references(axis)) {
    auto c = service_.worldbuilding().calendar(r.calendar_id);
    auto caption = r.label + " — " + project::format_temporal_reference(c, r);
    auto *row = button(body_, caption, [this, axis, r] {
      later([this, axis, r] { edit_reference(axis, r); });
    });
    rows->emplace_back(row, Glib::ustring(caption).casefold());
  }
  search->signal_search_changed().connect([search, rows] {
    const auto query = search->get_text().casefold();
    for (const auto &[row, caption] : *rows)
      row->set_visible(query.empty() ||
                       caption.find(query) != Glib::ustring::npos);
  });
}
void TemporalLibraryPage::edit_reference(
    const std::string &axis,
    std::optional<project::TemporalReference> original) {
  clear();
  back_.set_label("Cancelar edição");
  back_.set_tooltip_text("Voltar sem salvar estas alterações");
  back_.set_visible(true);
  title_.set_text(original ? "Editar data e horário" : "Nova data e horário");
  auto *name =
      entry(body_, "Nome da data ou período", original ? original->label : "");
  auto *combo = Gtk::make_managed<Gtk::ComboBoxText>();
  auto values = service_.worldbuilding().calendars();
  for (const auto &c : values)
    combo->append(c.id, c.name);
  if (original)
    combo->set_active_id(original->calendar_id);
  append_labeled_form_field(body_, "Calendário da data de origem", *combo);
  auto *picker = Gtk::make_managed<CalendarPicker>();
  body_.append(*picker);
  auto *details =
      Gtk::make_managed<Gtk::Expander>("Regras e limites deste calendário");
  auto *monthhelp = Gtk::make_managed<Gtk::Label>();
  monthhelp->set_xalign(0);
  monthhelp->set_wrap(true);
  details->set_child(*monthhelp);
  body_.append(*details);
  auto initial = std::make_shared<bool>(true);
  auto explain = [=, this] {
    try {
      if (combo->get_active_id().empty())
        return;
      auto c = service_.worldbuilding().calendar(combo->get_active_id());
      picker->set_calendar(c);
      if (*initial && original)
        picker->set_selection(
            project::TemporalBoundary{original->year, original->month,
                                      original->day, original->clock},
            original->upper);
      monthhelp->set_text(project::calendar_rule_description(c));
      *initial = false;
    } catch (const std::exception &e) {
      error(e);
    }
  };
  combo->signal_changed().connect(explain);
  explain();
  auto *note = entry(body_, "Nota", original ? original->note : "");
  auto *precision =
      Gtk::make_managed<Gtk::Expander>("Precisão, intervalo e incerteza");
  auto *precision_box =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  precision->set_child(*precision_box);
  body_.append(*precision);
  auto *qualifier = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &[key, label] :
       std::vector<std::pair<std::string, std::string>>{
           {"exact", "Data informada — respeitar campos desconhecidos"},
           {"before", "Antes da data informada"},
           {"after", "Depois da data informada"},
           {"between", "Entre dois limites inclusivos"},
           {"approximate",
            "Aproximadamente — sem limites numéricos presumidos"},
           {"season", "Estação / época descrita pelo autor"}})
    qualifier->append(key, label);
  qualifier->set_active_id(original ? original->qualifier : "exact");
  append_labeled_form_field(*precision_box, "Quanto sabemos sobre a data", *qualifier);
  label(*precision_box,
        "Dia sem hora cobre o dia; mês sem dia cobre o mês. Antes/depois não "
        "incluem o período informado. Aproximação e estação não entram em "
        "comparação numérica automática.");
  auto *uncertainty = entry(*precision_box, "Explicação da incerteza / estação",
                            original ? original->uncertainty : "");
  auto toggle_upper = [=] {
    picker->set_range(qualifier->get_active_id() == "between");
  };
  qualifier->signal_changed().connect(sigc::track_obj(toggle_upper, *picker));
  toggle_upper();
  precision->set_expanded(original && original->qualifier != "exact");
  const auto expected_links =
      original ? service_.worldbuilding().temporal_attachments(original->id)
               : std::vector<project::TemporalAttachment>{};
  const auto expected_text =
      original ? service_.worldbuilding().text_date_uses(original->id)
               : std::vector<application::TextDateUse>{};
  const auto affected = expected_links.size() + expected_text.size();
  Gtk::CheckButton *impact = nullptr;
  if (affected) {
    label(body_, std::to_string(affected) +
                     " vínculos (elementos e documentos) compartilham esta "
                     "referência. A edição altera "
                     "a data que eles consultam; para outra data, crie uma "
                     "referência separada.");
    impact = Gtk::make_managed<Gtk::CheckButton>(
        "Confirmo atualizar a data consultada por seus vínculos");
    body_.append(*impact);
    if (!expected_text.empty()) {
      auto *uses = Gtk::make_managed<Gtk::Expander>(
          "Documentos que consultam esta data");
      auto *items = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
      uses->set_child(*items);
      for (const auto &use : expected_text)
        label(*items, use.title + " — papel: " + text_role(use.role),
              "dim-label");
      body_.append(*uses);
    }
  }
  save_.set_visible(true);
  save_.set_label("Salvar data e horário");
  save_connection_ = save_.signal_clicked().connect([=, this] {
    try {
      if (affected && !impact->get_active())
        throw std::runtime_error("Confirme o impacto da edição nos vínculos");
      project::TemporalReference r =
          original.value_or(project::TemporalReference{});
      r.axis_id = axis;
      r.calendar_id = combo->get_active_id();
      r.label = name->get_text();
      const auto lower = picker->boundary();
      r.year = lower.year;
      r.month = lower.month;
      r.day = lower.day;
      r.clock = lower.clock;
      r.note = note->get_text();
      r.qualifier = qualifier->get_active_id();
      r.uncertainty = uncertainty->get_text();
      r.upper.reset();
      if (r.qualifier == "between") {
        auto upper = picker->boundary(true);
        r.upper = std::move(upper);
      }
      service_.worldbuilding().save_temporal_reference(
          r, original, expected_links, expected_text);
      later([this, axis] {
        references(axis);
        status_.set_text("Referência salva com sua precisão original.");
      });
    } catch (const std::exception &e) {
      error(e);
    }
  });
  if (original) {
    button(body_, "Vínculos desta data…", [this, original] {
      later([this, original] { date_links(original); });
    });
    button(body_, "Remover referência", [=, this] {
      try {
        auto *owner = dynamic_cast<Gtk::Window *>(get_root());
        if (!owner) return;
        confirm_project_removal(*owner, service_, project::RemovalKind::TemporalReference, original->id,
            [this, axis] { later([this, axis] { references(axis); }); });
      } catch (const std::exception &e) {
        error(e);
      }
    })->add_css_class("destructive-action");
  }
}
void TemporalLibraryPage::date_links(
    std::optional<project::TemporalReference> ref) {
  clear();
  back_.set_visible(true);
  title_.set_text(ref ? "Vínculos da data" : "Datas de um elemento");
  if (ref) {
    auto c = service_.worldbuilding().calendar(ref->calendar_id);
    label(body_,
          ref->label + " — " + project::format_temporal_reference(c, *ref),
          "heading");
  }
  label(body_,
        "Escolha um elemento cadastrado no projeto. O vínculo não altera a "
        "relação, a ordem narrativa ou o grafo; o significado da data é seu.");
  auto *kind = Gtk::make_managed<Gtk::ComboBoxText>();
  for (auto [id, name] : std::vector<std::pair<std::string, std::string>>{
           {"entity", "Elemento da história"},
           {"relation", "Relação entre elementos da história"},
           {"unit", "Parte da narrativa"},
           {"line", "Linha narrativa"}})
    kind->append(id, name);
  append_labeled_form_field(body_, "Tipo de elemento", *kind);
  kind->set_active(0);
  auto *query = entry(body_, "Buscar nome ou contexto", "",
                      "Até 100 resultados; refine pelo nome. Nada é "
                      "selecionado automaticamente.");
  auto *results = Gtk::make_managed<Gtk::ListBox>();
  results->add_css_class("boxed-list");
  results->set_selection_mode(Gtk::SelectionMode::SINGLE);
  auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
  scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  scroll->set_min_content_height(100);
  scroll->set_max_content_height(200);
  scroll->set_propagate_natural_height(true);
  scroll->set_child(*results);
  auto values = std::make_shared<std::vector<project::TemporalTarget>>();
  auto selected = std::make_shared<std::optional<project::TemporalTarget>>();
  auto *selection = label(body_, "Nenhum elemento selecionado.");
  auto *dates = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto reset = [=, this] {
    status_.set_text("");
    selected->reset();
    values->clear();
    while (auto *w = results->get_first_child())
      results->remove(*w);
    while (auto *w = dates->get_first_child())
      dates->remove(*w);
    selection->set_text("Nenhum elemento selecionado.");
  };
  button(body_, "Buscar elementos", [=, this] {
    try {
      reset();
      *values = service_.worldbuilding().temporal_targets(kind->get_active_id(),
                                                          query->get_text());
      for (const auto &v : *values) {
        auto *l = Gtk::make_managed<Gtk::Label>(v.label + "\n" + v.context);
        l->set_xalign(0);
        l->set_wrap(true);
        l->set_margin(8);
        results->append(*l);
      }
      if (values->empty())
        selection->set_text("Nenhum elemento encontrado. Crie-o no Projeto; nenhum elemento da história de exemplo será criado.");
    } catch (const std::exception &e) {
      error(e);
    }
  });
  body_.append(*scroll);
  body_.append(*dates);
  kind->signal_changed().connect(
      sigc::track_obj(reset, *selection, *results, *dates, status_));
  query->signal_changed().connect(
      sigc::track_obj(reset, *selection, *results, *dates, status_));
  // Clearing the page can emit row-unselected after earlier siblings were
  // destroyed. Disconnect when any referenced widget dies, not just the list.
  results->signal_row_selected().connect(sigc::track_obj(
      [=, this](Gtk::ListBoxRow *row) {
        status_.set_text("");
        selected->reset();
        if (row && row->get_index() >= 0 &&
            row->get_index() < static_cast<int>(values->size()))
          *selected = values->at(row->get_index());
        selection->set_text(*selected ? "Selecionado: " + (*selected)->label +
                                            " — " + (*selected)->context
                                      : "Nenhum elemento selecionado.");
        while (auto *w = dates->get_first_child())
          dates->remove(*w);
      },
      *selection, *dates, status_, *this));
  if (!ref) {
    button(body_, "Consultar datas do selecionado", [=, this] {
      try {
        if (!*selected)
          throw std::runtime_error("Selecione um resultado");
        while (auto *w = dates->get_first_child())
          dates->remove(*w);
        auto refs = service_.worldbuilding().target_dates(**selected);
        if (refs.empty())
          label(*dates, "Este elemento ainda não possui vínculos de datas.");
        for (const auto &r : refs) {
          auto c = service_.worldbuilding().calendar(r.calendar_id);
          std::string roles;
          for (const auto &a :
               service_.worldbuilding().temporal_attachments(r.id))
            if (a.kind == (*selected)->kind && a.target_id == (*selected)->id)
              roles += (roles.empty() ? "" : " · ") + a.role;
          button(*dates,
                 r.label + " — " + roles + "\n" +
                     project::format_temporal_reference(c, r),
                 [this, r] {
                   later([this, r] { edit_reference(r.axis_id, r); });
                 });
        }
      } catch (const std::exception &e) {
        error(e);
      }
    });
    return;
  }
  auto *role = entry(body_, "Significado da data", "",
                     "Escreva o papel: nascimento, início, fim, ocorrência, "
                     "menção… Nenhum significado é presumido.");
  auto *meaning = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &[key, value] :
       std::vector<std::pair<std::string, std::string>>{
           {"unspecified", "Não classificar — somente significado livre"},
           {"birth", "Nascimento (declaração formal)"},
           {"death", "Morte (declaração formal)"},
           {"start", "Início"},
           {"end", "Fim"},
           {"occurrence", "Quando acontece"},
           {"mention", "Menção"}})
    meaning->append(key, value);
  meaning->set_active_id("unspecified");
  append_labeled_form_field(body_, "Papel formal para consultas (opcional)",
                            *meaning);
  auto *note = entry(body_, "Nota do vínculo (opcional)");
  save_.set_label("Vincular data ao selecionado");
  save_.set_visible(true);
  save_connection_ = save_.signal_clicked().connect([=, this] {
    try {
      if (!*selected)
        throw std::runtime_error(
            "Selecione explicitamente um elemento da busca");
      project::TemporalAttachment a;
      a.reference_id = ref->id;
      a.kind = (*selected)->kind;
      a.target_id = (*selected)->id;
      a.role = role->get_text();
      a.meaning = meaning->get_active_id();
      a.note = note->get_text();
      service_.worldbuilding().attach_date(a, *ref, **selected);
      later([this, ref] {
        date_links(ref);
        status_.set_text(
            "Vínculo salvo. Data e significado autorais preservados.");
      });
    } catch (const std::exception &e) {
      error(e);
    }
  });
  label(body_, "Vínculos existentes", "heading");
  const auto links = service_.worldbuilding().temporal_attachments(ref->id);
  if (links.empty())
    label(body_, "Esta data ainda não está vinculada.", "dim-label");
  for (const auto &a : links) {
    auto target = service_.worldbuilding().temporal_target(a.kind, a.target_id);
    auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    body_.append(*box);
    label(*box, a.role + " — " + target.label + "\n" + target.context);
    if (a.meaning != "unspecified")
      label(*box,
            "Papel formal: " +
                std::map<std::string, std::string>{{"birth", "Nascimento"},
                                                   {"death", "Morte"},
                                                   {"start", "Início"},
                                                   {"end", "Fim"},
                                                   {"occurrence", "Quando acontece"},
                                                   {"mention", "Menção"}}
                    .at(a.meaning),
            "dim-label");
    if (!a.note.empty())
      label(*box, a.note, "dim-label");
    auto *ack = Gtk::make_managed<Gtk::CheckButton>(
        "Confirmar remoção apenas deste vínculo");
    box->append(*ack);
    button(*box, "Desvincular", [=, this] {
      try {
        if (!ack->get_active())
          throw std::runtime_error("Confirme a remoção do vínculo");
        service_.worldbuilding().remove_date_attachment(a);
        later([this, ref] { date_links(ref); });
      } catch (const std::exception &e) {
        error(e);
      }
    });
  }
}
} // namespace inde::ui
