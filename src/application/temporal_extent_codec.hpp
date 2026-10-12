#pragma once
#include "inde/persistence/json.hpp"
#include "inde/project/worldbuilding.hpp"
#include <stdexcept>
namespace inde::application::temporal_extent_codec {
using V = persistence::json::Value;
inline std::string encode(const project::TemporalReference &r) {
  V upper(nullptr);
  if (r.upper) {
    V::Array clock;
    for (auto n : r.upper->clock)
      clock.emplace_back(n);
    upper =
        V::Object{{"year", r.upper->year},
                  {"month", r.upper->month ? V(*r.upper->month) : V(nullptr)},
                  {"day", r.upper->day ? V(*r.upper->day) : V(nullptr)},
                  {"clock", std::move(clock)}};
  }
  return persistence::json::serialize(V::Object{{"version", 1},
                                                {"qualifier", r.qualifier},
                                                {"upper", std::move(upper)},
                                                {"uncertainty", r.uncertainty}},
                                      false);
}
inline void decode(const std::string &payload, project::TemporalReference &r) {
  if (payload.size() > 8192)
    throw std::runtime_error("Extensão temporal excessiva");
  auto v = persistence::json::parse(payload);
  if (v.as_object().size() != 4 || v.at("version").as_integer() != 1)
    throw std::runtime_error("Extensão temporal incompatível");
  r.qualifier = v.at("qualifier").as_string();
  r.uncertainty = v.at("uncertainty").as_string();
  const auto &u = v.at("upper");
  r.upper.reset();
  if (!u.is_null()) {
    if (u.as_object().size() != 4 || u.at("clock").as_array().size() > 8)
      throw std::runtime_error("Limite temporal inválido");
    project::TemporalBoundary b;
    b.year = u.at("year").as_integer();
    auto number = [](const V &value) -> std::optional<int> {
      if (value.is_null())
        return {};
      auto n = value.as_integer();
      if (n < 0 || n > 9999)
        throw std::runtime_error("Parte temporal inválida");
      return static_cast<int>(n);
    };
    b.month = number(u.at("month"));
    b.day = number(u.at("day"));
    for (const auto &n : u.at("clock").as_array()) {
      const auto value = number(n);
      if (!value)
        throw std::runtime_error("Horário final não aceita parte nula");
      b.clock.push_back(*value);
    }
    r.upper = std::move(b);
  }
}
} // namespace inde::application::temporal_extent_codec
