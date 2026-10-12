#include "inde/project/worldbuilding.hpp"
#include "calendar_engines.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <glib.h>
#include <limits>
#include <set>
#include <stdexcept>
namespace inde::project {
namespace {
void text(const std::string &v, std::size_t max, bool required = true) {
  if (v.size() > max || v.find('\0') != std::string::npos ||
      !g_utf8_validate(v.data(), v.size(), nullptr) ||
      (required && trim_world_text(v).empty()))
    throw std::runtime_error(
        "Texto vazio, inválido ou acima do limite permitido");
  for (const char *p = v.data(); p < v.data() + v.size();
       p = g_utf8_next_char(p))
    if (g_unichar_iscntrl(g_utf8_get_char(p)))
      throw std::runtime_error("Texto contém caracteres de controle");
}
std::int64_t floor_div(std::int64_t a, std::int64_t b) {
  auto q = a / b;
  if (a % b < 0)
    --q;
  return q;
}
std::int64_t mod(std::int64_t a, std::int64_t b) {
  auto r = a % b;
  return r < 0 ? r + b : r;
}
void year(std::int64_t y) {
  if (y < -1000000 || y > 1000000)
    throw std::runtime_error("Ano fora do intervalo -1000000 a 1000000");
}
bool leap(const FictionalCalendar &c, std::int64_t y) {
  if (c.engine == "gregorian")
    return mod(y, 4) == 0 && (mod(y, 100) != 0 || mod(y, 400) == 0);
  if (c.engine == "julian")
    return mod(y, 4) == 0;
  return c.leap_cycle && mod(y, c.leap_cycle) == c.leap_phase;
}
std::int64_t before(const FictionalCalendar &c, std::int64_t y) {
  std::int64_t days{};
  for (const auto &m : c.months)
    days += m.days;
  if (c.engine == "gregorian")
    return 365 * y + floor_div(y - 1, 4) - floor_div(y - 1, 100) +
           floor_div(y - 1, 400) + 1;
  if (c.engine == "julian")
    return 365 * y + floor_div(y - 1, 4) + 1;
  return days * y + (c.leap_cycle
                         ? floor_div(y - 1 - c.leap_phase, c.leap_cycle) -
                               floor_div(-1 - c.leap_phase, c.leap_cycle)
                         : 0);
}
std::int64_t add(std::int64_t a, std::int64_t b) {
  if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) ||
      (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b))
    throw std::runtime_error("Data excede o intervalo dos ordinais");
  return a + b;
}
std::int64_t subtract(std::int64_t a, std::int64_t b) {
  if ((b > 0 && a < std::numeric_limits<std::int64_t>::min() + b) ||
      (b < 0 && a > std::numeric_limits<std::int64_t>::max() + b))
    throw std::runtime_error("Ordinal fora do intervalo deste calendário");
  return a - b;
}
} // namespace
std::string trim_world_text(const std::string &v) {
  if (v.find('\0') != std::string::npos ||
      !g_utf8_validate(v.data(), v.size(), nullptr))
    throw std::runtime_error("Texto UTF-8 inválido");
  const char *start = v.data(), *end = start + v.size();
  while (start < end && g_unichar_isspace(g_utf8_get_char(start)))
    start = g_utf8_next_char(start);
  while (end > start) {
    auto p = g_utf8_find_prev_char(start, end);
    if (!p || !g_unichar_isspace(g_utf8_get_char(p)))
      break;
    end = p;
  }
  return {start, end};
}
std::string alias_key(const std::string &v) {
  if (v.size() > 512)
    throw std::runtime_error("Nome alternativo acima de 512 bytes");
  auto t = trim_world_text(v);
  text(t, 512);
  auto *n = g_utf8_normalize(t.c_str(), t.size(), G_NORMALIZE_NFC);
  auto *fold = g_utf8_casefold(n, -1);
  g_free(n);
  auto *normalized = g_utf8_normalize(fold, -1, G_NORMALIZE_NFC);
  g_free(fold);
  std::string result(normalized);
  g_free(normalized);
  return result;
}
void validate(const EntityAlias &a) {
  if (!g_uuid_string_is_valid(a.id.c_str()) ||
      !g_uuid_string_is_valid(a.entity_id.c_str()))
    throw std::runtime_error("Identidade do alias inválida");
  (void)alias_key(a.name);
  text(a.note, 2048, false);
  if (a.kind != "nickname" && a.kind != "title" && a.kind != "spelling" &&
      a.kind != "other")
    throw std::runtime_error("Categoria de alias inválida");
}
void validate(const TemporalAttachment &a) {
  for (const auto &id : {a.id, a.reference_id, a.target_id, a.token})
    if (!g_uuid_string_is_valid(id.c_str()))
      throw std::runtime_error("Identidade do vínculo temporal inválida");
  if (a.kind != "entity" && a.kind != "relation" && a.kind != "unit" &&
      a.kind != "line")
    throw std::runtime_error("Tipo de destino temporal inválido");
  text(a.role, 128);
  text(a.note, 2048, false);
  const std::vector<std::string> meanings{
      "unspecified", "birth", "death", "start", "end", "occurrence", "mention"};
  if (std::find(meanings.begin(), meanings.end(), a.meaning) == meanings.end())
    throw std::runtime_error("Significado formal temporal desconhecido");
}
void validate(const FictionalCalendar &c) {
  if (!g_uuid_string_is_valid(c.id.c_str()))
    throw std::runtime_error("Identidade de calendário inválida");
  text(c.name, 256);
  auto engines = calendar_template_engines();
  if (c.engine != "custom" &&
      std::find(engines.begin(), engines.end(), c.engine) == engines.end())
    throw std::runtime_error("Motor de calendário desconhecido");
  if (c.engine == "gregorian" || c.engine == "julian") {
    const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (c.months.size() != 12 || c.leap_month != 2 || c.leap_cycle != 4 ||
        c.leap_phase != 0)
      throw std::runtime_error(
          "Regras incompatíveis com o modelo real; escolha calendário próprio");
    for (int i = 0; i < 12; ++i)
      if (c.months[i].days != lengths[i])
        throw std::runtime_error(
            "Duração de mês incompatível com o modelo real");
  }
  if (c.engine != "custom" && c.engine != "gregorian" && c.engine != "julian")
    calendar_engines::validate_rules(c);
  else if (!c.rule_revision.empty())
    throw std::runtime_error("Proveniência incompatível com este motor");
  if (c.clock.empty() || c.clock.size() > 8)
    throw std::runtime_error("Relógio precisa de 1–8 subdivisões");
  std::int64_t total = 1;
  std::set<std::string> units;
  for (const auto &u : c.clock) {
    text(u.singular, 128);
    text(u.plural, 128);
    text(u.symbol, 32, false);
    if (u.parts < 1 || u.parts > 10000 || total > 1000000000000LL / u.parts ||
        !units.insert(alias_key(u.singular)).second)
      throw std::runtime_error(
          "Subdivisão duplicada ou relógio excede os limites");
    total *= u.parts;
  }
  if (c.months.empty() || c.months.size() > 32 || c.weekdays.size() > 16)
    throw std::runtime_error(
        "Calendário admite 1–32 meses e até 16 dias da semana");
  std::set<std::string> seen;
  for (const auto &m : c.months) {
    text(m.name, 128);
    if (m.days < 1 || m.days > 60 || !seen.insert(alias_key(m.name)).second)
      throw std::runtime_error("Mês duplicado ou duração fora de 1–60 dias");
  }
  seen.clear();
  for (const auto &w : c.weekdays) {
    text(w, 128);
    if (!seen.insert(alias_key(w)).second)
      throw std::runtime_error("Dia da semana duplicado");
  }
  if (c.leap_cycle < 0 || c.leap_cycle > 10000 || c.leap_phase < 0 ||
      (c.leap_cycle ? c.leap_phase >= c.leap_cycle : c.leap_phase != 0) ||
      c.leap_month < 1 || c.leap_month > static_cast<int>(c.months.size()))
    throw std::runtime_error("Regra de intercalação inválida");
  if (c.leap_cycle && c.months[c.leap_month - 1].days == 60)
    throw std::runtime_error("O mês intercalado não pode exceder 60 dias");
}
int days_in_month(const FictionalCalendar &c, std::int64_t y, int month) {
  validate(c);
  year(y);
  if (calendar_engines::icu(c)) {
    auto months = calendar_engines::months(c, y);
    if (month < 1 || month > static_cast<int>(months.size()))
      throw std::runtime_error("Mês inexistente neste ano");
    return months[month - 1].days;
  }
  if (month < 1 || month > static_cast<int>(c.months.size()))
    throw std::runtime_error("Mês fora do calendário");
  return c.months[month - 1].days +
         (leap(c, y) && month == c.leap_month ? 1 : 0);
}
std::int64_t calendar_ordinal(const FictionalCalendar &c, CalendarDate d,
                              std::int64_t epoch) {
  validate(c);
  year(d.year);
  if (calendar_engines::icu(c))
    return add(epoch, calendar_engines::day(c, d));
  auto days = days_in_month(c, d.year, d.month);
  const auto first = calendar_first_day(c);
  if (d.day < first || d.day >= days + first)
    throw std::runtime_error("Dia inexistente neste mês e ano");
  auto offset = before(c, d.year);
  for (int m = 1; m < d.month; ++m)
    offset += days_in_month(c, d.year, m);
  return add(epoch, offset + d.day - first);
}
CalendarDate calendar_date(const FictionalCalendar &c, std::int64_t ordinal,
                           std::int64_t epoch) {
  validate(c);
  auto day = subtract(ordinal, epoch);
  if (calendar_engines::icu(c))
    return calendar_engines::date(c, day);
  if (day < before(c, -1000000) || day >= before(c, 1000001))
    throw std::runtime_error(
        "Ordinal fora dos anos suportados pelo calendário");
  std::int64_t lo = -1000000, hi = 1000001;
  while (lo + 1 < hi) {
    auto mid = lo + (hi - lo) / 2;
    if (before(c, mid) <= day)
      lo = mid;
    else
      hi = mid;
  }
  day -= before(c, lo);
  int m = 1;
  while (day >= days_in_month(c, lo, m)) {
    day -= days_in_month(c, lo, m);
    ++m;
  }
  return {lo, m, static_cast<int>(day) + calendar_first_day(c)};
}
std::string format_calendar_date(const FictionalCalendar &c, CalendarDate d) {
  (void)calendar_ordinal(c, d);
  return std::to_string(d.day) + " de " +
         calendar_months(c, d.year)[d.month - 1].name +
         (c.engine == "maya-haab" ? ", ciclo Haab "
          : c.engine == "chinese" ? ", ano associado "
                                  : ", ano ") +
         std::to_string(d.year) +
         (c.engine == "maya-haab" ? calendar_engines::maya_cycles(d) : "");
}
int calendar_weekday(const FictionalCalendar &c, CalendarDate d) {
  if (c.weekdays.empty())
    return -1;
  if (calendar_engines::icu(c))
    return static_cast<int>(mod(terrestrial_day(c, d), c.weekdays.size()));
  return static_cast<int>(
      mod(calendar_ordinal(c, d) + (c.engine == "gregorian" ? 5
                                    : c.engine == "julian"  ? 3
                                                            : 0),
          c.weekdays.size()));
}
FictionalCalendar calendar_template(const std::string &engine) {
  if (engine != "gregorian" && engine != "julian")
    return calendar_engines::model(engine);
  FictionalCalendar c;
  c.id = new_uuid();
  c.engine = engine;
  if (engine != "gregorian" && engine != "julian")
    throw std::runtime_error("Modelo real indisponível");
  c.name =
      engine == "gregorian" ? "Gregoriano proléptico" : "Juliano proléptico";
  c.months = {{"Janeiro", 31}, {"Fevereiro", 28}, {"Março", 31},
              {"Abril", 30},   {"Maio", 31},      {"Junho", 30},
              {"Julho", 31},   {"Agosto", 31},    {"Setembro", 30},
              {"Outubro", 31}, {"Novembro", 30},  {"Dezembro", 31}};
  c.weekdays = {"Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira",
                "Sexta-feira",   "Sábado",      "Domingo"};
  c.leap_cycle = 4;
  c.leap_month = 2;
  return c;
}
std::int64_t terrestrial_day(const FictionalCalendar &c, CalendarDate d) {
  if (c.engine == "custom" || c.engine == "egyptian-civil")
    throw std::runtime_error(
        "Calendário sem época terrestre declarada exige alinhamento autoral");
  if (calendar_engines::icu(c))
    return calendar_ordinal(c, d);
  if (c.engine == "maya-haab")
    return calendar_ordinal(c, d, 583935);
  return calendar_ordinal(c, d, c.engine == "gregorian" ? 1721060 : 1721058);
}
CalendarDate terrestrial_date(const FictionalCalendar &c, std::int64_t day) {
  if (c.engine == "custom" || c.engine == "egyptian-civil")
    throw std::runtime_error(
        "Calendário sem época terrestre declarada exige alinhamento autoral");
  if (calendar_engines::icu(c))
    return calendar_date(c, day);
  if (c.engine == "maya-haab")
    return calendar_date(c, day, 583935);
  return calendar_date(c, day, c.engine == "gregorian" ? 1721060 : 1721058);
}
void validate(const FictionalCalendar &c, const TemporalReference &r) {
  validate(c);
  year(r.year);
  const auto months = calendar_months(c, r.year);
  text(r.label, 256);
  text(r.note, 2048, false);
  text(r.uncertainty, 2048, false);
  if (r.qualifier != "exact" && r.qualifier != "before" &&
      r.qualifier != "after" && r.qualifier != "between" &&
      r.qualifier != "approximate" && r.qualifier != "season")
    throw std::runtime_error("Precisão temporal desconhecida");
  if ((r.qualifier == "between") != r.upper.has_value())
    throw std::runtime_error(
        "Entre exige limite final; outros modos não aceitam limite final");
  if ((r.qualifier == "approximate" || r.qualifier == "season") &&
      trim_world_text(r.uncertainty).empty())
    throw std::runtime_error(
        "Explique a aproximação ou estação; nenhum limite será inventado");
  if (r.month && (*r.month < 1 || *r.month > static_cast<int>(months.size())))
    throw std::runtime_error("Mês inexistente");
  if (r.day &&
      (!r.month || *r.day < calendar_first_day(c) ||
       *r.day >= days_in_month(c, r.year, *r.month) + calendar_first_day(c)))
    throw std::runtime_error("Dia inexistente ou sem mês definido");
  if (!r.clock.empty()) {
    if (!r.day || r.clock.size() > c.clock.size())
      throw std::runtime_error(
          "Horário exige data completa e subdivisões conhecidas");
    for (std::size_t i = 0; i < r.clock.size(); ++i)
      if (r.clock[i] < 0 || r.clock[i] >= c.clock[i].parts)
        throw std::runtime_error("Horário fora das subdivisões do relógio");
  }
  if (r.upper) {
    auto end = r;
    end.year = r.upper->year;
    end.month = r.upper->month;
    end.day = r.upper->day;
    end.clock = r.upper->clock;
    end.qualifier = "exact";
    end.upper.reset();
    validate(c, end);
    auto start = r;
    start.qualifier = "exact";
    start.upper.reset();
    const auto lo = temporal_window(c, start, 0),
               hi = temporal_window(c, end, 0);
    if (compare_temporal_coordinates(*hi.upper_bound, *lo.lower_bound) <= 0)
      throw std::runtime_error("Limites temporais invertidos");
  }
}
int compare_temporal_coordinates(TemporalCoordinate a, TemporalCoordinate b) {
  if (a.denominator <= 0 || b.denominator <= 0 || a.numerator < 0 ||
      b.numerator < 0 || a.numerator >= a.denominator ||
      b.numerator >= b.denominator)
    throw std::runtime_error("Coordenada temporal inválida");
  if (a.day != b.day)
    return a.day < b.day ? -1 : 1;
  bool reverse = false;
  for (;;) {
    const auto aq = a.numerator / a.denominator,
               bq = b.numerator / b.denominator;
    if (aq != bq)
      return (aq < bq) != reverse ? -1 : 1;
    const auto ar = a.numerator % a.denominator,
               br = b.numerator % b.denominator;
    if (!ar || !br) {
      if (ar == br)
        return 0;
      return (!ar) != reverse ? -1 : 1;
    }
    a = {0, a.denominator, ar};
    b = {0, b.denominator, br};
    reverse = !reverse;
  }
}
TemporalWindow temporal_window(const FictionalCalendar &c,
                               const TemporalReference &r, std::int64_t epoch) {
  validate(c, r);
  if (r.qualifier == "approximate" || r.qualifier == "season")
    return {};
  const int first = calendar_first_day(c);
  const auto months = calendar_months(c, r.year);
  const int m = r.month.value_or(1),
            last_m = r.month.value_or(static_cast<int>(months.size()));
  const auto low_day =
      calendar_ordinal(c, {r.year, m, r.day.value_or(first)}, epoch);
  const auto high_day = add(
      calendar_ordinal(
          c,
          {r.year, last_m, r.day.value_or(months[last_m - 1].days + first - 1)},
          epoch),
      1);
  TemporalCoordinate low{low_day, 0, 1}, high{high_day, 0, 1};
  if (!r.clock.empty()) {
    std::int64_t parts = 1, ticks = 0;
    for (std::size_t i = 0; i < r.clock.size(); ++i) {
      parts *= c.clock[i].parts;
      ticks = ticks * c.clock[i].parts + r.clock[i];
    }
    low = {low_day, ticks, parts};
    high = ticks + 1 == parts ? TemporalCoordinate{add(low_day, 1), 0, 1}
                              : TemporalCoordinate{low_day, ticks + 1, parts};
  }
  auto make = [](std::optional<TemporalCoordinate> lo,
                 std::optional<TemporalCoordinate> hi) {
    TemporalWindow w;
    w.comparable = true;
    w.lower_bound = lo;
    w.upper_bound = hi;
    auto value = [](auto v) {
      return static_cast<long double>(v.day) +
             static_cast<long double>(v.numerator) / v.denominator;
    };
    if (lo)
      w.earliest = value(*lo);
    if (hi)
      w.latest = value(*hi);
    return w;
  };
  if (r.qualifier == "before")
    return make({}, low);
  if (r.qualifier == "after")
    return make(high, {});
  if (r.upper) {
    auto end = r;
    end.year = r.upper->year;
    end.month = r.upper->month;
    end.day = r.upper->day;
    end.clock = r.upper->clock;
    end.qualifier = "exact";
    end.upper.reset();
    high = *temporal_window(c, end, epoch).upper_bound;
  }
  return make(low, high);
}
std::int64_t clock_ticks(const FictionalCalendar &c,
                         const std::vector<int> &values) {
  validate(c);
  if (values.size() != c.clock.size())
    throw std::runtime_error("Horário parcial não é instante exato");
  std::int64_t n = 0;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (values[i] < 0 || values[i] >= c.clock[i].parts)
      throw std::runtime_error("Horário inválido");
    n = n * c.clock[i].parts + values[i];
  }
  return n;
}
std::string format_temporal_reference(const FictionalCalendar &c,
                                      const TemporalReference &r) {
  validate(c, r);
  if (r.qualifier != "exact") {
    auto start = r;
    start.qualifier = "exact";
    start.upper.reset();
    auto text = format_temporal_reference(c, start);
    if (r.upper) {
      auto end = start;
      end.year = r.upper->year;
      end.month = r.upper->month;
      end.day = r.upper->day;
      end.clock = r.upper->clock;
      return "Entre " + text + " e " + format_temporal_reference(c, end);
    }
    return std::string(r.qualifier == "before"   ? "Antes de "
                       : r.qualifier == "after"  ? "Depois de "
                       : r.qualifier == "season" ? "Estação/época: "
                                                 : "Aproximadamente ") +
           text + (r.uncertainty.empty() ? "" : " — " + r.uncertainty);
  }
  std::string s = r.day ? std::to_string(*r.day) + " de " : "";
  if (r.month)
    s += calendar_months(c, r.year)[*r.month - 1].name + ", ";
  s += (c.engine == "maya-haab" ? "ciclo Haab "
        : c.engine == "chinese" ? "ano associado "
                                : "ano ") +
       std::to_string(r.year);
  if (c.engine == "maya-haab" && r.month && r.day)
    s += calendar_engines::maya_cycles({r.year, *r.month, *r.day});
  for (std::size_t i = 0; i < r.clock.size(); ++i)
    s += (i ? " · " : " — ") + std::to_string(r.clock[i]) + " " +
         (r.clock[i] == 1 ? c.clock[i].singular : c.clock[i].plural);
  return s + (r.clock.empty()                   ? " (hora não informada)"
              : r.clock.size() < c.clock.size() ? " (horário parcial)"
                                                : "");
}
int calendar_first_day(const FictionalCalendar &c) {
  return c.engine == "maya-haab" ? 0 : 1;
}
std::vector<std::string> calendar_template_engines() {
  return {"gregorian",     "julian",         "chinese",   "hebrew",
          "islamic-civil", "egyptian-civil", "maya-haab", "ethiopic"};
}
std::vector<CalendarMonth> calendar_months(const FictionalCalendar &c,
                                           std::int64_t y) {
  validate(c);
  year(y);
  if (calendar_engines::icu(c))
    return calendar_engines::months(c, y);
  auto v = c.months;
  for (std::size_t i = 0; i < v.size(); ++i)
    v[i].days = days_in_month(c, y, static_cast<int>(i + 1));
  return v;
}
std::string calendar_rule_description(const FictionalCalendar &c) {
  if (c.engine == "chinese")
    return "Lunissolar calculado pela ICU. Ano = ano gregoriano em que começa "
           "o ano lunar (1901–2100). Meses numerados em ordem naquele ano, "
           "incluindo o intercalar explicitamente nomeado. Previsões "
           "astronômicas podem divergir um dia; não é uma tabela histórica "
           "universal. " +
           c.rule_revision;
  if (c.engine == "hebrew")
    return "Hebraico calculado, era AM, anos 1–9999; ano começa em Tishri. "
           "Meses em ordem no ano: Adar I e Adar II nos anos intercalares. Não "
           "calcula pôr do sol nem observância religiosa. " +
           c.rule_revision;
  if (c.engine == "islamic-civil")
    return "Hégira civil tabular, anos 1–9999, ciclo de 30 anos. Não é "
           "observação lunar, Umm al-Qura ou previsão religiosa local. " +
           c.rule_revision;
  if (c.engine == "ethiopic")
    return "Etíope Amete Mihret, anos 1–9999; 12 meses de 30 dias e Pagumen "
           "com 5 ou 6. Não é Amete Alem. " +
           c.rule_revision;
  if (c.engine == "egyptian-civil")
    return "Civil antigo: 12 meses de 30 dias, três estações e 5 dias "
           "epagomenais fora dos meses (período 13). Sem bissextos. Ano é "
           "coordenada autoral: alinhamento manual obrigatório; não presume "
           "reinado, época ou calendário copta.";
  if (c.engine == "maya-haab")
    return "Haab: 18 períodos de 20 dias e Wayeb de 5; dias 0–19 (Wayeb 0–4). "
           "Ciclo 0 começa 348 dias antes da origem da Contagem Longa, na "
           "correlação GMT 584283. Data completa mostra também Tzolkin e "
           "Contagem Longa; correlações alternativas não são presumidas.";
  if (c.engine == "custom")
    return "Sistema próprio: meses e ciclo autorais; conversão terrestre exige "
           "alinhamento explícito.";
  return "Proléptico, ano zero astronômico. Não presume reformas históricas ou "
         "adoções regionais.";
}
} // namespace inde::project
