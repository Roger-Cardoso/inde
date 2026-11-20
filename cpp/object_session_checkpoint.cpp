#include "object_session_checkpoint.hpp"
#include "inde/persistence/composition_store.hpp"
#include <charconv>
#include <limits>
#include <stdexcept>
#include <unordered_map>
namespace inde::objects::planning::checkpoint_detail {
namespace {
[[noreturn]] void bad() {
  throw std::invalid_argument(
      "Checkpoint OBJ-01 de laboratório inválido ou excedido");
}
struct Writer {
  std::string bytes{"OBJ01-RECOVERY-LAB-1\n"};
  void field(const std::string &s) {
    if (s.size() > byte_limit || bytes.size() + s.size() + 24 > byte_limit) {
      bad();
    }
    bytes += std::to_string(s.size());
    bytes += ':';
    bytes += s;
  }
  template <class N> void number(N n) {
    char buffer[64];
    const auto r = std::to_chars(buffer, buffer + sizeof(buffer), n);
    if (r.ec != std::errc{}) {
      bad();
    }
    field(std::string(buffer, r.ptr));
  }
};
struct Reader {
  const std::string &bytes;
  std::size_t offset{};
  std::string field(std::size_t limit) {
    if (offset >= bytes.size()) {
      bad();
    }
    const auto end = bytes.find(':', offset);
    if (end == std::string::npos || end - offset > 8 || end == offset) {
      bad();
    }
    std::size_t n{};
    const auto r =
        std::from_chars(bytes.data() + offset, bytes.data() + end, n);
    if (r.ec != std::errc{} || r.ptr != bytes.data() + end || n > limit ||
        n > bytes.size() - end - 1) {
      bad();
    }
    offset = end + 1 + n;
    return bytes.substr(end + 1, n);
  }
  template <class N> N number() {
    const auto s = field(64);
    N n{};
    const auto r = std::from_chars(s.data(), s.data() + s.size(), n);
    if (r.ec != std::errc{} || r.ptr != s.data() + s.size()) {
      bad();
    }
    return n;
  }
};
void list(Writer &w, const std::vector<std::string> &v) {
  w.number(v.size());
  for (const auto &s : v) {
    if (s.size() > 512) {
      bad();
    }
    w.field(s);
  }
}
std::vector<std::string> list(Reader &r) {
  const auto n = r.number<std::size_t>();
  if (n > 10000) {
    bad();
  }
  std::vector<std::string> v;
  v.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    v.push_back(r.field(512));
  }
  return v;
}
} // namespace
void bounded_source(const std::string &s) {
  if (s.size() > 16 * 1024 * 1024) {
    bad();
  }
  std::size_t depth{}, tokens{};
  bool quoted{}, escape{};
  for (char ch : s) {
    if (quoted) {
      if (escape)
        escape = false;
      else if (ch == '\\')
        escape = true;
      else if (ch == '"')
        quoted = false;
      continue;
    }
    if (ch == '"') {
      quoted = true;
      continue;
    }
    if (ch == '{' || ch == '[') {
      if (++depth > 64) {
        bad();
      }
    }
    if (ch == '}' || ch == ']') {
      if (!depth) {
        bad();
      }
      --depth;
    }
    if (ch == '{' || ch == '[' || ch == ',' || ch == ':')
      if (++tokens > 1000000) {
        bad();
      }
  }
  if (quoted || depth) {
    bad();
  }
}
SessionCheckpoint encode(const Data &d) {
  bounded_source(d.base);
  bounded_source(d.working);
  if (d.object_order.size() > 10000 || d.frames.size() > 10000 ||
      d.layout.children.size() > 10000 || d.layout.paint_order.size() > 10000) {
    bad();
  }
  Writer w;
  w.field(d.composition ? "C" : "D");
  w.field(d.pending ? "1" : "0");
  w.field(d.project_id);
  w.field(d.base);
  w.field(d.working);
  w.number(d.persisted_revision);
  w.field(d.timestamp);
  w.number(d.selection.cursor);
  w.number(d.selection.bound);
  w.number(static_cast<int>(d.layout.kind));
  w.number(d.layout.columns);
  list(w, d.object_order);
  list(w, d.layout.children);
  list(w, d.layout.paint_order);
  w.number(d.frames.size());
  for (const auto &o : d.frames) {
    if (o.id.size() > 512) {
      bad();
    }
    w.field(o.id);
    w.field(o.frame ? "1" : "0");
    if (o.frame) {
      w.number(o.frame->x);
      w.number(o.frame->y);
      w.number(o.frame->width);
      w.number(o.frame->height);
    }
  }
  const auto hash = persistence::composition_checksum(w.bytes);
  return {std::move(w.bytes), hash};
}
Data decode(const SessionCheckpoint &c) {
  constexpr auto magic = "OBJ01-RECOVERY-LAB-1\n";
  if (c.bytes.size() > byte_limit ||
      c.bytes.compare(0, std::char_traits<char>::length(magic), magic) != 0 ||
      c.sha256.size() != 64 ||
      persistence::composition_checksum(c.bytes) != c.sha256) {
    bad();
  }
  Reader r{c.bytes, std::char_traits<char>::length(magic)};
  Data d;
  const auto kind = r.field(1), pending = r.field(1);
  if ((kind != "D" && kind != "C") || (pending != "0" && pending != "1")) {
    bad();
  }
  d.composition = kind == "C";
  d.pending = pending == "1";
  d.project_id = r.field(128);
  d.base = r.field(16 * 1024 * 1024);
  d.working = r.field(16 * 1024 * 1024);
  bounded_source(d.base);
  bounded_source(d.working);
  d.persisted_revision = r.number<std::int64_t>();
  d.timestamp = r.field(128);
  d.selection = {r.number<std::size_t>(), r.number<std::size_t>()};
  const auto k = r.number<int>();
  if (k < 0 || k > static_cast<int>(LayoutKind::Freeform)) {
    bad();
  }
  d.layout.kind = static_cast<LayoutKind>(k);
  d.layout.columns = r.number<std::size_t>();
  d.object_order = list(r);
  d.layout.children = list(r);
  d.layout.paint_order = list(r);
  const auto n = r.number<std::size_t>();
  if (n > 10000) {
    bad();
  }
  d.frames.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    Occurrence o;
    o.id = r.field(512);
    const auto has = r.field(1);
    if (has != "0" && has != "1") {
      bad();
    }
    if (has == "1") {
      o.frame = Frame{r.number<double>(), r.number<double>(),
                      r.number<double>(), r.number<double>()};
    }
    d.frames.push_back(std::move(o));
  }
  if (r.offset != c.bytes.size()) {
    bad();
  }
  return d;
}
Composition presentation(const Data &d, Composition g) {
  if (g.layouts.size() != 1 || d.frames.size() != g.occurrences.size() ||
      d.object_order.size() != g.objects.size())
    bad();
  std::unordered_map<std::string, Occurrence> occurrences;
  for (const auto &o : g.occurrences)
    occurrences.emplace(o.id, o);
  g.occurrences.clear();
  for (const auto &f : d.frames) {
    auto found = occurrences.find(f.id);
    if (found == occurrences.end())
      bad();
    auto o = found->second;
    o.frame = f.frame;
    g.occurrences.push_back(std::move(o));
    occurrences.erase(found);
  }
  std::unordered_map<std::string, Object> objects;
  for (const auto &o : g.objects)
    objects.emplace(o.id, o);
  g.objects.clear();
  for (const auto &id : d.object_order) {
    auto found = objects.find(id);
    if (found == objects.end())
      bad();
    g.objects.push_back(found->second);
    objects.erase(found);
  }
  auto &l = g.layouts[0];
  l.kind = d.layout.kind;
  l.columns = d.layout.columns;
  l.children = d.layout.children;
  l.paint_order = d.layout.paint_order;
  if (!validate(g).empty())
    bad();
  return g;
}
} // namespace inde::objects::planning::checkpoint_detail
