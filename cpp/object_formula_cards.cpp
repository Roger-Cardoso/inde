#include "object_formula_cards.hpp"
#include "object_composite_pilot.hpp"
#include "object_formula.hpp"
#include <charconv>
#include <cmath>
#include <glib.h>
#include <map>
#include <stdexcept>
#include <string_view>
namespace inde::objects::formula::cards {
namespace {
using V = persistence::json::Value;
[[noreturn]] void fail() { throw std::runtime_error("formula_cards_invalid"); }
void fields(const V &v, std::initializer_list<const char *> keys) {
  if (v.as_object().size() != keys.size())
    fail();
  for (const auto *key : keys)
    (void)v.at(key);
}
std::string text(const V &v, std::size_t cap = 4096) {
  const auto &s = v.as_string();
  if (s.size() > cap ||
      !g_utf8_validate(s.data(), static_cast<gssize>(s.size()), nullptr))
    fail();
  for (const char c : s)
    if (static_cast<unsigned char>(c) < 32 && c != '\n' && c != '\t')
      fail();
  return s;
}
std::string quote(const std::string &s) { return pilot::encode_json(V(s)); }
std::string numeric(const std::string &s) {
  double d{};
  const auto r = std::from_chars(s.data(), s.data() + s.size(), d);
  if (s.empty() || r.ec != std::errc{} || r.ptr != s.data() + s.size() ||
      !std::isfinite(d) || std::abs(d) > 1e12)
    fail();
  return s; // Keep the writer's numeric spelling after strict validation.
}
const std::map<std::string, std::string> comparisons{{"lt", "<"},  {"le", "<="},
                                                     {"eq", "=="}, {"ne", "!="},
                                                     {"ge", ">="}, {"gt", ">"}};
std::string reference(const V &v) {
  fields(v, {"kind", "formula", "variable"});
  const auto f = text(v.at("formula"), 64), s = text(v.at("variable"), 64);
  if (!identifier(f) || (s != "resultado" && !identifier(s)))
    fail();
  return f + "." + s;
}
class Reader {
public:
  std::string_view s;
  std::size_t pos = 0;
  explicit Reader(std::string_view input) : s(input) {}
  bool take(std::string_view t) {
    if (s.substr(pos, t.size()) != t)
      return false;
    pos += t.size();
    return true;
  }
  void need(std::string_view t) {
    if (!take(t))
      fail();
  }
  std::string string() {
    const auto start = pos;
    need("\"");
    bool escape = false;
    while (pos < s.size()) {
      const char c = s[pos++];
      if (c == '"' && !escape)
        return persistence::json::parse(
                   std::string(s.substr(start, pos - start)))
            .as_string();
      if (c == '\\' && !escape)
        escape = true;
      else
        escape = false;
    }
    fail();
  }
  std::string until(std::string_view end) {
    const auto at = s.find(end, pos);
    if (at == s.npos)
      fail();
    auto out = std::string(s.substr(pos, at - pos));
    pos = at;
    return out;
  }
};
} // namespace
std::string compile(const V &model) {
  const bool current = model.as_object().contains("current");
  if (current)
    fields(model, {"version", "type", "label", "entry", "rules", "otherwise",
                   "current"});
  else
    fields(model, {"version", "type", "label", "entry", "rules", "otherwise"});
  if (model.at("version").as_integer() != 1)
    fail();
  const auto type = text(model.at("type"), 16),
             label = text(model.at("label"), 256);
  if (type != "number" && type != "text")
    fail();
  const auto &entry = model.at("entry");
  std::string value;
  if (entry.at("kind").as_string() == "literal") {
    fields(entry, {"kind", "value"});
    const auto raw = text(entry.at("value"));
    value = type == "number" ? numeric(raw) : quote(raw);
  } else if (entry.at("kind").as_string() == "reference")
    value = reference(entry);
  else
    fail();
  const auto &rules = model.at("rules").as_array();
  if (rules.size() > 12)
    fail();
  std::string expression;
  for (const auto &rule : rules) {
    fields(rule, {"op", "value", "text"});
    const auto op = text(rule.at("op"), 16), raw = text(rule.at("value"));
    std::string condition;
    if (type == "text" && (op == "contains" || op == "starts"))
      condition = (op == "contains" ? "CONTEM" : "COMECA_COM") +
                  std::string("(Entrada; ") + quote(raw) + ")";
    else {
      if (!comparisons.contains(op) ||
          (type == "text" && op != "eq" && op != "ne"))
        fail();
      condition = "Entrada " + comparisons.at(op) + " " +
                  (type == "number" ? numeric(raw) : quote(raw));
    }
    expression +=
        "SE(" + condition + "; " + quote(text(rule.at("text"))) + "; ";
  }
  expression += quote(text(model.at("otherwise")));
  expression += std::string(rules.size(), ')');
  const auto assignment =
      current ? "\nEntrada = " + (type == "number"
                                      ? numeric(text(model.at("current")))
                                      : quote(text(model.at("current"))))
              : "";
  auto out =
      "# INDE-CARDS-1 " +
      pilot::encode_json(V(V::Object{{"label", label}, {"type", type}})) +
      "\ndeclara Entrada = " + value + assignment + "\nresultado " + expression;
  if (out.size() > 16384)
    throw std::runtime_error("formula_source_limit");
  return out;
}
std::optional<V> recognize(const std::string &source) {
  try {
    Reader r(source);
    r.need("# INDE-CARDS-1 ");
    const auto header = persistence::json::parse(r.until("\n"));
    fields(header, {"label", "type"});
    r.need("\ndeclara Entrada = ");
    const auto input = r.until("\n");
    r.need("\n");
    while (r.take("\n")) {
    }
    const auto type = header.at("type").as_string();
    V entry;
    std::optional<std::string> current;
    if (r.take("Entrada = ")) {
      current = type == "text" ? r.string() : r.until("\n");
      r.need("\n");
    }
    while (r.take("\n")) {
    }
    r.need("resultado ");
    if (input.starts_with('"')) {
      Reader q(input);
      const auto s = q.string();
      if (q.pos != q.s.size())
        fail();
      entry = V::Object{{"kind", "literal"}, {"value", s}};
    } else if (const auto dot = input.find('.');
               dot != input.npos && identifier(input.substr(0, dot)) &&
               (input.substr(dot + 1) == "resultado" ||
                identifier(input.substr(dot + 1))))
      entry = V::Object{{"kind", "reference"},
                        {"formula", input.substr(0, dot)},
                        {"variable", input.substr(dot + 1)}};
    else
      entry = V::Object{{"kind", "literal"}, {"value", numeric(input)}};
    V::Array rules;
    while (r.take("SE(")) {
      if (rules.size() >= 12)
        fail();
      std::string op, value;
      if (r.take("CONTEM(Entrada; ")) {
        op = "contains";
        value = r.string();
        r.need(")");
      } else if (r.take("COMECA_COM(Entrada; ")) {
        op = "starts";
        value = r.string();
        r.need(")");
      } else {
        r.need("Entrada ");
        const auto symbol = r.until(" ");
        r.need(" ");
        for (const auto &[key, s] : comparisons)
          if (symbol == s)
            op = key;
        if (op.empty())
          fail();
        value = type == "text" ? r.string() : r.until("; ");
      }
      r.need("; ");
      const auto shown = r.string();
      r.need("; ");
      rules.emplace_back(
          V::Object{{"op", op}, {"value", value}, {"text", shown}});
    }
    const auto fallback = r.string();
    r.need(std::string(rules.size(), ')'));
    if (r.pos != r.s.size())
      fail();
    V model(V::Object{{"version", 1},
                      {"type", type},
                      {"label", header.at("label")},
                      {"entry", entry},
                      {"rules", rules},
                      {"otherwise", fallback}});
    if (current) {
      auto object = model.as_object();
      object.emplace("current", *current);
      model = V(std::move(object));
    }
    auto normalized = source;
    for (auto at = normalized.find("\n\n"); at != std::string::npos;
         at = normalized.find("\n\n"))
      normalized.erase(at, 1);
    if (compile(model) != normalized)
      return {};
    return model;
  } catch (const std::exception &) {
    return {};
  }
}
} // namespace inde::objects::formula::cards
