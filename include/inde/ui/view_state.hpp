#pragma once
#include "inde/persistence/json.hpp"
#include <glib.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
namespace inde::ui {
using ViewState = persistence::json::Value;
inline ViewState parse_view_state(const std::string &text) {
  if (text.size() > 65536) throw std::runtime_error("Contexto de interface excede o limite");
  if (!g_utf8_validate(text.data(),text.size(),nullptr))throw std::runtime_error("Contexto de interface não é UTF-8 válido");
  // Bound nesting before using the generic parser on a preferences file.
  int depth{}; bool quoted{}, escaped{};
  for (const auto c : text) {
    if (quoted) { if (escaped) escaped=false; else if (c=='\\') escaped=true; else if (c=='"') quoted=false; }
    else if (c=='"') quoted=true;
    else if (c=='{' || c=='[') { if (++depth>8) throw std::runtime_error("Contexto de interface inválido"); }
    else if (c=='}' || c==']') --depth;
  }
  return persistence::json::parse(text);
}
inline ViewState view_id(const std::optional<std::string> &id) { return id ? ViewState(*id) : ViewState(); }
inline std::optional<std::string> view_id(const ViewState &v) {
  if (v.is_null()) return {};
  const auto &id=v.as_string();
  if (id.empty() || id.size()>128) throw std::runtime_error("Identidade de contexto inválida");
  return id;
}
inline std::size_t view_offset(const ViewState &v) {
  const auto n=v.as_integer();
  if (n<0 || n>100000000) throw std::runtime_error("Posição de contexto inválida");
  return static_cast<std::size_t>(n);
}
inline double view_scroll(const ViewState &v) { return static_cast<double>(view_offset(v))/100.0; }
inline ViewState scroll_state(double value) {
  return static_cast<std::int64_t>(std::clamp(std::isfinite(value) ? value : 0.0,0.0,1000000.0)*100);
}
inline std::string serialize_view(const ViewState &v) { return persistence::json::serialize(v,false); }
}
