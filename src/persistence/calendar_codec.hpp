#pragma once
#include "inde/persistence/json.hpp"
#include "inde/project/worldbuilding.hpp"
#include <glib.h>
#include <stdexcept>
namespace inde::persistence::calendar_codec {
inline std::string hash(const std::string &v) {
  auto *p =
      g_compute_checksum_for_string(G_CHECKSUM_SHA256, v.data(), v.size());
  std::string h(p);
  g_free(p);
  return h;
}
inline std::string encode(const project::FictionalCalendar &c) {
  project::validate(c);
  json::Value::Array months, week, clock;
  for (const auto &m : c.months)
    months.emplace_back(
        json::Value::Object{{"name", m.name}, {"days", m.days}});
  for (const auto &w : c.weekdays)
    week.emplace_back(w);
  for (const auto &u : c.clock)
    clock.emplace_back(json::Value::Object{{"singular", u.singular},
                                           {"plural", u.plural},
                                           {"symbol", u.symbol},
                                           {"parts", u.parts}});
  auto object =
      json::Value::Object{{"version", c.rule_revision.empty() ? 2 : 3},
                          {"id", c.id},
                          {"name", c.name},
                          {"months", months},
                          {"weekdays", week},
                          {"cycle", c.leap_cycle},
                          {"phase", c.leap_phase},
                          {"leap_month", c.leap_month},
                          {"engine", c.engine},
                          {"clock", clock}};
  if (!c.rule_revision.empty())
    object.emplace("rule_revision", c.rule_revision);
  auto out = json::serialize(object, false);
  if (out.size() > 16384)
    throw std::runtime_error("Calendário excede 16 KiB");
  return out;
}
inline project::FictionalCalendar decode(const std::string &v,
                                         const std::string &sha) {
  if (v.size() > 16384 || v.find('\0') != std::string::npos ||
      !g_utf8_validate(v.data(), v.size(), nullptr) || hash(v) != sha)
    throw std::runtime_error("Calendário corrompido ou acima do limite");
  int depth{};
  bool quote{}, escape{};
  for (char ch : v) {
    if (quote) {
      if (escape)
        escape = false;
      else if (ch == '\\')
        escape = true;
      else if (ch == '"')
        quote = false;
    } else if (ch == '"')
      quote = true;
    else if (ch == '{' || ch == '[') {
      if (++depth > 4)
        throw std::runtime_error("Calendário excessivamente aninhado");
    } else if (ch == '}' || ch == ']')
      --depth;
  }
  auto o = json::parse(v).as_object();
  auto version = o.at("version").as_integer();
  if (!((version == 1 && o.size() == 8) || (version == 2 && o.size() == 10) ||
        (version == 3 && o.size() == 11)))
    throw std::runtime_error("Versão ou forma de calendário incompatível");
  project::FictionalCalendar c;
  c.id = o.at("id").as_string();
  c.name = o.at("name").as_string();
  if (version == 3)
    c.rule_revision = o.at("rule_revision").as_string();
  if (version >= 2) {
    c.engine = o.at("engine").as_string();
    const auto &units = o.at("clock").as_array();
    if (units.empty() || units.size() > 8)
      throw std::runtime_error("Relógio inválido");
    c.clock.clear();
    for (const auto &v : units) {
      const auto &u = v.as_object();
      auto n = u.at("parts").as_integer();
      if (u.size() != 4 || n < 1 || n > 10000)
        throw std::runtime_error("Subdivisão inválida");
      c.clock.push_back({u.at("singular").as_string(),
                         u.at("plural").as_string(), u.at("symbol").as_string(),
                         static_cast<int>(n)});
    }
  }
  auto integer = [&](const char *key, int max) {
    auto n = o.at(key).as_integer();
    if (n < 0 || n > max)
      throw std::runtime_error("Regra numérica de calendário inválida");
    return static_cast<int>(n);
  };
  c.leap_cycle = integer("cycle", 10000);
  c.leap_phase = integer("phase", 9999);
  c.leap_month = integer("leap_month", 32);
  const auto &months = o.at("months").as_array(),
             &week = o.at("weekdays").as_array();
  if (months.size() > 32 || week.size() > 16)
    throw std::runtime_error("Calendário excede limites estruturais");
  for (const auto &m : months) {
    const auto &a = m.as_object();
    auto n = a.at("days").as_integer();
    if (a.size() != 2 || n < 1 || n > 60)
      throw std::runtime_error("Mês inválido");
    c.months.push_back({a.at("name").as_string(), static_cast<int>(n)});
  }
  for (const auto &w : week)
    c.weekdays.push_back(w.as_string());
  project::validate(c);
  return c;
}

} // namespace inde::persistence::calendar_codec
