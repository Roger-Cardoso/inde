#include "object_formula_hub.hpp"
#include "object_composite_pilot.hpp"
#include "object_formula.hpp"
#include "object_formula_cards.hpp"
#include <glib.h>
#include <map>
#include <stdexcept>
#include <vector>
namespace inde::objects::formula::hub {
namespace {
using V = persistence::json::Value;
[[noreturn]] void fail(const char *code = "formula_hub_invalid") {
  throw std::runtime_error(code);
}
void fields(const V &v, std::initializer_list<const char *> keys) {
  if (v.as_object().size() != keys.size())
    fail();
  for (const auto *k : keys)
    (void)v.at(k);
}
std::string text(const V &v, std::size_t cap = 4096) {
  return cards::checked_text(v, cap);
}
const std::map<std::string, std::string> operations{
    {"add", "+"}, {"subtract", "-"}, {"multiply", "*"}, {"divide", "/"}};
std::string assign(const std::string &name, const V &entry,
                   const std::string &type) {
  return "\ndeclara " + name + " = " + cards::emit_operand(entry, type);
}
std::string current_line(const V &m, const std::string &symbol,
                         const std::string &type) {
  if (!m.as_object().contains("current"))
    return "";
  const auto &c = m.at("current");
  if (!c.as_object().contains(symbol))
    return "";
  return "\n" + symbol + " = " +
         cards::emit_operand(
             V(V::Object{{"kind", "literal"}, {"value", c.at(symbol)}}), type);
}
std::string target(const V &v) {
  fields(v, {"formula", "variable"});
  const auto f = text(v.at("formula"), 64), s = text(v.at("variable"), 64);
  if (!identifier(f) || !identifier(s))
    fail();
  return f + "." + s;
}
std::vector<std::string> lines(const std::string &s) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start <= s.size()) {
    const auto end = s.find('\n', start);
    out.push_back(
        s.substr(start, end == s.npos ? s.size() - start : end - start));
    if (end == s.npos)
      break;
    start = end + 1;
  }
  return out;
}
std::string after(const std::string &s, const std::string &prefix) {
  if (!s.starts_with(prefix))
    fail();
  return s.substr(prefix.size());
}
} // namespace
std::string compile(const V &m) {
  const auto kind = m.at("kind").as_string();
  if (kind == "condition") {
    fields(m, {"kind", "cards"});
    return cards::compile(m.at("cards"));
  }
  if (kind == "free") {
    const bool titled = m.as_object().contains("title");
    if (titled)
      fields(m, {"kind", "source", "title"});
    else
      fields(m, {"kind", "source"});
    auto s = m.at("source").as_string();
    if (s.size() > 16384 ||
        !g_utf8_validate(s.data(), static_cast<gssize>(s.size()), nullptr))
      fail("formula_source_limit");
    if (titled)
      s = "# INDE-WRITTEN-1 " +
          pilot::encode_json(
              V(V::Object{{"title", text(m.at("title"), 256)}})) +
          "\n" + s;
    if (s.size() > 16384)
      fail("formula_source_limit");
    return s;
  }
  auto base = m.as_object();
  const bool current = base.erase("current") != 0;
  V::Object header{{"kind", kind}, {"title", text(m.at("title"), 256)}};
  std::string body;
  if (kind == "value") {
    fields(V(base), {"kind", "title", "type", "variable", "label", "entry"});
    const auto type = text(m.at("type"), 16),
               symbol = text(m.at("variable"), 64);
    if (!identifier(symbol))
      fail();
    header.emplace("type", type);
    header.emplace("variable", symbol);
    header.emplace("label", text(m.at("label"), 256));
    if (current) {
      for (const auto &[k, v] : m.at("current").as_object()) {
        (void)v;
        if (k != symbol)
          fail();
      }
    }
    body = assign(symbol, m.at("entry"), type) + current_line(m, symbol, type) +
           "\nresultado " + symbol;
  } else if (kind == "calculate") {
    fields(V(base), {"kind", "title", "left", "right", "operation"});
    const auto op = text(m.at("operation"), 16);
    if (!operations.contains(op))
      fail();
    if (current) {
      for (const auto &[k, v] : m.at("current").as_object()) {
        (void)v;
        if (k != "Primeiro" && k != "Segundo")
          fail();
      }
    }
    body = assign("Primeiro", m.at("left"), "number") +
           current_line(m, "Primeiro", "number") +
           assign("Segundo", m.at("right"), "number") +
           current_line(m, "Segundo", "number") + "\nresultado Primeiro " +
           operations.at(op) + " Segundo";
  } else if (kind == "change") {
    if (current)
      fail();
    fields(m, {"kind", "title", "type", "target", "operation", "operand"});
    const auto type = text(m.at("type"), 16), op = text(m.at("operation"), 16),
               t = target(m.at("target"));
    if (op != "set" && op != "add" && op != "subtract")
      fail();
    if (op != "set" && type != "number")
      fail();
    header.emplace("type", type);
    body = "\nalterar " + t + " = " +
           (op == "set" ? "" : t + " " + operations.at(op) + " ") +
           cards::emit_operand(m.at("operand"), type);
  } else
    fail();
  auto source = "# INDE-BLOCK-1 " + pilot::encode_json(V(header)) + body;
  if (source.size() > 16384)
    fail("formula_source_limit");
  return source;
}
std::optional<V> recognize(const std::string &source) {
  if (const auto c = cards::recognize(source))
    return V(V::Object{{"kind", "condition"}, {"cards", *c}});
  try {
    if (source.starts_with("# INDE-WRITTEN-1 ")) {
      const auto newline = source.find('\n');
      if (newline == source.npos)
        fail();
      const auto header = persistence::json::parse(
          after(source.substr(0, newline), "# INDE-WRITTEN-1 "));
      fields(header, {"title"});
      V model(V::Object{{"kind", "free"},
                        {"title", header.at("title")},
                        {"source", source.substr(newline + 1)}});
      if (compile(model) != source)
        fail();
      return model;
    }
    const auto normalized = cards::normalize_lines(source);
    const auto rows = lines(normalized);
    if (rows.empty())
      fail();
    const auto header =
        persistence::json::parse(after(rows[0], "# INDE-BLOCK-1 "));
    const auto kind = header.at("kind").as_string();
    V::Object m{{"kind", kind}, {"title", header.at("title")}}, current;
    std::size_t at = 1;
    const auto read_declaration = [&](const std::string &symbol,
                                      const std::string &type) -> V {
      if (at >= rows.size())
        fail();
      const auto value = cards::read_operand(
          after(rows[at++], "declara " + symbol + " = "), type);
      if (at < rows.size() && rows[at].starts_with(symbol + " = ")) {
        const auto c =
            cards::read_operand(after(rows[at++], symbol + " = "), type);
        if (c.at("kind").as_string() != "literal")
          fail();
        current.emplace(symbol, c.at("value"));
      }
      return value;
    };
    if (kind == "value") {
      fields(header, {"kind", "title", "type", "variable", "label"});
      const auto symbol = header.at("variable").as_string(),
                 type = header.at("type").as_string();
      m.emplace("type", type);
      m.emplace("variable", symbol);
      m.emplace("label", header.at("label"));
      m.emplace("entry", read_declaration(symbol, type));
      if (at >= rows.size() || rows[at++] != "resultado " + symbol)
        fail();
    } else if (kind == "calculate") {
      fields(header, {"kind", "title"});
      m.emplace("left", read_declaration("Primeiro", "number"));
      m.emplace("right", read_declaration("Segundo", "number"));
      if (at >= rows.size())
        fail();
      const auto expr = after(rows[at++], "resultado Primeiro ");
      bool found = false;
      for (const auto &[key, value] : operations)
        if (expr == value + " Segundo") {
          m.emplace("operation", key);
          found = true;
        }
      if (!found)
        fail();
    } else if (kind == "change") {
      fields(header, {"kind", "title", "type"});
      if (at >= rows.size())
        fail();
      const auto line = after(rows[at++], "alterar ");
      const auto eq = line.find(" = ");
      if (eq == line.npos)
        fail();
      const auto t = line.substr(0, eq);
      const auto dot = t.find('.');
      if (dot == t.npos)
        fail();
      m.emplace("target", V(V::Object{{"formula", t.substr(0, dot)},
                                      {"variable", t.substr(dot + 1)}}));
      auto value = line.substr(eq + 3);
      std::string op = "set";
      for (const auto &key : {"add", "subtract"}) {
        const auto prefix = t + " " + operations.at(key) + " ";
        if (value.starts_with(prefix)) {
          value = value.substr(prefix.size());
          op = key;
          break;
        }
      }
      m.emplace("operation", op);
      m.emplace("type", header.at("type"));
      m.emplace("operand",
                cards::read_operand(value, header.at("type").as_string()));
    } else
      fail();
    if (at != rows.size())
      fail();
    if (!current.empty())
      m.emplace("current", V(current));
    V model(m);
    if (compile(model) != normalized)
      fail();
    return model;
  } catch (const std::exception &) {
    return {};
  }
}
void preserve_symbols(const std::string &source, const V &model) {
  if (model.at("kind").as_string() == "free")
    return;
  const auto old = recognize(source);
  if (!old || old->at("kind").as_string() != model.at("kind").as_string())
    fail("formula_hub_advanced_source");
  const auto symbol = [](const V &m) -> std::string {
    if (m.at("kind").as_string() == "condition") {
      const auto &c = m.at("cards");
      return c.at("version").as_integer() == 1 ? "Entrada"
                                               : c.at("variable").as_string();
    }
    if (m.at("kind").as_string() == "value")
      return m.at("variable").as_string();
    return "";
  };
  if (symbol(*old) != symbol(model))
    fail("formula_symbol_immutable");
}
} // namespace inde::objects::formula::hub
