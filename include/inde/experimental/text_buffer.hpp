#pragma once
#include <memory>
#include <string>
#include <vector>

// Experimental, not Document's authority or a persistent format.
namespace inde::experimental {
enum class TextBackend { Contiguous, ImmutableChunks, PieceTable };
const char *name(TextBackend);
class TextBuffer {
public:
  explicit TextBuffer(TextBackend backend, std::string text = {});
  // Byte coordinates. Invalid UTF-8, NUL and split-codepoint offsets rejected.
  void replace(std::size_t begin, std::size_t end, const std::string &text);
  [[nodiscard]] std::string text() const;
  [[nodiscard]] std::vector<std::string> fragments() const;
  [[nodiscard]] std::size_t bytes() const { return bytes_; }
  [[nodiscard]] std::size_t units() const { return pieces_.size(); }
  [[nodiscard]] TextBackend backend() const { return backend_; }
  [[nodiscard]] std::size_t retained_bytes() const;
  // Deduplicated backing allocations, excluding allocator/RSS overhead.
  static std::size_t retained_bytes(const std::vector<TextBuffer> &snapshots);
  static constexpr std::size_t limit = 32 * 1024 * 1024;
private:
  struct Piece {
    std::shared_ptr<const std::string> data;
    std::size_t begin{}, size{};
  };
  TextBackend backend_;
  std::vector<Piece> pieces_;
  std::size_t bytes_{};
  [[nodiscard]] bool boundary(std::size_t point) const;
  void append_new(std::vector<Piece> &, const std::string &) const;
  static void append_slice(std::vector<Piece> &, const std::vector<Piece> &,
                           std::size_t begin, std::size_t end);
  static void compact_chunks(std::vector<Piece> &);
};
}
