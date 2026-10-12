#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/temporal_context.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/temporal_reference_editor.hpp"
#include "temporal_form_helpers.hpp"
#include <memory>
namespace inde::ui {
namespace {
Gtk::Label *message(Gtk::Box &box, const std::string &text) {
  auto *l = Gtk::make_managed<Gtk::Label>(text);
  l->set_xalign(0);
  l->set_wrap(true);
  box.append(*l);
  return l;
}
} // namespace
void append_time_point_calendar(IncrementalSelector &selector,
                                application::ProjectService &s) {
  auto *expand =
      Gtk::make_managed<Gtk::Expander>("Escolher marco no calendário…");
  selector.append(*expand);
  auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
  expand->set_child(*box);
  auto built = std::make_shared<bool>(false);
  expand->property_expanded().signal_changed().connect([=, &selector, &s] {
    if (!expand->get_expanded() || *built)
      return;
    *built = true;
    if (!s.current()) {
      message(*box, "Abra um Projeto para consultar marcos.");
      return;
    }
    auto *axis = Gtk::make_managed<Gtk::ComboBoxText>();
    constrain_temporal_combo(*axis);
    axis->append("", "Escolha a linha do tempo com calendário");
    try {
      for (const auto &a : s.planning().time_axes())
        if (s.worldbuilding().binding(a.id))
          axis->append(a.id, a.name);
    } catch (const std::exception &e) {
      message(*box, author::diagnostic(e.what()));
      return;
    }
    axis->set_active(0);
    append_labeled_form_field(*box, "Linha do tempo do marco", *axis);
    auto *picker = Gtk::make_managed<CalendarPicker>(false, false);
    box->append(*picker);
    picker->set_visible(false);
    auto *records = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    box->append(*records);
    auto *status =
        message(*box, "A grade localiza marcos existentes; não transforma uma "
                      "data em marco nem cria registros.");
    auto binding = std::make_shared<std::optional<project::AxisCalendar>>();
    auto calendar = std::make_shared<project::FictionalCalendar>();
    auto view = std::make_shared<application::CalendarMonthView>();
    auto selected = [=, &selector] {
      while (auto *child = records->get_first_child())
        records->remove(*child);
      try {
        if (!*binding)
          return;
        const auto day = picker->date(), month = picker->visible_month();
        if (day.year != month.year || day.month != month.month) {
          message(*records, "A escolha anterior foi preservada. Escolha um dia "
                            "deste mês para consultar marcos.");
          return;
        }
        const auto at = project::calendar_ordinal(*calendar, day,
                                                  (**binding).epoch_ordinal);
        bool any = false;
        for (const auto &p : view->points)
          if (p.ordinal == at) {
            any = true;
            auto *b = Gtk::make_managed<Gtk::Button>(p.label);
            records->append(*b);
            b->signal_clicked().connect([=, &selector] {
              selector.set_selected(p.id, p.label);
              expand->set_expanded(false);
            });
          }
        if (!any)
          message(*records, "Nenhum marco exibido neste dia. Use a busca "
                            "textual para marcos sem data.");
      } catch (const std::exception &) {
      }
    };
    auto render = [=, &s] {
      try {
        if (!*binding)
          return;
        const auto date = picker->visible_month();
        *view = s.worldbuilding().month_view(**binding, date.year, date.month);
        std::map<int, std::size_t> marks;
        for (const auto &p : view->points)
          ++marks[project::calendar_date(*calendar, p.ordinal,
                                         (**binding).epoch_ordinal)
                      .day];
        picker->set_marks(marks);
        status->set_text(
            std::to_string(view->matching_points) + " marcos neste mês" +
            (view->matching_points > 500 ? " — até 500 exibidos" : ""));
      } catch (const std::exception &e) {
        *view = {};
        picker->set_marks({});
        status->set_text(author::diagnostic(e.what()));
      }
      selected();
    };
    picker->signal_month_changed().connect(
        sigc::track_obj(render, *axis, *status));
    picker->signal_changed().connect(
        sigc::track_obj(selected, *records, selector));
    axis->signal_changed().connect(sigc::track_obj(
        [=, &s] {
          try {
            *binding = axis->get_active_id().empty()
                           ? std::nullopt
                           : s.worldbuilding().binding(axis->get_active_id());
            picker->set_visible(binding->has_value());
            if (!*binding) {
              *view = {};
              while (auto *child = records->get_first_child())
                records->remove(*child);
            }
            if (*binding) {
              *calendar = s.worldbuilding().calendar((**binding).calendar_id);
              picker->set_calendar(*calendar);
            }
          } catch (const std::exception &e) {
            status->set_text(author::diagnostic(e.what()));
          }
        },
        *picker, *status));
  });
}
void show_temporal_context(Gtk::Window &owner, application::ProjectService &s,
                           const std::string &kind, const std::string &id,
                           sigc::slot<void()> changed) try {
  auto dest = s.worldbuilding().temporal_target(kind, id);
  auto pending = std::make_unique<OverlayDialog>(
      "Datas e horários — " + dest.label, owner, true);
  auto *d = pending.get();
  d->set_secondary_text(
      "Consulte, reutilize ou crie uma referência sem sair deste elemento. "
      "Significado e papel formal são declarados por você. Trocar um vínculo "
      "não edita uma data compartilhada.");
  d->add_button("Fechar", Gtk::ResponseType::CANCEL);
  d->add_button("Vincular data", Gtk::ResponseType::ACCEPT);
  auto &box = *d->get_content_area();
  auto *error = message(box, "");
  auto *uses = Gtk::make_managed<Gtk::Expander>("Datas já vinculadas");
  auto *rows = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
  uses->set_child(*rows);
  box.append(*uses);
  auto populate = [=, &s] {
    while (auto *child = rows->get_first_child())
      rows->remove(*child);
    auto refs = s.worldbuilding().target_dates(dest);
    uses->set_label(std::to_string(refs.size()) +
                    " datas vinculadas — consultar");
    for (const auto &r : refs) {
      message(*rows, r.label + " — " +
                         project::format_temporal_reference(
                             s.worldbuilding().calendar(r.calendar_id), r));
      for (const auto &a : s.worldbuilding().temporal_attachments(r.id))
        if (a.kind == dest.kind && a.target_id == dest.id) {
          message(*rows,
                  "Papel: " + a.role + (a.note.empty() ? "" : " · " + a.note));
          auto *remove = Gtk::make_managed<Gtk::CheckButton>(
              "Confirmar retirada deste vínculo");
          rows->append(*remove);
          auto *button = Gtk::make_managed<Gtk::Button>("Retirar vínculo");
          rows->append(*button);
          button->signal_clicked().connect([=, &s] {
            try {
              if (!remove->get_active())
                throw std::runtime_error("Confirme a retirada");
              s.worldbuilding().remove_date_attachment(a);
              button->set_sensitive(false);
              remove->set_sensitive(false);
              error->set_text("Vínculo retirado; a referência compartilhada "
                              "foi preservada. Reabra para atualizar a lista.");
            } catch (const std::exception &e) {
              error->set_text(author::diagnostic(e.what()));
            }
          });
        }
    }
    if (refs.empty())
      message(*rows, "Nenhuma data vinculada.");
  };
  populate();
  auto *axis = Gtk::make_managed<Gtk::ComboBoxText>();
  constrain_temporal_combo(*axis);
  axis->append("", "Escolha a linha do tempo");
  const auto axes = s.planning().time_axes();
  for (const auto &a : axes)
    axis->append(a.id, a.name);
  axis->set_active(0);
  if (axes.size() == 1)
    axis->set_active_id(axes.front().id);
  append_labeled_form_field(box, "Linha do tempo", *axis);
  if (axes.empty())
    message(box, "Crie uma linha do tempo em Planejar → Cronologia para registrar datas. Nenhuma linha do tempo será criada automaticamente.");
  auto *create =
      Gtk::make_managed<Gtk::CheckButton>("Criar nova data ou intervalo aqui");
  box.append(*create);
  auto *search = Gtk::make_managed<Gtk::SearchEntry>();
  search->set_placeholder_text("Pesquisar nome ou data");
  box.append(*search);
  auto *existing = Gtk::make_managed<Gtk::ComboBoxText>();
  constrain_temporal_combo(*existing);
  append_labeled_form_field(box, "Data ou período já registrado", *existing);
  auto refs =
      std::make_shared<std::map<std::string, project::TemporalReference>>();
  auto refresh = [=, &s] {
    existing->remove_all();
    existing->append("", "Escolha a referência");
    existing->set_active(0);
    refs->clear();
    if (axis->get_active_id().empty())
      return;
    const auto query = search->get_text().casefold();
    for (const auto &r :
         s.worldbuilding().temporal_references(axis->get_active_id())) {
      auto text = r.label + " — " +
                  project::format_temporal_reference(
                      s.worldbuilding().calendar(r.calendar_id), r);
      if (!query.empty() &&
          Glib::ustring(text).casefold().find(query) == Glib::ustring::npos)
        continue;
      refs->emplace(r.id, r);
      existing->append(r.id, text);
    }
  };
  axis->signal_changed().connect(
      sigc::track_obj(refresh, *existing, *axis, *search));
  search->signal_search_changed().connect(
      sigc::track_obj(refresh, *existing, *axis, *search));
  refresh();
  auto *draft =
      Gtk::make_managed<TemporalReferenceEditor>(s.worldbuilding().calendars());
  draft->set_visible(false);
  box.append(*draft);
  create->signal_toggled().connect([=] {
    draft->set_visible(create->get_active());
    existing->set_visible(!create->get_active());
    search->set_visible(!create->get_active());
  });
  auto *role = Gtk::make_managed<Gtk::Entry>();
  append_labeled_form_field(
      box, "Significado da data", *role,
      "Ex.: nascimento, início, lembrança. Nada é inferido.");
  auto *advanced =
      Gtk::make_managed<Gtk::Expander>("Papel formal e nota (opcionais)");
  auto *fields = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
  advanced->set_child(*fields);
  box.append(*advanced);
  auto *meaning = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &[key, text] :
       std::vector<std::pair<std::string, std::string>>{
           {"unspecified", "Somente significado livre"},
           {"birth", "Nascimento"},
           {"death", "Morte"},
           {"start", "Início"},
           {"end", "Fim"},
           {"occurrence", "Quando acontece"},
           {"mention", "Menção"}})
    meaning->append(key, text);
  meaning->set_active_id("unspecified");
  append_labeled_form_field(*fields, "Papel formal", *meaning);
  auto *note = Gtk::make_managed<Gtk::Entry>();
  append_labeled_form_field(*fields, "Nota do vínculo", *note);
  d->signal_response().connect([=, &s](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (axis->get_active_id().empty())
          throw std::runtime_error("Escolha a linha do tempo");
        project::TemporalAttachment a;
        a.kind = dest.kind;
        a.target_id = dest.id;
        a.role = role->get_text();
        a.meaning = meaning->get_active_id();
        a.note = note->get_text();
        if (create->get_active()) {
          auto r = draft->draft(axis->get_active_id());
          s.worldbuilding().create_attached_date(r, draft->calendar(), a, dest);
        } else {
          auto it = refs->find(existing->get_active_id());
          if (it == refs->end())
            throw std::runtime_error("Escolha uma referência");
          a.reference_id = it->first;
          s.worldbuilding().attach_date(a, it->second, dest);
        }
      } catch (const std::exception &e) {
        error->set_text(std::string("Não aplicado: ") + author::diagnostic(e.what()));
        d->keep_open_on_response();
        return;
      }
    }
    d->hide();
    if (!changed.empty()) {
      try {
        changed();
      } catch (const std::exception &) {
      } // a derived summary cannot undo a successful save
    }
  });
  d->present();
  pending.release(); // response lifetime is owned by OverlayDialog
} catch (const std::exception &e) {
  auto *d = new OverlayDialog("Consulta temporal indisponível", owner, true);
  d->set_secondary_text(author::diagnostic(e.what()));
  d->add_button("Fechar", Gtk::ResponseType::CLOSE);
  d->present();
}
Gtk::Button *append_temporal_context(Gtk::Box &box, Gtk::Window &owner,
                                     application::ProjectService &s,
                                     const std::string &kind,
                                     const std::string &id) {
  auto *button = Gtk::make_managed<Gtk::Button>("Datas e horários…");
  button->set_halign(Gtk::Align::START);
  box.append(*button);
  try {
    auto dest = s.worldbuilding().temporal_target(kind, id);
    auto refs = s.worldbuilding().target_dates(dest);
    std::string text = std::to_string(refs.size()) + " datas vinculadas";
    button->set_label("Datas e horários (" + std::to_string(refs.size()) +
                      ")…");
    if (!refs.empty())
      text += " · " + refs.front().label;
    button->set_tooltip_text(text);
  } catch (const std::exception &e) {
    button->set_tooltip_text(author::diagnostic(e.what()));
  }
  button->signal_clicked().connect([&owner, &s, kind, id, button] {
    try {
      show_temporal_context(
          owner, s, kind, id,
          sigc::track_obj(
              [&s, kind, id, button] {
                auto target = s.worldbuilding().temporal_target(kind, id);
                button->set_label(
                    "Datas e horários (" +
                    std::to_string(
                        s.worldbuilding().target_dates(target).size()) +
                    ")…");
              },
              *button));
    } catch (const std::exception &e) {
      auto *d =
          new OverlayDialog("Consulta temporal indisponível", owner, true);
      d->set_secondary_text(author::diagnostic(e.what()));
      d->add_button("Fechar", Gtk::ResponseType::CLOSE);
      d->present();
    }
  });
  return button;
}
} // namespace inde::ui
