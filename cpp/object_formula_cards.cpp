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
std::string checked_text(const V &v, std::size_t cap) { return text(v, cap); }
std::string quote_text(const std::string &s) { return quote(s); }
std::string emit_operand(const V &entry, const std::string &type) {
  if (type != "number" && type != "text")
    fail();
  if (entry.at("kind").as_string() == "reference")
    return reference(entry);
  fields(entry, {"kind", "value"});
  if (entry.at("kind").as_string() != "literal")
    fail();
  const auto raw = text(entry.at("value"));
  return type == "number" ? numeric(raw) : quote(raw);
}
V read_operand(const std::string &input, const std::string &type) {
  V out;
  if (input.starts_with('"'))
    out = V::Object{{"kind", "literal"},
                    {"value", persistence::json::parse(input).as_string()}};
  else if (const auto dot = input.find('.');
           dot != input.npos && identifier(input.substr(0, dot)) &&
           (input.substr(dot + 1) == "resultado" ||
            identifier(input.substr(dot + 1))))
    out = V::Object{{"kind", "reference"},
                    {"formula", input.substr(0, dot)},
                    {"variable", input.substr(dot + 1)}};
  else
    out = V::Object{{"kind", "literal"}, {"value", input}};
  if (emit_operand(out, type) != input)
    fail();
  return out;
}
std::string normalize_lines(const std::string &source) {
  std::string out;
  out.reserve(source.size());
  for (const auto c : source)
    if (c != '\n' || out.empty() || out.back() != '\n')
      out += c;
  return out;
}
std::string compile(const V &model) {
  auto base = model.as_object();
  const bool current = base.erase("current") != 0;
  const bool named = model.at("version").as_integer() == 2;
  if (named)
    fields(V(base), {"version", "type", "label", "entry", "rules", "otherwise",
                     "title", "variable"});
  else
    fields(V(base),
           {"version", "type", "label", "entry", "rules", "otherwise"});
  const auto variable = named ? text(model.at("variable"), 64) : "Entrada";
  if (!identifier(variable))
    fail();
  const auto title = named ? text(model.at("title"), 256) : "";
  if (!named && model.at("version").as_integer() != 1)
    fail();
  const auto type = text(model.at("type"), 16),
             label = text(model.at("label"), 256);
  if (type != "number" && type != "text")
    fail();
  const auto value = emit_operand(model.at("entry"), type);
  const auto &rules = model.at("rules").as_array();
  if (rules.size() > 12)
    fail();
  std::string expression;
  for (const auto &rule : rules) {
    fields(rule, {"op", "value", "text"});
    const auto op = text(rule.at("op"), 16), raw = text(rule.at("value"));
    std::string condition;
    if (type == "text" && (op == "contains" || op == "starts"))
      condition = std::string(op == "contains" ? "CONTEM" : "COMECA_COM") +
                  "(" + variable + "; " + quote(raw) + ")";
    else {
      if (!comparisons.contains(op) ||
          (type == "text" && op != "eq" && op != "ne"))
        fail();
      condition = variable + " " + comparisons.at(op) + " " +
                  (type == "number" ? numeric(raw) : quote(raw));
    }
    expression +=
        "SE(" + condition + "; " + quote(text(rule.at("text"))) + "; ";
  }
  expression += quote(text(model.at("otherwise")));
  expression += std::string(rules.size(), ')');
  const auto assignment =
      current ? "\n" + variable + " = " +
                    (type == "number" ? numeric(text(model.at("current")))
                                      : quote(text(model.at("current"))))
              : "";
  V::Object header{{"label", label}, {"type", type}};
  if (named) {
    header.emplace("title", title);
    header.emplace("variable", variable);
  }
  auto out = std::string(named ? "# INDE-CARDS-2 " : "# INDE-CARDS-1 ") +
             pilot::encode_json(V(header)) + "\ndeclara " + variable + " = " +
             value + assignment + "\nresultado " + expression;
  if (out.size() > 16384)
    throw std::runtime_error("formula_source_limit");
  return out;
}
std::optional<V> recognize(const std::string &source) {
  try {
    Reader r(source);
    const bool named = r.take("# INDE-CARDS-2 ");
    if (!named)
      r.need("# INDE-CARDS-1 ");
    const auto header = persistence::json::parse(r.until("\n"));
    if (named)
      fields(header, {"label", "type", "title", "variable"});
    else
      fields(header, {"label", "type"});
    const auto variable = named ? header.at("variable").as_string() : "Entrada";
    if (!identifier(variable))
      fail();
    r.need("\ndeclara " + variable + " = ");
    const auto input = r.until("\n");
    r.need("\n");
    while (r.take("\n")) {
    }
    const auto type = header.at("type").as_string();
    V entry;
    std::optional<std::string> current;
    if (r.take(variable + " = ")) {
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
      if (r.take("CONTEM(" + variable + "; ")) {
        op = "contains";
        value = r.string();
        r.need(")");
      } else if (r.take("COMECA_COM(" + variable + "; ")) {
        op = "starts";
        value = r.string();
        r.need(")");
      } else {
        r.need(variable + " ");
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
    V model(V::Object{{"version", named ? 2 : 1},
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
    if (named) {
      auto object = model.as_object();
      object.emplace("title", header.at("title"));
      object.emplace("variable", variable);
      model = V(object);
    }
    if (compile(model) != normalize_lines(source))
      return {};
    return model;
  } catch (const std::exception &) {
    return {};
  }
}
} // namespace inde::objects::formula::cards
