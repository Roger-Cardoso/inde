#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/calendar_picker.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/persistence/calendar_navigation_store.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace inde::ui {
namespace {
std::int64_t integer(const Glib::ustring &value) {
  auto s = project::trim_world_text(value.raw());
  std::int64_t n{};
  auto [p, e] = std::from_chars(s.data(), s.data() + s.size(), n);
  if (s.empty() || e != std::errc{} || p != s.data() + s.size())
    throw std::runtime_error("Informe um ano inteiro válido");
  return n;
}
} // namespace
CalendarPicker::CalendarPicker(bool partial, bool clock)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8), partial_(partial),
      clock_(clock) {
  add_css_class("calendar-picker");
  set_halign(Gtk::Align::START);
  set_hexpand(false);
  set_accessible_label(*this, "Calendário gráfico adaptativo");
  header_.append(previous_);
  header_.append(month_);
  header_.append(year_);
  header_.append(next_);
  month_.set_hexpand(true);
  year_.set_width_chars(8);
  year_.set_max_width_chars(10);
  set_accessible_label(year_, "Ano da visualização — navegar não escolhe data");
  set_accessible_label(month_, "Mês ou período da visualização");
  set_accessible_label(previous_, "Período anterior");
  set_accessible_label(next_, "Próximo período");
  append(header_);
  endpoints_.append(lower_button_);
  endpoints_.append(upper_button_);
  append(endpoints_);
  endpoints_.set_visible(false);
  grid_.set_column_homogeneous(true);
  grid_.set_column_spacing(2);
  grid_.set_row_spacing(2);
  append(grid_);
  summary_.set_wrap(true);
  summary_.set_max_width_chars(56);
  summary_.set_xalign(0);
  append(summary_);
  options_.append(year_only_);
  options_.append(month_only_);
  options_.append(clear_);
  append(options_);
  year_only_.set_visible(partial_);
  month_only_.set_visible(partial_);
  clock_expander_.set_child(clock_box_);
  clock_expander_.set_visible(clock_);
  append(clock_expander_);
  direct_expander_.set_child(direct_box_);
  direct_box_.append(direct_);
  auto *hint = Gtk::make_managed<Gtk::Label>(
      "Ano; número do mês/período; dia. Ex.: 2024; 2; 29. Só ano ou ano; "
      "período preservam precisão. Enter aplica ao rascunho.");
  hint->set_wrap(true);
  hint->set_max_width_chars(56);
  hint->set_xalign(0);
  direct_box_.append(*hint);
  append(direct_expander_);
  set_accessible_label(direct_,
                       "Entrada direta de data no calendário selecionado");
  direct_.signal_activate().connect([this] {
    try {
      if (!available_)
        throw std::runtime_error("Escolha um calendário");
      auto b = project::calendar_direct_boundary(calendar_, direct_.get_text());
      if (!partial_ && (!b.month || !b.day))
        throw std::runtime_error("Este uso exige um dia completo");
      (upper_active_ ? upper_ : lower_) = b;
      show_month({b.year, b.month.value_or(1),
                  project::calendar_first_day(calendar_)});
      clock_fields();
      summarize();
      changed_.emit();
    } catch (const std::exception &e) {
      status_.set_text(author::diagnostic(e.what()));
    }
  });
  status_.set_wrap(true);
  status_.set_max_width_chars(56);
  status_.set_xalign(0);
  status_.add_css_class("dim-label");
  append(status_);
  previous_.signal_clicked().connect([this] { navigate(-1); });
  next_.signal_clicked().connect([this] { navigate(1); });
  year_.signal_activate().connect([this] {
    try {
      auto y = integer(year_.get_text());
      auto ms = project::calendar_months(calendar_, y);
      // Navigating a year never changes the author's selection or guesses an
      // intercalary month.
      show_month({y, 1, project::calendar_first_day(calendar_)});
    } catch (const std::exception &e) {
      status_.set_text(author::diagnostic(e.what()));
    }
  });
  year_.set_tooltip_text(
      "Digite o ano e pressione Enter. Esta navegação não altera a escolha.");
  month_.signal_changed().connect([this] {
    if (updating_ || !available_)
      return;
    try {
      show_month({visible_.year,
                  static_cast<int>(integer(month_.get_active_id())),
                  project::calendar_first_day(calendar_)});
    } catch (const std::exception &e) {
      status_.set_text(author::diagnostic(e.what()));
    }
  });
  auto activate = [this](bool upper) {
    try {
      auto &old = upper_active_ ? upper_ : lower_;
      if (old)
        old = boundary(upper_active_);
      upper_active_ = upper;
      clock_fields();
      summarize();
    } catch (const std::exception &e) {
      status_.set_text(author::diagnostic(e.what()));
    }
  };
  lower_button_.signal_clicked().connect([activate] { activate(false); });
  upper_button_.signal_clicked().connect([activate] { activate(true); });
  year_only_.signal_clicked().connect([this] { choose({}, {}); });
  month_only_.signal_clicked().connect([this] { choose(visible_.month, {}); });
  clear_.signal_clicked().connect([this] {
    (upper_active_ ? upper_ : lower_).reset();
    clock_fields();
    summarize();
    changed_.emit();
  });
  auto keys = Gtk::EventControllerKey::create();
  keys->signal_key_pressed().connect(sigc::mem_fun(*this, &CalendarPicker::key),
                                     false);
  grid_.add_controller(keys);
  summarize();
}
void CalendarPicker::set_calendar(project::FictionalCalendar c) {
  project::validate(c);
  const bool same = available_ && calendar_ == c;
  calendar_ = std::move(c);
  available_ = true;
  if (!same) {
    lower_.reset();
    upper_.reset();
    upper_active_ = false;
    marks_.clear();
    day_notes_.clear();
    visible_ = {calendar_.engine == "chinese" ? 1901 : 1, 1,
                project::calendar_first_day(calendar_)};
    if (auto remembered = persistence::CalendarNavigationStore{}.load(calendar_))
      visible_ = *remembered;
  }
  render();
  clock_fields();
  summarize();
  month_changed_.emit();
  changed_.emit();
}
void CalendarPicker::set_selection(
    std::optional<project::TemporalBoundary> lo,
    std::optional<project::TemporalBoundary> hi) {
  if (!available_ && (lo || hi))
    throw std::runtime_error("Escolha o calendário e relógio antes da data");
  for (const auto &b : {lo, hi})
    if (b) {
      project::TemporalReference r;
      r.label = "Prévia";
      r.year = b->year;
      r.month = b->month;
      r.day = b->day;
      r.clock = b->clock;
      project::validate(calendar_, r);
    }
  lower_ = std::move(lo);
  upper_ = std::move(hi);
  upper_active_ = false;
  if (lower_)
    visible_ = {lower_->year, lower_->month.value_or(1),
                project::calendar_first_day(calendar_)};
  render();
  clock_fields();
  summarize();
  month_changed_.emit();
  changed_.emit();
}
void CalendarPicker::set_range(bool value) {
  range_ = value;
  endpoints_.set_visible(value);
  upper_active_ = false;
  clock_fields();
  summarize();
}
project::TemporalBoundary CalendarPicker::boundary(bool upper) const {
  const auto &b = upper ? upper_ : lower_;
  if (!available_ || !b)
    throw std::runtime_error(upper ? "Escolha o limite final"
                                   : "Escolha uma data, mês ou ano");
  if ((upper == upper_active_) && clock_) {
    auto out = *b;
    out.clock.clear();
    bool gap = false;
    for (std::size_t i = 0; i < clock_fields_.size(); ++i) {
      auto s = clock_fields_[i]->get_text();
      if (s.empty()) {
        gap = true;
        continue;
      }
      auto n = integer(s);
      if (gap || n < 0 || n >= calendar_.clock[i].parts)
        throw std::runtime_error("Horário inválido: preencha unidades em "
                                 "sequência e dentro dos limites");
      out.clock.push_back(static_cast<int>(n));
    }
    return out;
  }
  return *b;
}
project::CalendarDate CalendarPicker::date() const {
  auto b = boundary();
  if (!b.month || !b.day)
    throw std::runtime_error("Escolha um dia completo no calendário");
  return {b.year, *b.month, *b.day};
}
void CalendarPicker::show_month(project::CalendarDate d) {
  if (!available_)
    return;
  (void)project::calendar_grid(calendar_, d.year, d.month);
  visible_ = d;
  visible_.day = project::calendar_first_day(calendar_);
  persistence::CalendarNavigationStore{}.remember(calendar_, visible_);
  marks_.clear();
  day_notes_.clear();
  render();
  summarize();
  month_changed_.emit();
}
void CalendarPicker::navigate(int delta) {
  try {
    show_month(project::calendar_adjacent_month(calendar_, visible_, delta));
  } catch (const std::exception &e) {
    status_.set_text(author::diagnostic(e.what()));
  }
}
void CalendarPicker::set_marks(const std::map<int, std::size_t> &marks) {
  marks_ = marks;
  summarize();
}
void CalendarPicker::set_day_notes(const std::map<int, std::string> &notes) {
  day_notes_ = notes;
  summarize();
}
void CalendarPicker::render() {
  if (!available_)
    return;
  auto view = project::calendar_grid(calendar_, visible_.year, visible_.month);
  updating_ = true;
  month_.remove_all();
  int i = 0;
  for (const auto &m : project::calendar_months(calendar_, visible_.year))
    month_.append(std::to_string(++i), m.name);
  month_.set_active_id(std::to_string(visible_.month));
  year_.set_text(std::to_string(visible_.year));
  updating_ = false;
  days_.clear();
  if (auto *window = dynamic_cast<Gtk::Window *>(get_root()))
    if (auto *focus = window->get_focus();
        focus && gtk_widget_is_ancestor(GTK_WIDGET(focus->gobj()),
                                        GTK_WIDGET(grid_.gobj())))
      gtk_window_set_focus(GTK_WINDOW(window->gobj()), nullptr);
  while (auto *w = grid_.get_first_child())
    grid_.remove(*w);
  if (view.weekly)
    for (int col = 0; col < view.columns; ++col) {
      auto *l = Gtk::make_managed<Gtk::Label>(calendar_.weekdays[col]);
      l->set_ellipsize(Pango::EllipsizeMode::END);
      l->set_max_width_chars(5);
      l->set_tooltip_text(calendar_.weekdays[col]);
      l->add_css_class("calendar-weekday");
      grid_.attach(*l, col, 0);
    }
  for (const auto &cell : view.cells) {
    auto *b = Gtk::make_managed<Gtk::Button>(std::to_string(cell.date.day));
    b->add_css_class("calendar-day");
    b->set_hexpand(true);
    b->set_focusable(false);
    auto description = project::format_calendar_date(calendar_, cell.date) +
                       " — " + calendar_.name;
    if (!view.weekly && cell.weekday >= 0)
      description += " — " + calendar_.weekdays[cell.weekday];
    set_accessible_label(*b, description);
    b->set_tooltip_text(description);
    b->signal_clicked().connect(
        [this, day = cell.date.day] { choose(visible_.month, day); });
    grid_.attach(*b, cell.column, cell.row + (view.weekly ? 1 : 0));
    days_.push_back(b);
  }
  focus_index_ =
      std::clamp(focus_index_, 0, static_cast<int>(days_.size()) - 1);
  days_[focus_index_]->set_focusable(true);
  auto [min, max] = project::calendar_year_limits(calendar_);
  previous_.set_sensitive(visible_.year > min || visible_.month > 1);
  next_.set_sensitive(
      visible_.year < max ||
      visible_.month <
          static_cast<int>(
              project::calendar_months(calendar_, visible_.year).size()));
  status_.set_text(
      !view.weekly
          ? (calendar_.weekdays.empty()
                 ? "Dias em sequência — este calendário e relógio não define uma semana"
                 : "Semana própria — disposição compacta; nome do dia na "
                   "descrição")
          : "");
}
void CalendarPicker::choose(std::optional<int> month, std::optional<int> day) {
  if (!available_)
    return;
  auto &b = upper_active_ ? upper_ : lower_;
  b = project::TemporalBoundary{visible_.year, month, day, {}};
  if (day) {
    focus_index_ = *day - project::calendar_first_day(calendar_);
    for (std::size_t i = 0; i < days_.size(); ++i)
      days_[i]->set_focusable(static_cast<int>(i) == focus_index_);
  }
  if (range_ && !upper_active_ && !upper_)
    upper_active_ = true;
  clock_fields();
  summarize();
  changed_.emit();
}
void CalendarPicker::clock_fields() {
  clock_fields_.clear();
  if (auto *window = dynamic_cast<Gtk::Window *>(get_root()))
    if (auto *focus = window->get_focus();
        focus && gtk_widget_is_ancestor(GTK_WIDGET(focus->gobj()),
                                        GTK_WIDGET(clock_box_.gobj())))
      gtk_window_set_focus(GTK_WINDOW(window->gobj()), nullptr);
  while (auto *w = clock_box_.get_first_child())
    clock_box_.remove(*w);
  if (!clock_ || !available_)
    return;
  auto &b = upper_active_ ? upper_ : lower_;
  clock_expander_.set_label(range_ && upper_active_
                                ? "Horário do fim (opcional)"
                                : "Adicionar horário (opcional)");
  clock_expander_.set_expanded(b && !b->clock.empty());
  for (std::size_t i = 0; i < calendar_.clock.size(); ++i) {
    auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    auto *name = Gtk::make_managed<Gtk::Label>(calendar_.clock[i].singular);
    name->set_hexpand(true);
    name->set_xalign(0);
    row->append(*name);
    auto *field = Gtk::make_managed<Gtk::Entry>();
    field->set_width_chars(6);
    field->set_max_width_chars(8);
    set_accessible_label(*field,
                         calendar_.clock[i].singular +
                             (upper_active_ ? " do fim" : " do início"));
    field->set_placeholder_text("—");
    field->set_tooltip_text("0 a " +
                            std::to_string(calendar_.clock[i].parts - 1) +
                            "; vazio = desconhecido");
    if (b && i < b->clock.size())
      field->set_text(std::to_string(b->clock[i]));
    field->set_sensitive(b && b->day.has_value());
    row->append(*field);
    clock_box_.append(*row);
    clock_fields_.push_back(field);
    field->signal_changed().connect([this] {
      try {
        auto draft = boundary(upper_active_);
        (upper_active_ ? upper_ : lower_) = draft;
            status_.set_text("");
        summarize();
        changed_.emit();
      } catch (const std::exception &e) {
        status_.set_text(author::diagnostic(e.what()));
      }
    });
  }
}
void CalendarPicker::summarize() {
  auto describe = [this](const auto &b) {
    if (!b)
      return std::string("não informado");
    project::TemporalReference r;
    r.label = "Prévia";
    r.year = b->year;
    r.month = b->month;
    r.day = b->day;
    r.clock = b->clock;
    return project::format_temporal_reference(calendar_, r);
  };
  summary_.set_text(!available_
                        ? "Escolha um calendário e relógio de calendário"
                        : (range_ ? "Início: " : "Data: ") + describe(lower_) +
                              (range_ ? "\nFim: " + describe(upper_) : ""));
  lower_button_.set_label(upper_active_ ? "Editar início" : "Editando início");
  upper_button_.set_label(upper_active_ ? "Editando fim" : "Editar fim");
  if (!available_)
    return;
  const int first = project::calendar_first_day(calendar_);
  for (std::size_t i = 0; i < days_.size(); ++i) {
    auto *button = days_[i];
    const int day = first + static_cast<int>(i);
    auto matches = [&](const auto &b) {
      return b && b->year == visible_.year && b->month == visible_.month &&
             b->day == day;
    };
    for (auto css :
         {"calendar-selected", "calendar-in-range", "calendar-marked", "calendar-cut"})
      button->remove_css_class(css);
    bool selected = matches(lower_) || (range_ && matches(upper_));
    if (selected)
      button->add_css_class("calendar-selected");
    if (range_ && lower_ && upper_ && lower_->month && lower_->day &&
        upper_->month && upper_->day) {
      auto lo = project::calendar_ordinal(
          calendar_, {lower_->year, *lower_->month, *lower_->day});
      auto hi = project::calendar_ordinal(
          calendar_, {upper_->year, *upper_->month, *upper_->day});
      auto at = project::calendar_ordinal(calendar_,
                                          {visible_.year, visible_.month, day});
      if (at > lo && at < hi)
        button->add_css_class("calendar-in-range");
    }
    auto mark = marks_.find(day);
    if (mark != marks_.end())
      button->add_css_class("calendar-marked");
    auto note = day_notes_.find(day);
    const bool cut = note != day_notes_.end() && note->second.find("Adoção:") != std::string::npos;
    if (cut) button->add_css_class("calendar-cut");
    button->set_label(
        std::to_string(day) +
        (mark != marks_.end() ? " · " + std::to_string(mark->second) : "") + (cut ? " †" : ""));
    auto label = project::format_calendar_date(
                     calendar_, {visible_.year, visible_.month, day}) +
                 " — " + calendar_.name;
    if (!calendar_.weekdays.empty())
      label += " — " + calendar_.weekdays[project::calendar_weekday(
                           calendar_, {visible_.year, visible_.month, day})];
    if (selected)
      label += " — selecionado";
    if (mark != marks_.end())
      label += " — " + std::to_string(mark->second) + " registros";
    if (auto note = day_notes_.find(day); note != day_notes_.end()) label += " — " + note->second;
    set_accessible_label(*button, label);
    button->set_tooltip_text(label);
  }
}
void CalendarPicker::focus_day(int index) {
  if (days_.empty())
    return;
  for (auto *b : days_)
    b->set_focusable(false);
  focus_index_ = std::clamp(index, 0, static_cast<int>(days_.size()) - 1);
  days_[focus_index_]->set_focusable(true);
  days_[focus_index_]->grab_focus();
}
bool CalendarPicker::key(guint value, guint, Gdk::ModifierType modifiers) {
  const auto mask=static_cast<unsigned>(modifiers)&(GDK_CONTROL_MASK|GDK_SHIFT_MASK|GDK_ALT_MASK|GDK_SUPER_MASK|GDK_META_MASK|GDK_HYPER_MASK);
  if(!available_ || days_.empty() || (mask!=0 && !(mask==GDK_SHIFT_MASK && (value==GDK_KEY_Page_Up || value==GDK_KEY_Page_Down))))return false;
  bool focused=false;
  for (std::size_t i = 0; i < days_.size(); ++i)
    if (days_[i]->has_focus()) {
      focus_index_ = static_cast<int>(i);
      focused=true;
    }
  if(!focused)return false;
  const int columns =
      project::calendar_grid(calendar_, visible_.year, visible_.month).columns;
  if (value == GDK_KEY_Left)
    focus_day(focus_index_ - 1);
  else if (value == GDK_KEY_Right)
    focus_day(focus_index_ + 1);
  else if (value == GDK_KEY_Up)
    focus_day(focus_index_ - columns);
  else if (value == GDK_KEY_Down)
    focus_day(focus_index_ + columns);
  else if (value == GDK_KEY_Home)
    focus_day(0);
  else if (value == GDK_KEY_End)
    focus_day(static_cast<int>(days_.size()) - 1);
  else if (value == GDK_KEY_Page_Up || value == GDK_KEY_Page_Down) {
    Glib::signal_idle().connect_once(sigc::track_obj(
        [this, value, mask] {
          if(mask==GDK_SHIFT_MASK) {
            try {
              if((value==GDK_KEY_Page_Up && visible_.year==std::numeric_limits<std::int64_t>::min()) ||
                 (value==GDK_KEY_Page_Down && visible_.year==std::numeric_limits<std::int64_t>::max()))throw std::runtime_error("Ano fora do limite do calendário");
              const auto y=visible_.year+(value==GDK_KEY_Page_Up ? -1 : 1);
              const auto months=project::calendar_months(calendar_,y);
              show_month({y,std::min(visible_.month,static_cast<int>(months.size())),project::calendar_first_day(calendar_)});
            } catch(const std::exception &error) {status_.set_text(author::diagnostic(error.what()));}
          } else navigate(value == GDK_KEY_Page_Up ? -1 : 1);
          focus_day(focus_index_);
        },
        *this));
  } else
    return false;
  return true;
}
} // namespace inde::ui
