#include "calendar_engines.hpp"
#include "inde/project/manifest.hpp"
#include <map>
#include <memory>
#include <stdexcept>
#include <unicode/ucal.h>
#include <unicode/uversion.h>
namespace inde::project::calendar_engines {
namespace {
struct Month {
  CalendarMonth value;
  std::int64_t start;
};
using Year = std::vector<Month>;
void check(UErrorCode s) {
  if (U_FAILURE(s))
    throw std::runtime_error(std::string("Regras ICU indisponíveis: ") +
                             u_errorName(s));
}
using Calendar = std::unique_ptr<UCalendar, decltype(&ucal_close)>;
Calendar open(const FictionalCalendar &c) {
  if (c.rule_revision != revision())
    throw std::runtime_error(
        "Versão das regras temporais incompatível; mantenha a biblioteca ICU "
        "original ou migre explicitamente");
  UErrorCode s = U_ZERO_ERROR;
  const UChar tz[] = {85, 84, 67, 0};
  Calendar cal(ucal_open(tz, 3, ("en@calendar=" + c.engine).c_str(),
                         UCAL_TRADITIONAL, &s),
               ucal_close);
  check(s);
  if (!cal || c.engine != ucal_getType(cal.get(), &s))
    throw std::runtime_error("Motor ICU incompatível");
  check(s);
  ucal_setAttribute(cal.get(), UCAL_LENIENT, 0);
  return cal;
}
void range(const FictionalCalendar &c, std::int64_t y) {
  if (c.engine == "chinese" ? y < 1901 || y > 2100 : y < 1 || y > 9999)
    throw std::runtime_error(
        c.engine == "chinese"
            ? "Modelo chinês: ano gregoriano associado entre 1901 e 2100"
            : "Modelo real: anos da própria era entre 1 e 9999");
}
Year compute(const FictionalCalendar &c, std::int64_t y) {
  range(c, y);
  auto cal = open(c);
  auto *p = cal.get();
  ucal_clear(p);
  ucal_set(p, UCAL_EXTENDED_YEAR,
           static_cast<int>(y) + (c.engine == "chinese" ? 2637 : 0));
  ucal_set(p, UCAL_MONTH, 0);
  ucal_set(p, UCAL_IS_LEAP_MONTH, 0);
  ucal_set(p, UCAL_DATE, 1);
  ucal_set(p, UCAL_HOUR_OF_DAY, 12);
  UErrorCode s = U_ZERO_ERROR;
  auto ey = ucal_get(p, UCAL_EXTENDED_YEAR, &s);
  check(s);
  const std::vector<std::string> hebrew = {
      "Tishri", "Heshvan", "Kislev", "Tevet", "Shevat", "Adar I", "Adar",
      "Nisan",  "Iyar",    "Sivan",  "Tamuz", "Av",     "Elul"};
  Year out;
  for (int i = 0; i < 14 && ucal_get(p, UCAL_EXTENDED_YEAR, &s) == ey; ++i) {
    auto m = ucal_get(p, UCAL_MONTH, &s);
    auto leap = ucal_get(p, UCAL_IS_LEAP_MONTH, &s);
    auto start = ucal_get(p, UCAL_JULIAN_DAY, &s);
    check(s);
    std::string name;
    if (c.engine == "chinese")
      name =
          "Mês lunar " + std::to_string(m + 1) + (leap ? " (intercalar)" : "");
    else if (c.engine == "hebrew")
      name = hebrew.at(m);
    else
      name = c.months.at(m).name;
    ucal_add(p, UCAL_MONTH, 1, &s);
    auto end = ucal_get(p, UCAL_JULIAN_DAY, &s);
    check(s);
    if (end <= start || end - start > 30)
      throw std::runtime_error("Intercalação ICU inconsistente");
    out.push_back({{name, static_cast<int>(end - start)}, start});
  }
  check(s);
  if (out.size() < 12 || out.size() > 13)
    throw std::runtime_error("Ano ICU inconsistente");
  if (c.engine == "hebrew" && out.size() == 13)
    for (auto &m : out)
      if (m.value.name == "Adar")
        m.value.name = "Adar II";
  return out;
}
Year months_data(const FictionalCalendar &c, std::int64_t y) {
  range(c, y); // Check provenance even on cache hits.
  if (c.rule_revision != revision())
    throw std::runtime_error("Regras ICU incompatíveis");
  thread_local std::map<std::pair<std::string, std::int64_t>, Year> cache;
  auto key = std::pair{c.engine, y};
  if (!cache.contains(key)) {
    if (cache.size() >= 32)
      cache.clear();
    cache.emplace(key, compute(c, y));
  }
  return cache.at(key);
}
std::int64_t mod(std::int64_t a, std::int64_t b) {
  auto r = a % b;
  return r < 0 ? r + b : r;
}
} // namespace
bool icu(const FictionalCalendar &c) {
  return c.engine == "chinese" || c.engine == "hebrew" ||
         c.engine == "islamic-civil" || c.engine == "ethiopic";
}
std::string revision() {
  UVersionInfo v;
  u_getVersion(v);
  char s[U_MAX_VERSION_STRING_LENGTH];
  u_versionToString(v, s);
  return std::string("icu-") + s + "/inde-day-v1";
}
std::vector<CalendarMonth> months(const FictionalCalendar &c, std::int64_t y) {
  std::vector<CalendarMonth> out;
  for (auto &m : months_data(c, y))
    out.push_back(m.value);
  return out;
}
std::int64_t day(const FictionalCalendar &c, CalendarDate d) {
  auto v = months_data(c, d.year);
  if (d.month < 1 || d.month > static_cast<int>(v.size()) || d.day < 1 ||
      d.day > v[d.month - 1].value.days)
    throw std::runtime_error("Data inexistente neste modelo e ano");
  return v[d.month - 1].start + d.day - 1;
}
CalendarDate date(const FictionalCalendar &c, std::int64_t jd) {
  if (jd < -4000000 || jd > 6000000)
    throw std::runtime_error("Dia fora do intervalo dos modelos ICU");
  auto cal = open(c);
  UErrorCode s = U_ZERO_ERROR;
  ucal_clear(cal.get());
  ucal_set(cal.get(), UCAL_JULIAN_DAY, static_cast<int>(jd));
  ucal_set(cal.get(), UCAL_HOUR_OF_DAY, 12);
  auto y =
      static_cast<std::int64_t>(ucal_get(cal.get(), UCAL_EXTENDED_YEAR, &s)) -
      (c.engine == "chinese" ? 2637 : 0);
  check(s);
  auto v = months_data(c, y);
  for (std::size_t i = 0; i < v.size(); ++i)
    if (jd >= v[i].start && jd < v[i].start + v[i].value.days)
      return {y, static_cast<int>(i + 1),
              static_cast<int>(jd - v[i].start + 1)};
  throw std::runtime_error("Data ICU inconsistente");
}
FictionalCalendar model(const std::string &engine) {
  FictionalCalendar c;
  c.id = new_uuid();
  c.engine = engine;
  if (icu(c))
    c.rule_revision = revision();
  if (engine == "chinese") {
    c.name = "Chinês lunissolar — ICU";
    for (int i = 1; i <= 12; ++i)
      c.months.push_back({"Mês lunar " + std::to_string(i), 30});
  } else if (engine == "hebrew") {
    c.name = "Judaico — hebraico calculado";
    for (auto n : {"Tishri", "Heshvan", "Kislev", "Tevet", "Shevat", "Adar I",
                   "Adar", "Nisan", "Iyar", "Sivan", "Tamuz", "Av", "Elul"})
      c.months.push_back({n, 30});
  } else if (engine == "islamic-civil") {
    c.name = "Islâmico civil tabular";
    int i = 0;
    for (auto n : {"Muharram", "Safar", "Rabi I", "Rabi II", "Jumada I",
                   "Jumada II", "Rajab", "Shaban", "Ramadan", "Shawwal",
                   "Dhu al-Qadah", "Dhu al-Hijjah"})
      c.months.push_back({n, (i++ % 2) ? 29 : 30});
  } else if (engine == "ethiopic") {
    c.name = "Etíope — Amete Mihret";
    for (auto n : {"Meskerem", "Tikimt", "Hidar", "Tahsas", "Tir", "Yekatit",
                   "Megabit", "Miyazya", "Ginbot", "Sene", "Hamle", "Nehase"})
      c.months.push_back({n, 30});
    c.months.push_back({"Pagumen", 5});
  } else if (engine == "egyptian-civil") {
    c.name = "Egípcio antigo — civil de 365 dias";
    for (auto season : {"Akhet", "Peret", "Shemu"})
      for (int i = 1; i <= 4; ++i)
        c.months.push_back({std::to_string(i) + " · " + season, 30});
    c.months.push_back({"Dias epagomenais (fora dos meses)", 5});
  } else if (engine == "maya-haab") {
    c.name = "Maia — Haab (correlação GMT 584283)";
    for (auto n : {"Pop", "Wo", "Sip", "Sotz", "Sek", "Xul", "Yaxkin", "Mol",
                   "Chen", "Yax", "Sak", "Keh", "Mak", "Kankin", "Muwan", "Pax",
                   "Kayab", "Kumku"})
      c.months.push_back({n, 20});
    c.months.push_back({"Wayeb", 5});
  } else
    throw std::runtime_error("Modelo real indisponível");
  if (icu(c))
    c.weekdays = {"Segunda-feira", "Terça-feira", "Quarta-feira",
                  "Quinta-feira",  "Sexta-feira", "Sábado",
                  "Domingo"};
  return c;
}
void validate_rules(const FictionalCalendar &c) {
  auto base = model(c.engine);
  if (c.months != base.months || c.leap_cycle || c.leap_phase ||
      c.leap_month != 1 || c.rule_revision != base.rule_revision ||
      c.weekdays.size() != base.weekdays.size())
    throw std::runtime_error(
        "Modelo real tem regras fixas; para alterar meses ou tamanho da "
        "semana, derive um calendário próprio");
}
std::string maya_cycles(CalendarDate d) {
  auto c = model("maya-haab");
  auto n = calendar_ordinal(c, d) - 348;
  if (n < 0)
    return " · antes da origem da Contagem Longa GMT";
  const std::vector<std::string> names = {
      "Imix",  "Ik’",   "Akbal", "Kan",    "Chikchan", "Kimi", "Manik",
      "Lamat", "Muluk", "Ok",    "Chuwen", "Eb",       "Ben",  "Ix",
      "Men",   "Kib",   "Kaban", "Etznab", "Kawak",    "Ajaw"};
  auto r = n;
  std::string s = " · Contagem Longa ";
  for (auto unit : {144000, 7200, 360, 20, 1}) {
    s += std::to_string(r / unit) + (unit == 1 ? "" : ".");
    r %= unit;
  }
  return s + " · Tzolkin " + std::to_string(mod(n + 3, 13) + 1) + " " +
         names[mod(n + 19, 20)];
}
} // namespace inde::project::calendar_engines
