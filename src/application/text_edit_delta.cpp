#include "inde/application/document_edit_session.hpp"
#include <glib.h>
#include <stdexcept>
namespace inde::application {
EditCommand text_delta(const std::string &a, const std::string &b) {
  if (!g_utf8_validate(a.data(), static_cast<gssize>(a.size()), nullptr) ||
      !g_utf8_validate(b.data(), static_cast<gssize>(b.size()), nullptr))
    throw std::runtime_error("Delta exige textos UTF-8 válidos");
  std::size_t prefix = 0, suffix = 0;
  while (prefix < a.size() && prefix < b.size() && a[prefix] == b[prefix]) ++prefix;
  while (prefix > 0 && prefix < a.size() && (static_cast<unsigned char>(a[prefix]) & 0xc0U) == 0x80U) --prefix;
  while (suffix < a.size()-prefix && suffix < b.size()-prefix && a[a.size()-suffix-1] == b[b.size()-suffix-1]) ++suffix;
  while (suffix > 0 && ((static_cast<unsigned char>(a[a.size()-suffix]) & 0xc0U) == 0x80U ||
                       (static_cast<unsigned char>(b[b.size()-suffix]) & 0xc0U) == 0x80U)) --suffix;
  return {prefix, a.substr(prefix, a.size()-prefix-suffix), b.substr(prefix, b.size()-prefix-suffix)};
}
}
