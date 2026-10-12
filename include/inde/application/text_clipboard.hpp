#pragma once
#include "inde/application/document_edit_session.hpp"
namespace inde::application {
inline constexpr const char *clipboard_mime = "application/x-inde-text-fragment-v1+json";
inline constexpr std::size_t clipboard_limit = 16 * 1024 * 1024;
struct TextFragment {
  std::string text;
  std::vector<project::DocumentFormatSpan> formatting;
};
// Never serializes identities, anchors, references, bindings or project data.
[[nodiscard]] TextFragment copy_fragment(const project::Document &, std::size_t start, std::size_t end, bool styles);
[[nodiscard]] std::string encode_fragment(const TextFragment &);
[[nodiscard]] TextFragment decode_fragment(const std::string &);
[[nodiscard]] DocumentEditState paste_fragment(const DocumentEditState &,
    std::size_t start, std::size_t end, const TextFragment &, bool styles);
}
