#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace inde::objects::formula {
// Names are immutable ASCII identifiers in this first profile. References bind
// names to persistent object IDs; neither layout nor cell coordinates bind
// data.
struct Record {
  std::string id, name, source;
  std::int64_t revision = 0;
  bool advanced = false, consumed = false;
  std::map<std::string, std::string> bindings;
  std::map<std::string, double> overrides;
};
struct Variable {
  std::string name;
  std::optional<double> initial;
  double current = 0;
  bool overridden = false;
  std::string initial_display, current_display;
  std::string initial_type = "number", current_type = "number";
};
struct Result {
  bool valid = false;
  bool command = false, consumed = false;
  std::string type = "number";
  double value = 0;
  std::string display, error;
  std::size_t offset = 0, actions = 0;
  std::vector<Variable> variables;
};
[[nodiscard]] bool identifier(const std::string &);
// Structural validation throws; source/evaluation diagnostics remain editable.
void validate(const std::vector<Record> &);
void bind(Record &, const std::vector<Record> &);
[[nodiscard]] std::map<std::string, Result>
evaluate(const std::vector<Record> &);
// Explicit, simultaneous assignments from one pre-state. All affected results
// must remain valid. Returns a candidate; the caller persists it atomically.
[[nodiscard]] std::vector<Record> execute(const std::vector<Record> &,
                                          const std::string &id);
namespace rich {
void validate(const std::vector<Record> &);
void bind(Record &, const std::vector<Record> &);
std::map<std::string, Result> evaluate(const std::vector<Record> &);
std::vector<Record> execute(const std::vector<Record> &, const std::string &);
void reset(Record &, const std::string &);
void materialize_legacy(Record &);
} // namespace rich
} // namespace inde::objects::formula
