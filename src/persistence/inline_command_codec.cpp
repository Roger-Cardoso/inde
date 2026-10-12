#include "inde/persistence/inline_command_codec.hpp"
#include "inde/persistence/json.hpp"
#include <stdexcept>
namespace inde::persistence {
std::string encode_inline_commands(
    const std::vector<project::DocumentInlineCommand> &items) {
  project::validate_inline_commands(items);
  json::Value::Array a;
  for (const auto &c : items)
    a.emplace_back(json::Value::Object{{"prefix", c.prefix},
                                       {"keyword", c.keyword},
                                       {"label", c.label},
                                       {"content", c.content}});
  return json::serialize(a, false);
}
std::vector<project::DocumentInlineCommand>
decode_inline_commands(const std::string &text) {
  if (text.size() > 9 * 1024 * 1024)
    throw std::runtime_error("Comandos inline excedem o limite");
  int depth{};bool quoted{},escaped{};
  for(char ch:text) {
    if(quoted){if(escaped)escaped=false;else if(ch=='\\')escaped=true;else if(ch=='"')quoted=false;}
    else if(ch=='"')quoted=true;
    else if(ch=='{'||ch=='['){if(++depth>2)throw std::runtime_error("Comandos inline têm estrutura profunda demais");}
    else if(ch=='}'||ch==']')--depth;
  }
  std::vector<project::DocumentInlineCommand> out;
  const auto root = json::parse(text);
  for (const auto &v : root.as_array()) {
    if (v.as_object().size() != 4)
      throw std::runtime_error("Campos incompatíveis em comando inline");
    out.push_back({v.at("prefix").as_string(), v.at("keyword").as_string(),
                   v.at("label").as_string(), v.at("content").as_string()});
  }
  project::validate_inline_commands(out);
  return out;
}
} // namespace inde::persistence
