#include "inde/project/writing.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <glib.h>
#include <stdexcept>
namespace inde::project {
bool paragraph_style(DocumentTextStyle s) {
  return s == DocumentTextStyle::Heading ||
         s == DocumentTextStyle::Subheading || s == DocumentTextStyle::Quote ||
         (s >= DocumentTextStyle::Heading2 &&
          s <= DocumentTextStyle::AlignJustify);
}
void validate_format_span(const DocumentFormatSpan &s) {
  (void)to_string(s.style);
  bool valid = s.value.empty();
  if (s.style == DocumentTextStyle::FontFamily)
    valid = !s.value.empty() && s.value.size() <= 128 &&
            g_utf8_validate(s.value.data(), s.value.size(), nullptr) &&
            std::none_of(s.value.begin(), s.value.end(),
                         [](unsigned char c) { return c < 32 || c == 127; });
  else if (s.style == DocumentTextStyle::FontSize) {
    int size{};
    auto parsed =
        std::from_chars(s.value.data(), s.value.data() + s.value.size(), size);
    valid = parsed.ec == std::errc{} &&
            parsed.ptr == s.value.data() + s.value.size() && size >= 6 &&
            size <= 96 && s.value == std::to_string(size);
  } else if (s.style == DocumentTextStyle::Foreground)
    valid = s.value.size() == 7 && s.value[0] == '#' &&
            std::all_of(s.value.begin() + 1, s.value.end(),
                        [](unsigned char c) { return std::isxdigit(c); });
  if (!valid)
    throw std::runtime_error("Valor de formatação inválido: " +
                             to_string(s.style));
}
void validate_inline_commands(
    const std::vector<DocumentInlineCommand> &commands) {
  if (commands.size() > 128)
    throw std::runtime_error("Limite de 128 comandos inline por Documento");
  for (const auto &c : commands) {
    auto token = [](const std::string &v) {
      return g_utf8_validate(v.data(), v.size(), nullptr) &&
             std::none_of(v.begin(), v.end(), [](unsigned char ch) {
               return ch <= 32 || ch == 127;
             });
    };
    if (c.prefix.empty() || c.prefix.size() > 16 || !token(c.prefix) ||
        c.keyword.size() > 64 || !token(c.keyword) || c.label.empty() ||
        c.label.size() > 256 || c.content.empty() || c.content.size() > 65536 ||
        !g_utf8_validate(c.label.data(), c.label.size(), nullptr) ||
        !g_utf8_validate(c.content.data(), c.content.size(), nullptr))
      throw std::runtime_error(
          "Comando inline inválido: prefixo, palavra-chave, nome ou conteúdo");
  }
}
} // namespace inde::project
