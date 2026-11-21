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
#include <variant>

namespace inde::objects::formula::rich {
namespace {
using Value = std::variant<double, std::string, bool>;
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
  bool quoted = false;
  std::size_t end = 0;
};
struct Expr {
  std::string op, qualifier, symbol;
  double number = 0;
  std::string literal;
  std::size_t at = 0;
  std::vector<std::shared_ptr<Expr>> children;
};
using E = std::shared_ptr<Expr>;
struct Assignment {
  std::string qualifier, symbol;
  E expression;
  std::size_t at;
};
struct Current {
  E expression;
  std::size_t start, end;
};
struct Program {
  std::map<std::string, Current> current;
  std::map<std::string, std::size_t> declaration_ends;
  std::size_t insert_at = 0;
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
    if (peek().quoted || peek().text != s)
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
    if (t.quoted || !identifier(t.text) || t.text == "true" ||
        t.text == "false" || t.text == "verdadeiro" || t.text == "falso")
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
    if (op == "+" || op == "-" || op == "&")
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
    if (t.quoted) {
      left = node("string", t.at);
      left->literal = t.text;
    } else if (t.text == "+" || t.text == "-") {
      left = node(t.text == "+" ? "positive" : "negative", t.at);
      left->children.push_back(expression(4, depth + 1));
    } else if (t.text == "(") {
      left = expression(1, depth + 1);
      need(")");
    } else if (t.text == "verdadeiro" || t.text == "true" ||
               t.text == "falso" || t.text == "false") {
      left = node("boolean", t.at);
      left->number = (t.text == "verdadeiro" || t.text == "true") ? 1 : 0;
    } else if (t.numeric) {
      left = node("number", t.at);
      const auto r = std::from_chars(
          t.text.data(), t.text.data() + t.text.size(), left->number);
      if (r.ec != std::errc{} || r.ptr != t.text.data() + t.text.size())
        fail("formula_number", t.at);
      checked(left->number, t.at);
    } else if (identifier(t.text) || t.text == "NÃO" || t.text == "não" ||
               t.text == "Não") {
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
    while (!peek().quoted && precedence(peek().text) >= minimum) {
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
      if (c == '"') {
        ++i;
        std::string literal;
        bool closed = false;
        while (i < source.size()) {
          char ch = source[i++];
          if (ch == '"') {
            closed = true;
            break;
          }
          if (ch == '\n' || ch == '\r')
            fail("formula_string_syntax", i - 1);
          if (ch == '\\') {
            if (i >= source.size())
              fail("formula_string_syntax", i - 1);
            ch = source[i++];
            if (ch == 'n')
              ch = '\n';
            else if (ch == 't')
              ch = '\t';
            else if (ch != '"' && ch != '\\')
              fail("formula_string_syntax", i - 1);
          }
          literal += ch;
          if (literal.size() > 4096)
            fail("formula_text_limit", start);
        }
        if (!closed)
          fail("formula_string_syntax", start);
        tokens.push_back({std::move(literal), start, false, true, i});
      } else if ((source.compare(i, 4, "NÃO") == 0 ||
                  source.compare(i, 4, "não") == 0 ||
                  source.compare(i, 4, "Não") == 0)) {
        tokens.push_back({source.substr(i, 4), i});
        i += 4;
      } else if (alpha(c)) {
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
        if (std::string_view("\n{}();,+-*/^.=<>!&").find(c) ==
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
      if (!tokens.empty())
        tokens.back().end = i;
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
      if (!peek().quoted && (peek().text.empty() || peek().text == "}"))
        break;
      if (take("declara")) {
        const auto at = peek().at;
        auto variable = name();
        need("=");
        if (variable == "resultado" ||
            !program.variables.emplace(variable, expression()).second)
          fail("formula_duplicate_variable", at);
        program.declaration_ends[variable] = peek().at;
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
      } else if (!peek().quoted && identifier(peek().text) &&
                 tokens.at(cursor + 1).text == "=") {
        const auto start = peek().at;
        const auto symbol = name();
        need("=");
        auto e = expression();
        if (!program.current
                 .emplace(symbol, Current{std::move(e), start,
                                          tokens.at(cursor - 1).end})
                 .second)
          fail("formula_duplicate_current", start);
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
    program.insert_at = peek().at;
    if (braces)
      need("}");
    while (take("\n") || take(";")) {
    }
    if (peek().quoted || !peek().text.empty())
      fail("formula_syntax", peek().at);
    if (!program.actions.empty()) {
      if (program.actions.size() != 1 || !program.variables.empty() ||
          !program.current.empty() || program.result)
        fail("formula_command_must_be_alone");
    } else if (!program.result)
      fail("formula_result_required");
    for (const auto &[name, current] : program.current) {
      (void)current;
      if (!program.variables.contains(name))
        fail("formula_current_undeclared");
    }
    return std::move(program);
  }
};
std::string display_number(double value) {
  std::array<char, 64> b{};
  const auto r = std::to_chars(b.data(), b.data() + b.size(), value,
                               std::chars_format::general, 12);
  if (r.ec != std::errc{})
    fail("formula_number_limit");
  return {b.data(), r.ptr};
}
std::string display(const Value &value) {
  if (const auto *n = std::get_if<double>(&value))
    return display_number(*n);
  if (const auto *s = std::get_if<std::string>(&value))
    return *s;
  return std::get<bool>(value) ? "verdadeiro" : "falso";
}
std::string type(const Value &v) {
  return std::holds_alternative<double>(v)        ? "number"
         : std::holds_alternative<std::string>(v) ? "text"
                                                  : "boolean";
}
double numeric(const Value &v, std::size_t at = 0) {
  if (const auto *n = std::get_if<double>(&v))
    return *n;
  fail("formula_type", at);
}
bool truth(const Value &v, std::size_t at = 0) {
  if (const auto *b = std::get_if<bool>(&v))
    return *b;
  if (const auto *n = std::get_if<double>(&v))
    return *n != 0;
  fail("formula_type", at);
}
std::string upper(const std::string &s) {
  std::unique_ptr<gchar, decltype(&g_free)> p(
      g_utf8_strup(s.c_str(), static_cast<gssize>(s.size())), &g_free);
  return p.get();
}
std::string literal(const Value &v) {
  if (const auto *s = std::get_if<std::string>(&v)) {
    std::string out = "\"";
    for (char c : *s) {
      if (c == '"' || c == '\\')
        out += '\\';
      if (c == '\n')
        out += "\\n";
      else if (c == '\t')
        out += "\\t";
      else
        out += c;
    }
    return out + '"';
  }
  if (const auto *n = std::get_if<double>(&v)) {
    std::array<char, 64> b{};
    const auto r = std::to_chars(b.data(), b.data() + b.size(), *n,
                                 std::chars_format::general,
                                 std::numeric_limits<double>::max_digits10);
    if (r.ec != std::errc{})
      fail("formula_number_limit");
    return {b.data(), r.ptr};
  }
  return std::get<bool>(v) ? "verdadeiro" : "falso";
}
void set_current(Record &r, const std::string &symbol, const Value &v) {
  const auto p = Parser(r.source).parse();
  if (!p.variables.contains(symbol))
    fail("formula_variable_missing");
  const auto statement = symbol + " = " + literal(v);
  if (p.current.contains(symbol)) {
    const auto &current = p.current.at(symbol);
    r.source.replace(current.start, current.end - current.start, statement);
  } else {
    const auto at = p.declaration_ends.at(symbol);
    const auto suffix = at < r.source.size() && r.source[at] == '}' ? "\n" : "";
    r.source.insert(at, "\n" + statement + suffix);
  }
  if (r.source.size() > 16384)
    fail("formula_source_limit");
}
void bump(Record &r) {
  if (r.revision == std::numeric_limits<std::int64_t>::max())
    fail("revision_limit");
  ++r.revision;
}
class Evaluation {
  const std::vector<Record> &records;
  std::map<std::string, const Record *> by_id;
  std::map<std::string, Program> programs;
  std::map<std::string, Error> errors;
  std::map<std::pair<std::string, std::string>, Value> memo;
  std::set<std::pair<std::string, std::string>> visiting;
  std::size_t budget = 0;

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
  const Record &record(const std::string &id) const {
    const auto at = by_id.find(id);
    if (at == by_id.end())
      fail("formula_reference_missing");
    return *at->second;
  }
  const Program &program(const std::string &id) const {
    (void)record(id);
    if (errors.contains(id))
      throw errors.at(id);
    return programs.at(id);
  }
  std::string target(const Record &r, const std::string &q) const {
    if (q.empty())
      return r.id;
    const auto at = r.bindings.find(q);
    if (at == r.bindings.end())
      fail("formula_reference_missing");
    return at->second;
  }
  Value expression(const Record &r, const E &e, int depth = 0) {
    if (depth > 64 || ++budget > 200000)
      fail("formula_evaluation_limit", e->at);
    if (e->op == "number")
      return e->number;
    if (e->op == "string")
      return e->literal;
    if (e->op == "boolean")
      return e->number != 0;
    if (e->op == "reference")
      return value(target(r, e->qualifier), e->symbol, depth + 1);
    const auto child = [&](std::size_t i) {
      return expression(r, e->children.at(i), depth + 1);
    };
    if (e->op == "call") {
      const auto name = upper(e->symbol);
      const auto n = e->children.size();
      if (name == "SE" || name == "IF") {
        if (n != 3)
          fail("formula_arguments", e->at);
        return truth(child(0), e->at) ? child(1) : child(2);
      }
      if (name == "E" || name == "AND" || name == "OU" || name == "OR") {
        if (n == 0 || n > 32)
          fail("formula_arguments", e->at);
        const bool all = name == "E" || name == "AND";
        for (std::size_t i = 0; i < n; ++i) {
          const auto b = truth(child(i), e->at);
          if (all && !b)
            return false;
          if (!all && b)
            return true;
        }
        return all;
      }
      if (name == "NÃO" || name == "NAO" || name == "NOT") {
        if (n != 1)
          fail("formula_arguments", e->at);
        return !truth(child(0), e->at);
      }
      if (name == "TEXTO" || name == "TEXT") {
        if (n != 1)
          fail("formula_arguments", e->at);
        return display(child(0));
      }
      if (name == "ABS") {
        if (n != 1)
          fail("formula_arguments", e->at);
        return std::abs(numeric(child(0), e->at));
      }
      if (name != "SOMA" && name != "SUM" && name != "MEDIA" &&
          name != "AVERAGE" && name != "MIN" && name != "MAX")
        fail("formula_function", e->at);
      if (n == 0 || n > 32)
        fail("formula_arguments", e->at);
      double out = numeric(child(0), e->at);
      for (std::size_t i = 1; i < n; ++i) {
        const auto v = numeric(child(i), e->at);
        out = name == "MIN"   ? std::min(out, v)
              : name == "MAX" ? std::max(out, v)
                              : checked(out + v, e->at);
      }
      return name == "MEDIA" || name == "AVERAGE"
                 ? checked(out / static_cast<double>(n), e->at)
                 : out;
    }
    const auto a = child(0);
    if (e->op == "positive")
      return numeric(a, e->at);
    if (e->op == "negative")
      return -numeric(a, e->at);
    const auto b = child(1);
    if (e->op == "&") {
      const auto x = display(a), y = display(b);
      if (x.size() + y.size() > 4096)
        fail("formula_text_limit", e->at);
      return x + y;
    }
    if (e->op == "==" || e->op == "!=") {
      if (a.index() != b.index())
        fail("formula_type", e->at);
      return e->op == "==" ? a == b : a != b;
    }
    const auto x = numeric(a, e->at), y = numeric(b, e->at);
    if (e->op == "+")
      return checked(x + y, e->at);
    if (e->op == "-")
      return checked(x - y, e->at);
    if (e->op == "*")
      return checked(x * y, e->at);
    if (e->op == "/") {
      if (y == 0)
        fail("formula_division_zero", e->at);
      return checked(x / y, e->at);
    }
    if (e->op == "^")
      return checked(std::pow(x, y), e->at);
    if (e->op == "<")
      return x < y;
    if (e->op == ">")
      return x > y;
    if (e->op == "<=")
      return x <= y;
    if (e->op == ">=")
      return x >= y;
    fail("formula_operator", e->at);
  }
  Value value(const std::string &id, const std::string &symbol, int depth = 0) {
    if (depth > 64)
      fail("formula_evaluation_limit");
    const auto key = std::pair{id, symbol};
    if (memo.contains(key))
      return memo.at(key);
    const auto &r = record(id);
    const auto &p = program(id);
    if (!p.actions.empty())
      fail("formula_command_has_no_value");
    const E e = symbol == "resultado"          ? p.result
                : p.current.contains(symbol)   ? p.current.at(symbol).expression
                : p.variables.contains(symbol) ? p.variables.at(symbol)
                                               : E{};
    if (!e)
      fail("formula_variable_missing");
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
    std::size_t text_budget = 0;
    for (const auto &r : records) {
      Result result;
      result.consumed = r.consumed;
      try {
        const auto &p = program(r.id);
        result.command = !p.actions.empty();
        if (result.command) {
          result.type = "command";
          result.actions = r.consumed ? 0 : 1;
          if (!r.consumed) {
            const auto &a = p.actions.front();
            const auto id = target(r, a.qualifier);
            if (!program(id).variables.contains(a.symbol))
              fail("formula_variable_missing", a.at);
            (void)expression(r, a.expression);
          }
        } else {
          const auto v = value(r.id, "resultado");
          result.type = type(v);
          result.display = display(v);
          if (const auto *n = std::get_if<double>(&v))
            result.value = *n;
          for (const auto &[name, e] : p.variables) {
            const auto current = value(r.id, name);
            Variable var;
            var.name = name;
            var.current_display = display(current);
            var.current_type = type(current);
            var.initial_type.clear();
            var.overridden = p.current.contains(name);
            if (const auto *n = std::get_if<double>(&current))
              var.current = *n;
            try {
              const auto initial = expression(r, e);
              var.initial_display = display(initial);
              var.initial_type = type(initial);
              if (const auto *n = std::get_if<double>(&initial))
                var.initial = *n;
            } catch (const Error &) {
              var.initial_display = "indisponível";
            }
            text_budget +=
                var.current_display.size() + var.initial_display.size();
            result.variables.push_back(std::move(var));
          }
        }
        text_budget += result.display.size();
        if (text_budget > 512 * 1024)
          fail("formula_output_limit");
        result.valid = true;
      } catch (const Error &e) {
        result.error = e.what();
        result.offset = e.offset;
        result.display.clear();
        result.variables.clear();
      }
      out.emplace(r.id, std::move(result));
    }
    return out;
  }
};
} // namespace
void validate(const std::vector<Record> &records) {
  for (const auto &r : records) {
    if (r.name == "true" || r.name == "false" || r.name == "verdadeiro" ||
        r.name == "falso")
      fail("formula_identifier");
    if (!r.overrides.empty())
      fail("formula_current_must_be_in_source");
    if (r.consumed) {
      const auto p = Parser(r.source).parse();
      if (p.actions.size() != 1)
        fail("formula_consumed_not_command");
    }
  }
}
void bind(Record &r, const std::vector<Record> &records) {
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
    else {
      const auto target =
          std::find_if(records.begin(), records.end(),
                       [&](const auto &v) { return v.name == q; });
      if (target != records.end())
        next.emplace(q, target->id);
    }
  }
  r.bindings = std::move(next);
}
std::map<std::string, Result> evaluate(const std::vector<Record> &records) {
  return Evaluation(records).all();
}
std::vector<Record> execute(const std::vector<Record> &records,
                            const std::string &id) {
  Evaluation evaluation(records);
  const auto before = evaluation.all();
  const auto &caller = evaluation.record(id);
  if (caller.consumed)
    fail("formula_command_consumed");
  const auto &p = evaluation.program(id);
  if (p.actions.size() != 1)
    fail("formula_command_must_be_alone");
  const auto &a = p.actions.front();
  const auto target = evaluation.target(caller, a.qualifier);
  if (target == id)
    fail("formula_command_must_be_alone");
  const auto value = evaluation.expression(caller, a.expression);
  auto candidate = records;
  auto at = std::find_if(candidate.begin(), candidate.end(),
                         [&](const auto &r) { return r.id == target; });
  if (at == candidate.end())
    fail("formula_reference_missing");
  set_current(*at, a.symbol, value);
  bump(*at);
  auto command = std::find_if(candidate.begin(), candidate.end(),
                              [&](const auto &r) { return r.id == id; });
  command->consumed = true;
  bump(*command);
  const auto after = formula::evaluate(candidate);
  for (const auto &[key, result] : after)
    if (before.at(key).valid && !result.valid)
      fail("formula_execution_would_break_dependency");
  return candidate;
}
void reset(Record &r, const std::string &symbol) {
  const auto p = Parser(r.source).parse();
  const auto at = p.current.find(symbol);
  if (at == p.current.end())
    fail("formula_override_missing");
  r.source.erase(at->second.start, at->second.end - at->second.start);
  bump(r);
}
void materialize_legacy(Record &r) {
  const auto values = r.overrides;
  r.overrides.clear();
  r.advanced = true;
  for (const auto &[name, value] : values)
    set_current(r, name, value);
}
} // namespace inde::objects::formula::rich
