#include "inde/experimental/text_buffer.hpp"
#include <algorithm>
#include <glib.h>
#include <set>
#include <stdexcept>
namespace inde::experimental {
namespace {
void valid(const std::string &text) {
  if (text.size() > TextBuffer::limit || text.find('\0') != std::string::npos ||
      !g_utf8_validate(text.data(), static_cast<gssize>(text.size()), nullptr))
    throw std::runtime_error("Buffer experimental exige UTF-8 sem NUL e até 32 MiB");
}
constexpr std::size_t chunk_size = 4096;
}
const char *name(TextBackend backend) {
  switch (backend) {
  case TextBackend::Contiguous: return "contiguous";
  case TextBackend::ImmutableChunks: return "immutable_chunks_4096";
  case TextBackend::PieceTable: return "hybrid_piece_table";
  }
  throw std::runtime_error("Backend experimental desconhecido");
}
TextBuffer::TextBuffer(TextBackend backend, std::string text)
    : backend_(backend), bytes_(text.size()) {
  name(backend); valid(text); append_new(pieces_, text);
}
void TextBuffer::append_new(std::vector<Piece> &out, const std::string &text) const {
  if (text.empty()) return;
  if (backend_ != TextBackend::ImmutableChunks) {
    auto p = std::make_shared<const std::string>(text);
    out.push_back({p, 0, p->size()}); return;
  }
  for (std::size_t begin = 0; begin < text.size();) {
    auto end = std::min(begin + chunk_size, text.size());
    while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
    auto p = std::make_shared<const std::string>(text.substr(begin, end - begin));
    out.push_back({p, 0, p->size()}); begin = end;
  }
}
bool TextBuffer::boundary(std::size_t point) const {
  if (point > bytes_) return false;
  if (point == bytes_) return true;
  for (const auto &piece : pieces_) {
    if (point < piece.size)
      return (static_cast<unsigned char>((*piece.data)[piece.begin + point]) & 0xc0) != 0x80;
    point -= piece.size;
  }
  return false;
}
void TextBuffer::append_slice(std::vector<Piece> &out, const std::vector<Piece> &pieces,
                              std::size_t begin, std::size_t end) {
  std::size_t offset{};
  for (const auto &piece : pieces) {
    const auto left = std::max(begin, offset), right = std::min(end, offset + piece.size);
    if (left < right) out.push_back({piece.data, piece.begin + left - offset, right - left});
    offset += piece.size;
    if (offset >= end) break;
  }
}
void TextBuffer::compact_chunks(std::vector<Piece> &pieces) {
  std::vector<Piece> packed;
  for (const auto &p : pieces) {
    if (!packed.empty() && packed.back().size + p.size <= chunk_size) {
      auto &previous = packed.back();
      auto joined = previous.data->substr(previous.begin, previous.size);
      joined.append(*p.data, p.begin, p.size);
      auto data = std::make_shared<const std::string>(std::move(joined));
      previous = {data, 0, data->size()};
    } else packed.push_back(p);
  }
  pieces.swap(packed);
}
void TextBuffer::replace(std::size_t begin, std::size_t end, const std::string &inserted) {
  valid(inserted);
  if (begin > end || !boundary(begin) || !boundary(end) ||
      inserted.size() > limit - (bytes_ - (end - begin)))
    throw std::runtime_error("Edição fora do buffer, corta Unicode ou excede limite");
  const auto size = bytes_ - (end - begin) + inserted.size();
  std::vector<Piece> next;
  if (backend_ == TextBackend::Contiguous) {
    auto value = text(); value.replace(begin, end - begin, inserted); append_new(next, value);
  } else {
    append_slice(next, pieces_, 0, begin);
    append_new(next, inserted);
    append_slice(next, pieces_, end, bytes_);
    if (backend_ == TextBackend::ImmutableChunks) compact_chunks(next);
    if (next.size() > 100000) throw std::runtime_error("Limite de fragmentos; materialize antes de continuar");
  }
  pieces_.swap(next); bytes_ = size;
}
std::string TextBuffer::text() const {
  std::string result; result.reserve(bytes_);
  for (const auto &p : pieces_) result.append(*p.data, p.begin, p.size);
  return result;
}
std::vector<std::string> TextBuffer::fragments() const {
  std::vector<std::string> result;
  for (const auto &p : pieces_) result.push_back(p.data->substr(p.begin, p.size));
  return result;
}
std::size_t TextBuffer::retained_bytes() const { return retained_bytes({*this}); }
std::size_t TextBuffer::retained_bytes(const std::vector<TextBuffer> &snapshots) {
  std::set<const std::string *> seen;
  std::size_t total{};
  for (const auto &buffer : snapshots) {
    total += buffer.pieces_.capacity() * sizeof(Piece);
    for (const auto &p : buffer.pieces_)
      if (seen.insert(p.data.get()).second) total += sizeof(std::string) + p.data->capacity() + 1;
  }
  return total;
}
}
