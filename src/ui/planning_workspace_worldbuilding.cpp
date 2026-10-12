#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/calendar_picker.hpp"
#include "temporal_form_helpers.hpp"
#include "inde/persistence/calendar_navigation_store.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/planning_workspace.hpp"
#include <charconv>
#include <memory>
#include <sstream>
namespace inde::ui {
namespace {
void replace(OverlayDialog *d, const std::function<void()> &next) {
  d->hide();
  next();
  Glib::signal_idle().connect_once([d] { delete d; });
}
Gtk::Label *message(Gtk::Box &box, const std::string &text = {}) {
  auto *l = Gtk::make_managed<Gtk::Label>(text);
  l->set_xalign(0);
  l->set_wrap(true);
  box.append(*l);
  return l;
}
Gtk::Entry *entry(Gtk::Box &box, const std::string &label,
                  const std::string &value = {}, const std::string &help = {}) {
  auto *e = Gtk::make_managed<Gtk::Entry>();
  e->set_text(value);
  e->set_max_length(2048);
  append_labeled_form_field(box, label, *e, help);
  return e;
}
std::int64_t number(const Glib::ustring &value) {
  auto s = project::trim_world_text(value.raw());
  std::int64_t n{};
  auto [p, e] = std::from_chars(s.data(), s.data() + s.size(), n);
  if (e != std::errc{} || p != s.data() + s.size() || s.empty())
    throw std::runtime_error("Preencha um número inteiro válido");
  return n;
}
int bounded_number(const Glib::ustring &value, int max) {
  auto n = number(value);
  if (n < 0 || n > max)
    throw std::runtime_error("Número fora do limite permitido");
  return static_cast<int>(n);
}
const char *kind_label(const std::string &kind) {
  return kind == "nickname"   ? "Apelido"
         : kind == "title"    ? "Título"
         : kind == "spelling" ? "Grafia alternativa"
                              : "Outro";
}
} // namespace
void PlanningWorkspace::show_aliases() {
  const auto *entity = selected_entity();
  auto *owner = owner_window();
  if (!entity || !owner)
    return;
  const auto identity = entity->id;
  try {
    const auto values = service_.worldbuilding().aliases(identity);
    auto *d =
        new OverlayDialog("Nomes alternativos — " + entity->name, *owner, true);
    d->set_secondary_text(
        "Apelidos, títulos e grafias ajudam a localizar este mesmo elemento da história. Homônimos não são fundidos; nenhuma menção do texto é alterada.");
    d->add_button("Fechar", Gtk::ResponseType::CLOSE);
    auto &box = *d->get_content_area();
    auto *add = Gtk::make_managed<Gtk::Button>("Adicionar alias");
    box.append(*add);
    add->signal_clicked().connect(
        [this, d] { replace(d, [this] { edit_alias(); }); });
    if (values.empty())
      message(box, "Nenhum nome alternativo cadastrado.");
    for (const auto &a : values) {
      auto *button =
          Gtk::make_managed<Gtk::Button>(a.name + " — " + kind_label(a.kind));
      button->set_tooltip_text(a.note);
      box.append(*button);
      button->signal_clicked().connect(
          [this, d, a] { replace(d, [this, a] { edit_alias(a); }); });
    }
    d->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível consultar aliases", e);
  }
}
void PlanningWorkspace::edit_alias(
    std::optional<project::EntityAlias> original) {
  const auto *entity = selected_entity();
  auto *owner = owner_window();
  if (!entity || !owner)
    return;
  const auto entity_id = entity->id;
  auto *d =
      new OverlayDialog(original ? "Editar nome alternativo" : "Novo nome alternativo", *owner, true);
  d->add_button("Fechar", Gtk::ResponseType::CLOSE);
  d->set_secondary_text(
      "O nome principal e a identidade não mudam. A nota descreve uso; não impõe período ou área da consulta automaticamente.");
  auto &box = *d->get_content_area();
  auto *name = entry(box, "Nome alternativo", original ? original->name : "");
  auto *kind = Gtk::make_managed<Gtk::ComboBoxText>();
  for (auto key : {"nickname", "title", "spelling", "other"})
    kind->append(key, kind_label(key));
  kind->set_active_id(original ? original->kind : "nickname");
  append_labeled_form_field(box, "Tipo de nome alternativo", *kind);
  auto *note = entry(box, "Nota de uso", original ? original->note : "");
  auto *status = message(box);
  auto *save = Gtk::make_managed<Gtk::Button>("Salvar nome alternativo");
  save->add_css_class("suggested-action");
  box.append(*save);
  save->signal_clicked().connect(
      [this, d, name, kind, note, status, original, entity_id] {
        try {
          project::EntityAlias a = original.value_or(project::EntityAlias{});
          a.entity_id = entity_id;
          a.name = name->get_text();
          a.kind = kind->get_active_id();
          a.note = note->get_text();
          (void)service_.worldbuilding().save_alias(a, original);
          refresh_details();
          replace(d, [this] { show_aliases(); });
          signal_status_message_.emit(
              "Nome alternativo salvo; nome principal preservado");
        } catch (const std::exception &e) {
          status->set_text("Não salvo: " + std::string(author::diagnostic(e.what())));
        }
      });
  if (original) {
    auto *confirm =
        Gtk::make_managed<Gtk::CheckButton>("Confirmo a remoção deste alias");
    box.append(*confirm);
    auto *remove = Gtk::make_managed<Gtk::Button>("Remover alias");
    remove->add_css_class("destructive-action");
    remove->set_sensitive(false);
    box.append(*remove);
    confirm->signal_toggled().connect(
        [confirm, remove] { remove->set_sensitive(confirm->get_active()); });
    remove->signal_clicked().connect([this, d, confirm, status, original] {
      if (!confirm->get_active())
        return;
      try {
        service_.worldbuilding().remove_alias(*original);
        refresh_details();
        replace(d, [this] { show_aliases(); });
      } catch (const std::exception &e) {
        status->set_text("Não removido: " + std::string(author::diagnostic(e.what())));
      }
    });
  }
  d->present();
}
void PlanningWorkspace::show_calendar(std::string preferred) {
  if (preferred.empty()) preferred = axis_combo_.get_active_id().raw();
  show_temporal_library();
  if (preferred.empty()) temporal_library_.show_usage();
  else temporal_library_.configure_axis(preferred);
}
void PlanningWorkspace::create_calendar_definition() {
  show_temporal_library();
  temporal_library_.edit_calendar();
}
void PlanningWorkspace::create_calendar_point() {
  const auto axis = axis_combo_.get_active_id().raw();
  auto *owner = owner_window();
  if (axis.empty() || !owner)
    return;
  try {
    const auto initial = service_.worldbuilding().binding(axis);
    if (!initial) {
      show_calendar();
      return;
    }
    std::vector<project::AxisCalendar> choices{*initial};
    const auto history = service_.worldbuilding().adoptions(axis);
    for (const auto &a : history)
      choices.push_back({axis, a.calendar_id, a.epoch_ordinal, a.id});
    auto b = std::make_shared<project::AxisCalendar>(choices.back());
    auto c = std::make_shared<project::FictionalCalendar>(
        service_.worldbuilding().calendar(b->calendar_id));
    auto *d = new OverlayDialog("Novo ponto por data ficcional", *owner, true);
    d->add_button("Fechar", Gtk::ResponseType::CLOSE);
    d->set_secondary_text(
        "O sistema precisa estar vigente na data escolhida. O rótulo autoral "
        "é independente da data calculada; adoções são conferidas ao criar.");
    auto &box = *d->get_content_area();
    auto *systems = Gtk::make_managed<Gtk::ComboBoxText>();
    for (std::size_t i = 0; i < choices.size(); ++i)
      systems->append(
          std::to_string(i),
          service_.worldbuilding().calendar(choices[i].calendar_id).name +
              (i ? " — a partir da ordem " +
                       std::to_string(history[i - 1].from_ordinal)
                 : " — origem"));
    append_labeled_form_field(box, "Sistema da data", *systems);
    auto *picker = Gtk::make_managed<CalendarPicker>(false, false);
    box.append(*picker);
    auto *label = entry(box, "Nome do marco");
    auto *description = entry(box, "Descrição");
    auto *preview = message(box);
    auto *status = message(box);
    auto *create = Gtk::make_managed<Gtk::Button>("Criar ponto por data");
    box.append(*create);
    auto update = [c, b, picker, preview, create] {
      try {
        auto value = picker->date();
        auto ordinal = project::calendar_ordinal(*c, value, b->epoch_ordinal);
        preview->set_text(project::format_calendar_date(*c, value));
        preview->set_tooltip_text("Ordem interna: " + std::to_string(ordinal));
        create->set_sensitive(true);
      } catch (const std::exception &e) {
        preview->set_text(author::diagnostic(e.what()));
        create->set_sensitive(false);
      }
    };
    picker->signal_changed().connect(update);
    systems->signal_changed().connect([=, this] {
      try {
        auto index = bounded_number(systems->get_active_id(), 128);
        *b = choices.at(index);
        *c = service_.worldbuilding().calendar(b->calendar_id);
        picker->set_calendar(*c);
        picker->set_selection({});
        if (index)
          picker->show_month(project::calendar_date(
              *c, history.at(index - 1).from_ordinal, b->epoch_ordinal));
        else {
          persistence::PlanningQuery q;
          q.limit = 1;
          const auto points = service_.planning().time_points(axis, q);
          if (!points.empty())
            picker->show_month(project::calendar_date(
                *c, points.front().ordinal, b->epoch_ordinal));
        }
        update();
      } catch (const std::exception &e) {
        status->set_text(author::diagnostic(e.what()));
        create->set_sensitive(false);
      }
    });
    systems->set_active_id(std::to_string(choices.size() - 1));
    create->signal_clicked().connect(
        [this, d, b, picker, label, description, status] {
          try {
            auto p = service_.worldbuilding().create_dated_point(
                *b, picker->date(), label->get_text(), description->get_text());
            refresh_time_points();
            select_time_point(p.id);
            signal_status_message_.emit("Ponto criado por data ficcional");
            d->hide();
            Glib::signal_idle().connect_once([d] { delete d; });
          } catch (const std::exception &e) {
            status->set_text("Não criado: " + std::string(author::diagnostic(e.what())));
          }
        });
    d->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível criar por data", e);
  }
}
void PlanningWorkspace::show_calendar_month() {
  const auto axis = axis_combo_.get_active_id().raw();
  auto *owner = owner_window();
  if (axis.empty() || !owner)
    return;
  try {
    auto base = service_.worldbuilding().binding(axis);
    if (!base) {
      show_calendar();
      return;
    }
    const auto history = service_.worldbuilding().adoptions(axis);
    std::vector<project::AxisCalendar> choices{*base};
    for (const auto &a : history)
      choices.push_back({axis, a.calendar_id, a.epoch_ordinal, a.id});
    auto binding = std::make_shared<project::AxisCalendar>(choices.back());
    auto calendar = std::make_shared<project::FictionalCalendar>(
        service_.worldbuilding().calendar(binding->calendar_id));
    auto *d = new OverlayDialog("Calendário — consulta", *owner, true);
    d->add_button("Fechar", Gtk::ResponseType::CLOSE);
    d->set_secondary_text(
        "Navegue pelo calendário e escolha um dia para consultar seus "
        "registros. † indica uma adoção; o número conta registros exibidos. "
        "A consulta não grava datas nem modifica acontecimentos.");
    auto &box = *d->get_content_area();
    auto *lens = Gtk::make_managed<Gtk::ComboBoxText>();
    constrain_temporal_combo(*lens);
    for (std::size_t i = 0; i < choices.size(); ++i)
      lens->append(
          std::to_string(i),
          service_.worldbuilding().calendar(choices[i].calendar_id).name +
              (i ? " — adoção" : " — origem"));
    append_labeled_form_field(box, "Apresentação do calendário", *lens);
    auto *filter = Gtk::make_managed<Gtk::ComboBoxText>();
    filter->append("all","Todos os registros");
    filter->append("active","Somente vigência principal");
    filter->append("parallel","Somente apresentação paralela");
    filter->append("outside","Fora da vigência declarada");
    filter->set_active_id("all");
    append_labeled_form_field(box,"Vigência da apresentação",*filter);
    auto *regime = message(box);
    auto *picker = Gtk::make_managed<CalendarPicker>(false, false);
    box.append(*picker);
    auto view = std::make_shared<application::CalendarMonthView>();
    auto *status = message(box);
    auto *records = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    box.append(*records);
    auto *unpositioned = Gtk::make_managed<Gtk::Expander>();
    auto *unpositioned_rows = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,4);
    unpositioned->set_child(*unpositioned_rows);
    unpositioned->set_visible(false);
    box.append(*unpositioned);
    auto selected = [=, this] {
      while (auto *child = records->get_first_child())
        records->remove(*child);
      try {
        const auto day = picker->date();
        const auto month = picker->visible_month();
        if(day.year!=month.year || day.month!=month.month) {
          message(*records,"A escolha anterior foi preservada. Escolha um dia do mês visível para consultar seus registros.");return;
        }
        const auto at =
            project::calendar_ordinal(*calendar, day, binding->epoch_ordinal);
        message(*records, project::format_calendar_date(*calendar, day));
        const auto state = view->regimes.find(day.day);
        if (state != view->regimes.end()) message(*records,
            state->second == "active" ? "Calendário principal nesta data" :
            state->second == "parallel" ? "Apresentação paralela nesta data" : "Fora da vigência declarada — conversão para consulta");
        if (auto cut=view->cuts.find(day.day);cut!=view->cuts.end()) message(*records,cut->second);
        bool any = false;
        for (const auto &p : view->points)
          if (p.ordinal == at) {
            any = true;
            auto *button = Gtk::make_managed<Gtk::Button>(p.label);
            records->append(*button);
            button->signal_clicked().connect([this, d, id = p.id] {
              d->hide();
              reveal_time_point(id);
              Glib::signal_idle().connect_once([d] { delete d; });
            });
          }
        for (const auto &e : view->events)
          if (e.ordinal == at) {
            any = true;
            message(*records, e.name);
          }
        for (const auto &hit : view->references)
          if (std::find(hit.days.begin(),hit.days.end(),day.day)!=hit.days.end()) {
            any = true;
            const auto &r = hit.reference;
            message(*records,r.label + " — " + project::format_temporal_reference(service_.worldbuilding().calendar(r.calendar_id),r));
          }
        if (!any)
          message(*records, "Nenhum registro exibido neste dia.");
      } catch (const std::exception &) {
        message(*records, "Escolha um dia para ver seus registros.");
      }
    };
    auto render = [=, this] {
      try {
        if (service_.worldbuilding().binding(axis) != base ||
            service_.worldbuilding().adoptions(axis) != history)
          throw std::runtime_error("Adoções mudaram; reabra a consulta");
        const auto month = picker->visible_month();
        *view = service_.worldbuilding().month_view(*binding, month.year,
                                                    month.month, filter->get_active_id());
        std::map<int, std::size_t> marks;
        for (const auto &p : view->points)
          ++marks[project::calendar_date(*calendar, p.ordinal,
                                         binding->epoch_ordinal)
                      .day];
        for (const auto &e : view->events)
          ++marks[project::calendar_date(*calendar, e.ordinal,
                                         binding->epoch_ordinal)
                      .day];
        for (const auto &hit : view->references) for (const auto day : hit.days) ++marks[day];
        picker->set_marks(marks);
        std::map<int,std::string> notes;
        for(const auto &[day,state]:view->regimes) notes[day]=state=="active"?"vigência principal":state=="parallel"?"apresentação paralela":"fora da vigência";
        for(const auto &[day,cut]:view->cuts) notes[day]+=" · "+cut;
        picker->set_day_notes(notes);
        status->set_text(
            std::to_string(view->matching_points) + " marcos · " +
            std::to_string(view->matching_events) + " acontecimentos · " +
            std::to_string(view->references.size()) + " referências/intervalos neste mês" +
            (view->matching_points > 500 || view->matching_events > 500
                 ? " — consulta parcial; até 500 de cada"
                 : ""));
        selected();
        while(auto *child=unpositioned_rows->get_first_child()) unpositioned_rows->remove(*child);
        unpositioned->set_visible(!view->unpositioned.empty());
        unpositioned->set_label(std::to_string(view->unpositioned.size())+" referências sem posição comprovável — não atribuídas a este mês");
        for(const auto &r:view->unpositioned) message(*unpositioned_rows,r.label+" — "+project::format_temporal_reference(service_.worldbuilding().calendar(r.calendar_id),r));
      } catch (const std::exception &e) {
        status->set_text(author::diagnostic(e.what()));
        *view = {};
        unpositioned->set_visible(false);
        picker->set_marks({});
        picker->set_day_notes({});
        selected();
      }
    };
    picker->signal_month_changed().connect(render);
    picker->signal_changed().connect(selected);
    filter->signal_changed().connect(render);
    lens->signal_changed().connect([=, this] {
      try {
        auto index = bounded_number(lens->get_active_id(), 128);
        *binding = choices.at(index);
        *calendar = service_.worldbuilding().calendar(binding->calendar_id);
        std::string description =
            index ? "Uso declarado a partir da ordem " +
                        std::to_string(history[index - 1].from_ordinal)
                  : "Sistema de origem da linha do tempo";
        for (std::size_t n = index; n < history.size(); ++n)
          if (history[n].mode == "retire") {
            description += "; aposentado a partir da ordem " +
                           std::to_string(history[n].from_ordinal);
            break;
          }
        regime->set_text(description +
                         ". A grade é uma conversão para consulta; não "
                         "autoriza uso vigente fora desse período.");
        picker->set_calendar(*calendar);
        picker->set_selection({});
        if (persistence::CalendarNavigationStore{}.load(*calendar)) {
          // set_calendar restored navigation; selection remains empty.
        } else if (index)
          picker->show_month(
              project::calendar_date(*calendar, history[index - 1].from_ordinal,
                                     binding->epoch_ordinal));
        else {
          persistence::PlanningQuery q;
          q.limit = 1;
          auto points = service_.planning().time_points(axis, q);
          if (!points.empty())
            picker->show_month(project::calendar_date(
                *calendar, points.front().ordinal, binding->epoch_ordinal));
        }
        render();
      } catch (const std::exception &e) {
        status->set_text(author::diagnostic(e.what()));
      }
    });
    lens->set_active_id(std::to_string(choices.size() - 1));
    d->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível consultar o calendário", e);
  }
}
} // namespace inde::ui
