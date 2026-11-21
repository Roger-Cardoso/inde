#include "object_formula.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <functional>
#include <glib.h>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace inde::objects::formula {
namespace {
constexpr double magnitude = 1e12;
struct Error : std::runtime_error {
  std::size_t offset;
  Error(const char *code, std::size_t at = 0)
      : std::runtime_error(code), offset(at) {}
};
[[noreturn]] void fail(const char *code, std::size_t at = 0) {
  throw Error(code, at);
}
double checked(double v, std::size_t at = 0) {
  if (!std::isfinite(v) || std::abs(v) > magnitude)
    fail("formula_number_limit", at);
  return v == 0 ? 0 : v;
}
bool alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool digit(char c) { return c >= '0' && c <= '9'; }
struct Token {
  std::string text;
  std::size_t at;
  bool numeric = false;
};
struct Expr {
  std::string op, qualifier, symbol;
  double number = 0;
  std::size_t at = 0;
  std::vector<std::shared_ptr<Expr>> children;
};
using E = std::shared_ptr<Expr>;
struct Assignment {
  std::string qualifier, symbol;
  E expression;
  std::size_t at;
};
struct Program {
  std::map<std::string, E> variables;
  E result;
  std::vector<Assignment> actions;
  std::set<std::string> qualifiers;
};
class Parser {
  std::vector<Token> tokens;
  std::size_t cursor = 0, nodes = 0;
  Program program;
  const Token &peek() const { return tokens.at(cursor); }
  bool take(std::string_view s) {
    if (peek().text != s)
      return false;
    ++cursor;
    return true;
  }
  void need(std::string_view s) {
    if (!take(s))
      fail("formula_syntax", peek().at);
  }
  std::string name() {
    auto t = peek();
    if (!identifier(t.text))
      fail("formula_identifier", t.at);
    ++cursor;
    return t.text;
  }
  E node(std::string op, std::size_t at) {
    if (++nodes > 512)
      fail("formula_expression_limit", at);
    auto e = std::make_shared<Expr>();
    e->op = std::move(op);
    e->at = at;
    return e;
  }
  static int precedence(const std::string &op) {
    if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" ||
        op == ">=")
      return 1;
    if (op == "+" || op == "-")
      return 2;
    if (op == "*" || op == "/")
      return 3;
    if (op == "^")
      return 4;
    return 0;
  }
  E expression(int minimum = 1, int depth = 0) {
    if (depth > 48)
      fail("formula_depth", peek().at);
    auto t = peek();
    ++cursor;
    E left;
    if (t.text == "+" || t.text == "-") {
      left = node(t.text == "+" ? "positive" : "negative", t.at);
      left->children.push_back(expression(4, depth + 1));
    } else if (t.text == "(") {
      left = expression(1, depth + 1);
      need(")");
    } else if (t.numeric) {
      left = node("number", t.at);
      const auto r = std::from_chars(
          t.text.data(), t.text.data() + t.text.size(), left->number);
      if (r.ec != std::errc{} || r.ptr != t.text.data() + t.text.size())
        fail("formula_number", t.at);
      checked(left->number, t.at);
    } else if (identifier(t.text)) {
      if (take("(")) {
        left = node("call", t.at);
        left->symbol = t.text;
        if (!take(")")) {
          do {
            left->children.push_back(expression(1, depth + 1));
          } while (take(";") || take(","));
          need(")");
        }
      } else {
        left = node("reference", t.at);
        left->symbol = t.text;
        if (take(".")) {
          left->qualifier = t.text;
          left->symbol = take("resultado") ? "resultado" : name();
          program.qualifiers.insert(t.text);
        }
      }
    } else
      fail("formula_expression", t.at);
    while (precedence(peek().text) >= minimum) {
      t = peek();
      ++cursor;
      auto binary = node(t.text, t.at);
      binary->children = {
          left,
          expression(precedence(t.text) + (t.text == "^" ? 0 : 1), depth + 1)};
      left = std::move(binary);
    }
    return left;
  }

public:
  explicit Parser(const std::string &source) {
    if (source.size() > 16384 ||
        !g_utf8_validate(source.data(), static_cast<gssize>(source.size()),
                         nullptr))
      fail("formula_source_limit");
    for (std::size_t i = 0; i < source.size();) {
      const auto start = i;
      const auto c = source[i];
      if (c == ' ' || c == '\t' || c == '\r') {
        ++i;
        continue;
      }
      if (c == '#') {
        while (i < source.size() && source[i] != '\n')
          ++i;
        continue;
      }
      if (alpha(c)) {
        while (i < source.size() && (alpha(source[i]) || digit(source[i])))
          ++i;
        tokens.push_back({source.substr(start, i - start), start});
      } else if (digit(c) ||
                 (c == '.' && i + 1 < source.size() && digit(source[i + 1]))) {
        while (i < source.size() && (digit(source[i]) || source[i] == '.'))
          ++i;
        if (i < source.size() && (source[i] == 'e' || source[i] == 'E')) {
          ++i;
          if (i < source.size() && (source[i] == '+' || source[i] == '-'))
            ++i;
          while (i < source.size() && digit(source[i]))
            ++i;
        }
        tokens.push_back({source.substr(start, i - start), start, true});
      } else if (source.compare(i, 2, "×") == 0 ||
                 source.compare(i, 2, "÷") == 0) {
        tokens.push_back({source.compare(i, 2, "×") == 0 ? "*" : "/", i});
        i += 2;
      } else {
        if (std::string_view("\n{}();,+-*/^.=<>!").find(c) ==
            std::string_view::npos)
          fail("formula_character", i);
        std::string op(1, c);
        ++i;
        if ((c == '=' || c == '<' || c == '>' || c == '!') &&
            i < source.size() && source[i] == '=') {
          op += '=';
          ++i;
        }
        tokens.push_back({std::move(op), start});
      }
      if (tokens.size() > 2048)
        fail("formula_token_limit", start);
    }
    tokens.push_back({"", source.size()});
  }
  Program parse() {
    while (take("\n") || take(";")) {
    }
    const bool braces = take("{");
    while (true) {
      while (take("\n") || take(";")) {
      }
      if (peek().text.empty() || peek().text == "}")
        break;
      if (take("declara")) {
        const auto at = peek().at;
        auto variable = name();
        need("=");
        if (variable == "resultado" ||
            !program.variables.emplace(variable, expression()).second)
          fail("formula_duplicate_variable", at);
        if (program.variables.size() > 32)
          fail("formula_variable_limit", at);
      } else if (take("alterar")) {
        Assignment a;
        a.at = peek().at;
        a.qualifier = name();
        need(".");
        a.symbol = name();
        need("=");
        a.expression = expression();
        program.qualifiers.insert(a.qualifier);
        program.actions.push_back(std::move(a));
        if (program.actions.size() > 32)
          fail("formula_action_limit");
      } else {
        if (program.result)
          fail("formula_multiple_results", peek().at);
        if (take("resultado")) {
          (void)take("final");
          (void)take("calcula");
        } else
          (void)take("=");
        program.result = expression();
      }
      if (!peek().text.empty() && peek().text != "}" && peek().text != "\n" &&
          peek().text != ";")
        fail("formula_statement", peek().at);
    }
    if (braces)
      need("}");
    while (take("\n") || take(";")) {
    }
    if (!peek().text.empty())
      fail("formula_syntax", peek().at);
    if (!program.result)
      fail("formula_result_required");
    return std::move(program);
  }
};
std::string display(double value) {
  std::array<char, 64> b{};
  const auto r = std::to_chars(b.data(), b.data() + b.size(), value,
                               std::chars_format::general, 12);
  if (r.ec != std::errc{})
    fail("formula_number_limit");
  return {b.data(), r.ptr};
}
class Evaluation {
  const std::vector<Record> &records;
  std::map<std::string, const Record *> by_id;
  std::map<std::string, Program> programs;
  std::map<std::string, Error> errors;
  std::map<std::pair<std::string, std::string>, double> memo;
  std::set<std::pair<std::string, std::string>> visiting;
  std::size_t budget = 0;
  const Record &record(const std::string &id) const {
    const auto at = by_id.find(id);
    if (at == by_id.end())
      fail("formula_reference_missing");
    return *at->second;
  }
  double reference(const Record &r, const std::string &q,
                   const std::string &symbol, int depth) {
    const auto at = r.bindings.find(q);
    if (!q.empty() && at == r.bindings.end())
      fail("formula_reference_missing");
    return value(q.empty() ? r.id : at->second, symbol, depth + 1);
  }

public:
  explicit Evaluation(const std::vector<Record> &input) : records(input) {
    for (const auto &r : records) {
      by_id.emplace(r.id, &r);
      try {
        programs.emplace(r.id, Parser(r.source).parse());
      } catch (const Error &e) {
        errors.emplace(r.id, e);
      }
    }
  }
  double expression(const Record &r, const E &e, int depth = 0) {
    if (depth > 64 || ++budget > 200000)
      fail("formula_evaluation_limit", e->at);
    if (e->op == "number")
      return e->number;
    if (e->op == "reference")
      return reference(r, e->qualifier, e->symbol, depth);
    const auto child = [&](std::size_t i) {
      return expression(r, e->children.at(i), depth + 1);
    };
    if (e->op == "call") {
      const auto &name = e->symbol;
      const auto n = e->children.size();
      if (name == "SE") {
        if (n != 3)
          fail("formula_arguments", e->at);
        return child(0) != 0 ? child(1) : child(2);
      }
      if (name == "ABS") {
        if (n != 1)
          fail("formula_arguments", e->at);
        return std::abs(child(0));
      }
      if (name != "SOMA" && name != "MIN" && name != "MAX" && name != "MEDIA")
        fail("formula_function", e->at);
      if (n == 0 || n > 32)
        fail("formula_arguments", e->at);
      double out = child(0);
      for (std::size_t i = 1; i < n; ++i) {
        const auto v = child(i);
        out = name == "MIN"   ? std::min(out, v)
              : name == "MAX" ? std::max(out, v)
                              : checked(out + v, e->at);
      }
      return name == "MEDIA" ? checked(out / static_cast<double>(n), e->at)
                             : out;
    }
    const auto a = child(0);
    if (e->op == "positive")
      return a;
    if (e->op == "negative")
      return -a;
    const auto b = child(1);
    if (e->op == "+")
      return checked(a + b, e->at);
    if (e->op == "-")
      return checked(a - b, e->at);
    if (e->op == "*")
      return checked(a * b, e->at);
    if (e->op == "/") {
      if (b == 0)
        fail("formula_division_zero", e->at);
      return checked(a / b, e->at);
    }
    if (e->op == "^")
      return checked(std::pow(a, b), e->at);
    if (e->op == "==")
      return a == b;
    if (e->op == "!=")
      return a != b;
    if (e->op == "<")
      return a < b;
    if (e->op == ">")
      return a > b;
    if (e->op == "<=")
      return a <= b;
    if (e->op == ">=")
      return a >= b;
    fail("formula_operator", e->at);
  }
  double value(const std::string &id, const std::string &symbol,
               int depth = 0) {
    if (depth > 64)
      fail("formula_evaluation_limit");
    const auto key = std::pair{id, symbol};
    if (memo.contains(key))
      return memo.at(key);
    const auto &r = record(id);
    if (errors.contains(id))
      throw errors.at(id);
    const auto &p = programs.at(id);
    const E e = symbol == "resultado"          ? p.result
                : p.variables.contains(symbol) ? p.variables.at(symbol)
                                               : E{};
    if (!e)
      fail("formula_variable_missing");
    if (symbol != "resultado" && r.overrides.contains(symbol))
      return checked(r.overrides.at(symbol));
    if (!visiting.insert(key).second)
      fail("formula_cycle", e->at);
    try {
      const auto out = expression(r, e, depth + 1);
      memo.emplace(key, out);
      visiting.erase(key);
      return out;
    } catch (...) {
      visiting.erase(key);
      throw;
    }
  }
  std::map<std::string, Result> all() {
    std::map<std::string, Result> out;
    for (const auto &r : records) {
      Result result;
      try {
        result.value = value(r.id, "resultado");
        result.display = display(result.value);
        const auto &p = programs.at(r.id);
        result.actions = p.actions.size();
        for (const auto &[name, e] : p.variables) {
          const auto current = value(r.id, name);
          std::optional<double> initial;
          try {
            initial = expression(r, e);
          } catch (const Error &) {
          }
          result.variables.push_back({name,
                                      initial,
                                      current,
                                      r.overrides.contains(name),
                                      {},
                                      {},
                                      "number",
                                      "number"});
        }
        result.valid = true;
      } catch (const Error &e) {
        result.error = e.what();
        result.offset = e.offset;
      }
      out.emplace(r.id, std::move(result));
    }
    return out;
  }
  const Program &program(const std::string &id) {
    (void)record(id);
    if (errors.contains(id))
      throw errors.at(id);
    return programs.at(id);
  }
  const Record &get(const std::string &id) const { return record(id); }
};
} // namespace
bool identifier(const std::string &s) {
  return !s.empty() && s.size() <= 64 && alpha(s.front()) &&
         std::all_of(s.begin(), s.end(),
                     [](char c) { return alpha(c) || digit(c); }) &&
         s != "declara" && s != "alterar" && s != "resultado" && s != "final" &&
         s != "calcula";
}
void validate(const std::vector<Record> &records) {
  if (records.size() > 64)
    fail("formula_count_limit");
  std::set<std::string> ids, names;
  for (const auto &r : records) {
    if (r.id.empty() || r.id.size() > 256 ||
        !g_utf8_validate(r.id.data(), static_cast<gssize>(r.id.size()),
                         nullptr) ||
        !ids.insert(r.id).second || !identifier(r.name) ||
        !names.insert(r.name).second || r.revision < 0)
      fail("formula_identity");
    if (r.source.size() > 16384 ||
        !g_utf8_validate(r.source.data(), static_cast<gssize>(r.source.size()),
                         nullptr) ||
        r.bindings.size() > 64 || r.overrides.size() > 32)
      fail("formula_source_limit");
    for (const auto &[q, id] : r.bindings)
      if (!identifier(q) || id.empty() || id.size() > 256)
        fail("formula_binding");
    for (const auto &[name, value] : r.overrides) {
      if (!identifier(name))
        fail("formula_identifier");
      checked(value);
    }
  }
  // Renaming is not supported in this profile. A persisted binding must match
  // both the stable target identity and its immutable public name.
  for (const auto &r : records)
    for (const auto &[name, id] : r.bindings)
      if (std::none_of(records.begin(), records.end(), [&](const auto &target) {
            return target.id == id && target.name == name;
          }))
        fail("formula_binding_identity");
  for (const auto &r : records)
    if (r.advanced != records.front().advanced || (!r.advanced && r.consumed))
      fail("formula_language_mismatch");
  if (!records.empty() && records.front().advanced)
    rich::validate(records);
}
void bind(Record &r, const std::vector<Record> &records) {
  if (r.advanced) {
    rich::bind(r, records);
    return;
  }
  // Invalid drafts keep their previous bindings/overrides for later repair.
  Program p;
  try {
    p = Parser(r.source).parse();
  } catch (const Error &) {
    return;
  }
  std::map<std::string, std::string> next;
  for (const auto &q : p.qualifiers) {
    if (r.bindings.contains(q))
      next.emplace(q, r.bindings.at(q));
    else if (q == r.name)
      next.emplace(q, r.id);
    else {
      const auto target =
          std::find_if(records.begin(), records.end(),
                       [&](const auto &v) { return v.name == q; });
      if (target != records.end())
        next.emplace(q, target->id);
    }
  }
  r.bindings = std::move(next);
  std::erase_if(r.overrides,
                [&](const auto &v) { return !p.variables.contains(v.first); });
}
std::map<std::string, Result> evaluate(const std::vector<Record> &records) {
  validate(records);
  if (!records.empty() && records.front().advanced)
    return rich::evaluate(records);
  return Evaluation(records).all();
}
std::vector<Record> execute(const std::vector<Record> &records,
                            const std::string &id) {
  validate(records);
  if (!records.empty() && records.front().advanced)
    return rich::execute(records, id);
  Evaluation evaluation(records);
  const auto before = evaluation.all();
  if (!before.contains(id) || !before.at(id).valid)
    fail("formula_execution_invalid");
  const auto &p = evaluation.program(id);
  if (p.actions.empty())
    fail("formula_no_actions");
  auto candidate = records;
  std::set<std::pair<std::string, std::string>> written;
  for (const auto &a : p.actions) {
    const auto &caller = evaluation.get(id);
    const auto binding = caller.bindings.find(a.qualifier);
    if (binding == caller.bindings.end())
      fail("formula_reference_missing", a.at);
    const auto &target = evaluation.get(binding->second);
    if (!evaluation.program(target.id).variables.contains(a.symbol))
      fail("formula_variable_missing", a.at);
    if (!written.emplace(target.id, a.symbol).second)
      fail("formula_duplicate_assignment", a.at);
    const auto value = evaluation.expression(caller, a.expression);
    const auto at =
        std::find_if(candidate.begin(), candidate.end(),
                     [&](const auto &r) { return r.id == target.id; });
    at->overrides[a.symbol] = value;
  }
  for (auto &r : candidate)
    if (std::any_of(written.begin(), written.end(),
                    [&](const auto &v) { return v.first == r.id; })) {
      if (r.revision == std::numeric_limits<std::int64_t>::max())
        fail("revision_limit");
      ++r.revision;
    }
  const auto after = evaluate(candidate);
  for (const auto &[key, result] : after)
    if (before.at(key).valid && !result.valid)
      fail("formula_execution_would_break_dependency");
  return candidate;
}
} // namespace inde::objects::formula
