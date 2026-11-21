#include "object_composite_pilot.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include "object_commands.hpp"
#include "object_document_adapter.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <fcntl.h>
#include <fstream>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <sqlite3.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

namespace inde::objects::pilot {
std::string encode_json(const persistence::json::Value &v) {
  if (const auto *d = std::get_if<double>(&v.storage())) {
    if (!std::isfinite(*d))
      throw std::runtime_error("nonfinite_json_number");
    std::array<char, 64> buffer{};
    const auto r = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                                 *d, std::chars_format::general,
                                 std::numeric_limits<double>::max_digits10);
    if (r.ec != std::errc{})
      throw std::runtime_error("json_number_limit");
    return {buffer.data(), r.ptr};
  }
  if (const auto *a =
          std::get_if<persistence::json::Value::Array>(&v.storage())) {
    std::string out = "[";
    for (const auto &item : *a) {
      if (out.size() > 1)
        out += ',';
      out += encode_json(item);
    }
    return out + "]";
  }
  if (const auto *o =
          std::get_if<persistence::json::Value::Object>(&v.storage())) {
    std::string out = "{";
    for (const auto &[key, item] : *o) {
      if (out.size() > 1)
        out += ',';
      out += encode_json(persistence::json::Value(key));
      out += ':';
      out += encode_json(item);
    }
    return out + "}";
  }
  auto out = persistence::json::serialize(v, false);
  if (!out.empty() && out.back() == '\n')
    out.pop_back();
  return out;
}
namespace {
using V = persistence::json::Value;
using Bytes = std::vector<std::uint8_t>;
constexpr std::size_t mib = 1024 * 1024;
constexpr int application_id = 0x4f424a31;
[[noreturn]] void fail(const char *code) { throw std::runtime_error(code); }
std::string sha(std::span<const std::uint8_t> b) {
  std::unique_ptr<gchar, decltype(&g_free)> s(
      g_compute_checksum_for_data(G_CHECKSUM_SHA256, b.data(), b.size()),
      &g_free);
  if (!s)
    fail("sha256_unavailable");
  return s.get();
}
std::string sha(const std::string &s) {
  return sha({reinterpret_cast<const std::uint8_t *>(s.data()), s.size()});
}
void text(const std::string &s, std::size_t cap = 4096) {
  if (s.size() > cap ||
      !g_utf8_validate(s.data(), static_cast<gssize>(s.size()), nullptr))
    fail("invalid_text_or_limit");
}
std::int64_t number(std::size_t n) {
  if (n > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))
    fail("number_limit");
  return static_cast<std::int64_t>(n);
}
void fields(const V &v, std::initializer_list<const char *> keys) {
  if (v.as_object().size() != keys.size())
    fail("unknown_or_missing_fields");
  for (const auto *k : keys)
    (void)v.at(k);
}
V array(const std::vector<std::string> &values) {
  V::Array a;
  for (const auto &s : values)
    a.emplace_back(s);
  return a;
}
std::vector<std::string> strings(const V &v, std::size_t cap = 10512) {
  if (v.as_array().size() > cap)
    fail("entity_limit");
  std::vector<std::string> r;
  for (const auto &s : v.as_array()) {
    text(s.as_string(), 256);
    r.push_back(s.as_string());
  }
  return r;
}
double real(const V &v) {
  if (const auto *d = std::get_if<double>(&v.storage()))
    return *d;
  return static_cast<double>(v.as_integer());
}
Bytes base64(const std::string &s) {
  if (s.empty() || s.size() > ((8 * mib + 2) / 3) * 4 || s.size() % 4 != 0)
    fail("png_byte_limit");
  std::size_t padding{};
  if (s.back() == '=')
    ++padding;
  if (s.size() > 1 && s[s.size() - 2] == '=')
    ++padding;
  for (std::size_t i = 0; i < s.size() - padding; ++i) {
    const char c = s[i];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '+' || c == '/'))
      fail("invalid_base64");
  }
  gsize size{};
  std::unique_ptr<guchar, decltype(&g_free)> bytes(
      g_base64_decode(s.c_str(), &size), &g_free);
  if (size > 8 * mib)
    fail("png_byte_limit");
  // Reject noncanonical padding/unused bits as well as malformed alphabet.
  std::unique_ptr<gchar, decltype(&g_free)> roundtrip(
      g_base64_encode(bytes.get(), size), &g_free);
  if (!roundtrip || s != roundtrip.get())
    fail("invalid_base64");
  return {bytes.get(), bytes.get() + size};
}
std::string base64(const Bytes &b) {
  std::unique_ptr<gchar, decltype(&g_free)> s(
      g_base64_encode(b.data(), b.size()), &g_free);
  if (!s)
    fail("base64_unavailable");
  return s.get();
}
std::uint32_t u32(const Bytes &b, std::size_t n) {
  return (static_cast<std::uint32_t>(b[n]) << 24U) |
         (static_cast<std::uint32_t>(b[n + 1]) << 16U) |
         (static_cast<std::uint32_t>(b[n + 2]) << 8U) |
         static_cast<std::uint32_t>(b[n + 3]);
}
std::pair<int, int> png(const Bytes &b) {
  const std::array<std::uint8_t, 8> sig{137, 80, 78, 71, 13, 10, 26, 10};
  if (b.size() > 8 * mib)
    fail("png_byte_limit");
  if (b.size() < 45 || !std::equal(sig.begin(), sig.end(), b.begin()))
    fail("invalid_png");
  std::size_t at = 8;
  bool first = true, ended = false, data = false;
  int width = 0, height = 0;
  while (at < b.size()) {
    if (b.size() - at < 12)
      fail("invalid_png_chunk");
    const std::size_t count = u32(b, at);
    if (count > b.size() - at - 12)
      fail("invalid_png_chunk");
    const std::string type(reinterpret_cast<const char *>(b.data() + at + 4),
                           4);
    const auto crc = crc32(0, b.data() + at + 4, static_cast<uInt>(count + 4));
    if (static_cast<std::uint32_t>(crc) != u32(b, at + 8 + count))
      fail("png_crc_mismatch");
    if (first) {
      if (type != "IHDR" || count != 13)
        fail("invalid_png_header");
      const auto w = u32(b, at + 8), h = u32(b, at + 12);
      if (!w || !h || w > 8192 || h > 8192 ||
          static_cast<std::uint64_t>(w) * h > 16777216)
        fail("png_dimensions_limit");
      width = static_cast<int>(w);
      height = static_cast<int>(h);
      first = false;
    } else if (type == "IHDR")
      fail("invalid_png_header");
    if (type == "acTL" || type == "fcTL" || type == "fdAT")
      fail("animated_png_not_supported");
    if (type == "IDAT")
      data = true;
    at += count + 12;
    if (type == "IEND") {
      if (count != 0 || !data || at != b.size())
        fail("invalid_png_end");
      ended = true;
      break;
    }
  }
  if (!ended)
    fail("invalid_png_end");
  GError *error = nullptr;
  std::unique_ptr<GdkPixbufLoader, decltype(&g_object_unref)> loader(
      gdk_pixbuf_loader_new_with_type("png", &error), &g_object_unref);
  if (!loader) {
    if (error)
      g_error_free(error);
    fail("png_decoder_unavailable");
  }
  const bool written =
      gdk_pixbuf_loader_write(loader.get(), b.data(), b.size(), &error) != 0;
  if (error) {
    g_error_free(error);
    error = nullptr;
  }
  const bool closed = gdk_pixbuf_loader_close(loader.get(), &error) != 0;
  if (error)
    g_error_free(error);
  if (!written || !closed)
    fail("png_decode_failed");
  auto *image = gdk_pixbuf_loader_get_pixbuf(loader.get());
  if (!image || gdk_pixbuf_get_width(image) != width ||
      gdk_pixbuf_get_height(image) != height)
    fail("png_dimensions_mismatch");
  return {width, height};
}
struct Fd {
  int n = -1;
  ~Fd() {
    if (n >= 0)
      ::close(n);
  }
  Fd() = default;
  Fd(const Fd &) = delete;
  Fd &operator=(const Fd &) = delete;
};
void secure_file(int fd) {
  struct stat st {};
  if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
      st.st_uid != ::geteuid() || (st.st_mode & 077) != 0 || st.st_nlink != 1)
    fail("unsafe_workspace_file");
}
void create_file(const std::filesystem::path &p, const std::string &data) {
  Fd f;
  f.n = ::open(p.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
               0600);
  if (f.n < 0)
    fail("exclusive_file_creation_failed");
  std::size_t done = 0;
  while (done < data.size()) {
    const auto n = ::write(f.n, data.data() + done, data.size() - done);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      fail("fixture_write_failed");
    done += static_cast<std::size_t>(n);
  }
  if (::fsync(f.n) != 0)
    fail("fixture_sync_failed");
}
std::string read_file(const std::filesystem::path &p) {
  Fd f;
  f.n = ::open(p.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (f.n < 0)
    fail("fixture_missing");
  secure_file(f.n);
  struct stat st {};
  if (::fstat(f.n, &st) != 0 || st.st_size < 0 ||
      st.st_size > 16 * static_cast<off_t>(mib))
    fail("fixture_limit");
  std::string s(static_cast<std::size_t>(st.st_size), '\0');
  std::size_t done = 0;
  while (done < s.size()) {
    const auto n = ::read(f.n, s.data() + done, s.size() - done);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      fail("fixture_read_failed");
    done += static_cast<std::size_t>(n);
  }
  return s;
}
// Deliberately narrow SQLite adapter. The canonical SqliteTransaction owns
// OperationCapture/project removal/migration dependencies, inappropriate here.
// No canonical schema or operation-history stubs are linked into this pilot.
class Db {
  sqlite3 *db_{};

public:
  explicit Db(const std::filesystem::path &p) {
    Fd f;
    f.n = ::open(p.c_str(), O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    if (f.n < 0)
      fail("database_missing");
    secure_file(f.n);
    struct stat st {};
    if (::fstat(f.n, &st) != 0 || st.st_size > 80 * static_cast<off_t>(mib))
      fail("database_size_limit");
    const int rc = sqlite3_open_v2(
        p.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOFOLLOW, nullptr);
    if (rc != SQLITE_OK) {
      if (db_)
        sqlite3_close(db_);
      db_ = nullptr;
      fail("sqlite_open_failed");
    }
    sqlite3_busy_timeout(db_, 2500);
    sqlite3_limit(db_, SQLITE_LIMIT_LENGTH, 48 * static_cast<int>(mib));
    sqlite3_limit(db_, SQLITE_LIMIT_SQL_LENGTH, 32768);
  }
  ~Db() {
    if (db_)
      sqlite3_close(db_);
  }
  Db(const Db &) = delete;
  Db &operator=(const Db &) = delete;
  void exec(const char *sql) {
    if (sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
      fail("sqlite_operation_failed");
  }
  sqlite3 *handle() { return db_; }
};
class Query {
  sqlite3_stmt *q_{};

public:
  Query(Db &db, const char *sql) {
    if (sqlite3_prepare_v2(db.handle(), sql, -1, &q_, nullptr) != SQLITE_OK)
      fail("sqlite_schema_or_query_failed");
  }
  ~Query() {
    if (q_)
      sqlite3_finalize(q_);
  }
  Query(const Query &) = delete;
  Query &operator=(const Query &) = delete;
  void bind(int i, const std::string &s) {
    if (sqlite3_bind_text(q_, i, s.data(), static_cast<int>(s.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK)
      fail("sqlite_bind_failed");
  }
  void bind(int i, std::int64_t n) {
    if (sqlite3_bind_int64(q_, i, n) != SQLITE_OK)
      fail("sqlite_bind_failed");
  }
  void bind(int i, const Bytes &b) {
    if (sqlite3_bind_blob64(q_, i, b.data(), b.size(), SQLITE_TRANSIENT) !=
        SQLITE_OK)
      fail("sqlite_bind_failed");
  }
  bool step() {
    const int rc = sqlite3_step(q_);
    if (rc == SQLITE_ROW)
      return true;
    if (rc == SQLITE_DONE)
      return false;
    fail("sqlite_step_failed");
  }
  std::int64_t integer(int i) {
    if (sqlite3_column_type(q_, i) != SQLITE_INTEGER)
      fail("sqlite_value_type");
    return sqlite3_column_int64(q_, i);
  }
  std::string string(int i) {
    if (sqlite3_column_type(q_, i) != SQLITE_TEXT)
      fail("sqlite_value_type");
    return {reinterpret_cast<const char *>(sqlite3_column_text(q_, i)),
            static_cast<std::size_t>(sqlite3_column_bytes(q_, i))};
  }
  Bytes bytes(int i) {
    if (sqlite3_column_type(q_, i) != SQLITE_BLOB)
      fail("sqlite_value_type");
    const auto *p =
        static_cast<const std::uint8_t *>(sqlite3_column_blob(q_, i));
    const int n = sqlite3_column_bytes(q_, i);
    if (n <= 0)
      fail("empty_blob");
    return {p, p + n};
  }
};
struct Tx {
  Db &db;
  bool active = true;
  explicit Tx(Db &d) : db(d) { db.exec("BEGIN IMMEDIATE"); }
  ~Tx() {
    if (active) {
      try {
        db.exec("ROLLBACK");
      } catch (...) {
      }
    }
  }
  void commit() {
    db.exec("COMMIT");
    active = false;
  }
};
struct Figure {
  std::string occurrence, object, resource, hash, caption, alt;
  bool decorative = false;
  int width{}, height{};
};
struct Row {
  std::string layout, object, occurrence;
  std::vector<std::string> children;
  double first = .5, second = .5, gap = 20;
};
struct State {
  std::string project, source, source_hash, selection;
  std::vector<std::string> author, visual;
  std::vector<Figure> figures;
  std::vector<Row> rows;
  std::map<std::string, Bytes> blobs;
};
std::unique_ptr<compat::DocumentReadProjection> projection(const State &s) {
  auto d = persistence::decode_document_snapshot(s.source);
  auto r = compat::read_document(s.project, d);
  if (!r.projection || r.projection->source().snapshot_sha256 != s.source_hash)
    fail("invalid_source_capture");
  return std::move(r.projection);
}
const Figure &figure(const State &s, const std::string &id) {
  const auto it =
      std::find_if(s.figures.begin(), s.figures.end(),
                   [&](const auto &f) { return f.occurrence == id; });
  if (it == s.figures.end())
    fail("figure_not_found");
  return *it;
}
Composition graph(const State &s,
                  const compat::DocumentReadProjection &source) {
  Composition g = source.graph();
  std::set<std::string> objects, resources;
  for (const auto &f : s.figures) {
    if (objects.insert(f.object).second)
      g.objects.push_back({f.object, ObjectKind::Image, f.resource, {}});
    if (resources.insert(f.resource).second)
      g.resources.push_back(
          {f.resource, f.hash, "image/png",
           static_cast<std::uint64_t>(s.blobs.at(f.hash).size())});
    g.occurrences.push_back({f.occurrence, f.object, {}});
  }
  auto &children = g.layouts.front().children;
  children.clear();
  for (const auto &id : s.visual) {
    auto row = std::find_if(s.rows.begin(), s.rows.end(),
                            [&](const auto &r) { return r.children[0] == id; });
    if (row != s.rows.end())
      children.push_back(row->occurrence);
    else if (std::none_of(s.rows.begin(), s.rows.end(),
                          [&](const auto &r) { return r.children[1] == id; }))
      children.push_back(id);
  }
  for (const auto &r : s.rows) {
    g.objects.push_back({r.object, ObjectKind::Container, {}, r.layout});
    g.occurrences.push_back({r.occurrence, r.object, {}});
    g.layouts.push_back({r.layout, LayoutKind::Row, 0, r.children});
  }
  return g;
}
void validate(const State &s) {
  if (s.figures.size() > 256 || s.rows.size() > 128 || s.blobs.size() > 64)
    fail("entity_limit");
  text(s.source, 16 * mib);
  text(s.selection, 256);
  if (s.source.size() + s.selection.size() > 8 * mib)
    fail("document_string_limit");
  if (sha(s.source) != s.source_hash)
    fail("source_hash_mismatch");
  const auto p = projection(s);
  std::set<std::string> expected;
  std::vector<std::string> paragraph_order;
  for (const auto &b : p->paragraphs()) {
    expected.insert(b.occurrence_id);
    paragraph_order.push_back(b.occurrence_id);
  }
  const auto paragraph_ids = expected;
  std::map<std::string, std::pair<std::string, std::string>> object_links;
  std::map<std::string, std::string> resource_links;
  std::map<std::string, std::pair<int, int>> decoded;
  std::set<std::string> hashes;
  std::size_t string_bytes = s.source.size() + s.selection.size();
  for (const auto &f : s.figures) {
    for (const auto *v : {&f.occurrence, &f.object, &f.resource}) {
      text(*v, 256);
      if (v->empty())
        fail("empty_id");
    }
    text(f.caption);
    text(f.alt);
    string_bytes += f.occurrence.size() + f.object.size() + f.resource.size() +
                    f.hash.size() + f.caption.size() + f.alt.size();
    if (string_bytes > 8 * mib)
      fail("document_string_limit");
    if (!f.decorative && f.alt.empty())
      fail("alt_text_required");
    if (f.occurrence == f.object || f.object == f.resource ||
        f.occurrence == f.resource || !expected.insert(f.occurrence).second)
      fail("identity_collision");
    const auto [obj, inserted] =
        object_links.emplace(f.object, std::pair{f.resource, f.hash});
    if (!inserted && obj->second != std::pair{f.resource, f.hash})
      fail("shared_object_mismatch");
    const auto [res, fresh] = resource_links.emplace(f.resource, f.hash);
    if (!fresh && res->second != f.hash)
      fail("shared_resource_mismatch");
    hashes.insert(f.hash);
    if (!s.blobs.contains(f.hash))
      fail("blob_missing");
    if (!decoded.contains(f.hash)) {
      const auto &b = s.blobs.at(f.hash);
      if (sha(b) != f.hash)
        fail("blob_hash_mismatch");
      decoded.emplace(f.hash, png(b));
    }
    if (decoded.at(f.hash) != std::pair{f.width, f.height})
      fail("png_dimensions_mismatch");
  }
  std::size_t total = 0;
  for (const auto &[hash, b] : s.blobs) {
    if (!hashes.contains(hash))
      fail("unreferenced_blob");
    total += b.size();
    if (total > 32 * mib)
      fail("blob_total_limit");
  }
  const auto complete = [&](const auto &order) {
    return order.size() == expected.size() &&
           std::set<std::string>(order.begin(), order.end()) == expected;
  };
  if (!complete(s.author) || !complete(s.visual))
    fail("occurrence_order_mismatch");
  std::vector<std::string> filtered;
  for (const auto &id : s.author)
    if (paragraph_ids.contains(id))
      filtered.push_back(id);
  if (filtered != paragraph_order)
    fail("paragraph_order_change_out_of_scope");
  for (const auto &group : p->reading_groups()) {
    if (group.occurrence_ids.empty())
      fail("empty_reading_group");
    const auto at = std::find(s.author.begin(), s.author.end(),
                              group.occurrence_ids.front());
    const auto start = static_cast<std::size_t>(at - s.author.begin());
    if (start + group.occurrence_ids.size() > s.author.size() ||
        !std::equal(group.occurrence_ids.begin(), group.occurrence_ids.end(),
                    s.author.begin() + static_cast<std::ptrdiff_t>(start)))
      fail("reading_group_split");
  }
  std::set<std::string> paired;
  for (const auto &r : s.rows) {
    for (const auto *id : {&r.layout, &r.object, &r.occurrence}) {
      text(*id, 256);
      if (id->empty())
        fail("empty_id");
    }
    if (r.children.size() != 2 || !std::isfinite(r.first) ||
        !std::isfinite(r.second) || r.first <= 0 || r.second <= 0 ||
        std::abs(r.first + r.second - 1) > 1e-9 || !std::isfinite(r.gap) ||
        r.gap < 0 || r.gap > 256)
      fail("invalid_row_profile");
    const auto it = std::find(s.visual.begin(), s.visual.end(), r.children[0]);
    if (it == s.visual.end() || std::next(it) == s.visual.end() ||
        *std::next(it) != r.children[1])
      fail("row_not_adjacent");
    bool has_paragraph = false, has_figure = false;
    for (const auto &id : r.children) {
      if (!paired.insert(id).second)
        fail("row_overlap");
      has_paragraph = has_paragraph || paragraph_ids.contains(id);
      has_figure = has_figure || std::any_of(s.figures.begin(), s.figures.end(),
                                             [&](const auto &f) {
                                               return f.occurrence == id;
                                             });
    }
    if (!has_paragraph || !has_figure)
      fail("row_requires_paragraph_and_figure");
  }
  if (!s.selection.empty() && !expected.contains(s.selection))
    fail("selection_not_found");
  if (!objects::validate(graph(s, *p)).empty())
    fail("invalid_object_graph");
}
V encode(const State &s) {
  V::Array figures, rows;
  for (const auto &f : s.figures)
    figures.emplace_back(V::Object{{"occurrence", f.occurrence},
                                   {"object", f.object},
                                   {"resource", f.resource},
                                   {"sha256", f.hash},
                                   {"caption", f.caption},
                                   {"alt", f.alt},
                                   {"decorative", f.decorative},
                                   {"width", f.width},
                                   {"height", f.height}});
  for (const auto &r : s.rows)
    rows.emplace_back(V::Object{{"layout", r.layout},
                                {"object", r.object},
                                {"occurrence", r.occurrence},
                                {"children", array(r.children)},
                                {"first", r.first},
                                {"second", r.second},
                                {"gap", r.gap}});
  return V::Object{{"format", "INDE-OBJ01-COMPOSITE-P2A-1"},
                   {"project", s.project},
                   {"source", s.source},
                   {"source_sha256", s.source_hash},
                   {"author_order", array(s.author)},
                   {"visual_order", array(s.visual)},
                   {"figures", std::move(figures)},
                   {"rows", std::move(rows)},
                   {"selection", s.selection}};
}
State decode(const std::string &bytes) {
  text(bytes, 24 * mib);
  // The format is shallow, including the owner snapshot as an opaque string.
  int depth = 0;
  bool quoted = false, escaped = false;
  for (char c : bytes) {
    if (quoted) {
      if (escaped)
        escaped = false;
      else if (c == '\\')
        escaped = true;
      else if (c == '"')
        quoted = false;
    } else if (c == '"')
      quoted = true;
    else if (c == '[' || c == '{') {
      if (++depth > 8)
        fail("snapshot_depth");
    } else if (c == ']' || c == '}')
      --depth;
  }
  const auto v = persistence::json::parse(bytes);
  fields(v, {"format", "project", "source", "source_sha256", "author_order",
             "visual_order", "figures", "rows", "selection"});
  if (v.at("format").as_string() != "INDE-OBJ01-COMPOSITE-P2A-1")
    fail("unsupported_snapshot_version");
  State s;
  s.project = v.at("project").as_string();
  s.source = v.at("source").as_string();
  s.source_hash = v.at("source_sha256").as_string();
  s.selection = v.at("selection").as_string();
  s.author = strings(v.at("author_order"));
  s.visual = strings(v.at("visual_order"));
  if (v.at("figures").as_array().size() > 256 ||
      v.at("rows").as_array().size() > 128)
    fail("entity_limit");
  for (const auto &f : v.at("figures").as_array()) {
    fields(f, {"occurrence", "object", "resource", "sha256", "caption", "alt",
               "decorative", "width", "height"});
    const auto w = f.at("width").as_integer(), h = f.at("height").as_integer();
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192)
      fail("png_dimensions_limit");
    s.figures.push_back(
        {f.at("occurrence").as_string(), f.at("object").as_string(),
         f.at("resource").as_string(), f.at("sha256").as_string(),
         f.at("caption").as_string(), f.at("alt").as_string(),
         f.at("decorative").as_boolean(), static_cast<int>(w),
         static_cast<int>(h)});
  }
  for (const auto &r : v.at("rows").as_array()) {
    fields(r, {"layout", "object", "occurrence", "children", "first", "second",
               "gap"});
    s.rows.push_back({r.at("layout").as_string(), r.at("object").as_string(),
                      r.at("occurrence").as_string(),
                      strings(r.at("children"), 2), real(r.at("first")),
                      real(r.at("second")), real(r.at("gap"))});
  }
  return s;
}
State fixture() {
  project::Document d;
  d.id = project::new_uuid();
  d.title = "Travessia — experimento de texto e figura";
  d.created_at = d.updated_at = "2026-11-21T12:00:00Z";
  d.content = "Água, luz e memória.\nO horizonte abre uma passagem.\nA casa "
              "guarda outra história 🙂.";
  d.paragraphs = project::identify_paragraphs(d.content);
  d.formatting = {{project::DocumentTextStyle::Bold, 0, 4, {}}};
  d.anchors = {{project::new_uuid(), "Água declarada pelo autor", 0, 4}};
  const auto top = project::new_uuid();
  auto leaf = std::make_shared<project::DocumentLeafContent>();
  leaf->text = "FOLHA PRIVADA — alternativa que não é publicada.";
  d.resmas = {
      {project::new_uuid(),
       "Grupo de leitura preservado",
       top,
       d.paragraphs.rows[1].start,
       d.paragraphs.rows[2].end,
       {{top, "Corrente", {}}, {project::new_uuid(), "Coberta", leaf}}}};
  d.visibility_profiles = {
      {project::new_uuid(),
       "Lente privada",
       true,
       {},
       {{project::VisibilityRuleKind::Paragraph, d.paragraphs.rows[2].id}}}};
  State s;
  s.project = project::new_uuid();
  s.source = persistence::encode_document_snapshot(d);
  s.source_hash = sha(s.source);
  const auto p = projection(s);
  for (const auto &b : p->paragraphs())
    s.author.push_back(b.occurrence_id);
  s.visual = s.author;
  validate(s);
  return s;
}
void unpair(State &s, const std::string &id) {
  std::erase_if(s.rows, [&](const auto &r) {
    return std::find(r.children.begin(), r.children.end(), id) !=
           r.children.end();
  });
}
} // namespace
struct Session::Impl {
  std::filesystem::path workspace;
  Fd lease;
  std::unique_ptr<Db> db;
  State working;
  std::string saved_hash, saved_bytes;
  std::int64_t persisted = 0, revision = 0;

  explicit Impl(std::filesystem::path path)
      : workspace(
            std::filesystem::absolute(std::move(path)).lexically_normal()) {
    for (const auto &part : workspace)
      if (part.extension() == ".inde")
        fail("legacy_project_path_forbidden");
    if (::mkdir(workspace.c_str(), 0700) != 0 && errno != EEXIST)
      fail("workspace_creation_failed");
    struct stat st {};
    if (::lstat(workspace.c_str(), &st) != 0 || !S_ISDIR(st.st_mode) ||
        st.st_uid != ::geteuid() || (st.st_mode & 077) != 0)
      fail("unsafe_workspace_directory");
    const auto database = workspace / "obj01-piloto.sqlite3";
    const bool exists = std::filesystem::symlink_status(database).type() !=
                        std::filesystem::file_type::not_found;
    if (!exists && !std::filesystem::is_empty(workspace))
      fail("new_workspace_must_be_empty");
    lease.n = ::open((workspace / ".pilot.lock").c_str(),
                     O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (lease.n < 0)
      fail("workspace_lease_failed");
    secure_file(lease.n);
    if (::flock(lease.n, LOCK_EX | LOCK_NB) != 0)
      fail("workspace_already_open");
    if (!exists) {
      working = fixture();
      create_file(workspace / "origem-documento.json", working.source);
      create_file(database, "");
      db = std::make_unique<Db>(database);
      db->exec("PRAGMA synchronous=FULL");
      Tx tx(*db);
      db->exec("PRAGMA application_id=1329744433; PRAGMA user_version=1;");
      db->exec("CREATE TABLE saved(id INTEGER PRIMARY KEY CHECK(id=1), "
               "revision INTEGER NOT NULL CHECK(revision>=0), sha256 TEXT NOT "
               "NULL, snapshot TEXT NOT NULL); CREATE TABLE blobs(sha256 TEXT "
               "PRIMARY KEY, data BLOB NOT NULL);");
      saved_bytes = encode_json(encode(working));
      saved_hash = sha(saved_bytes);
      Query q(*db, "INSERT INTO saved VALUES(1,0,?,?)");
      q.bind(1, saved_hash);
      q.bind(2, saved_bytes);
      (void)q.step();
      tx.commit();
      Fd folder;
      folder.n = ::open(workspace.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
      if (folder.n < 0 || ::fsync(folder.n) != 0)
        fail("workspace_sync_failed");
    } else
      db = std::make_unique<Db>(database);
    auto loaded = load();
    working = std::move(loaded.state);
    saved_bytes = std::move(loaded.bytes);
    saved_hash = std::move(loaded.hash);
    persisted = loaded.rev;
    db->exec("PRAGMA synchronous=FULL");
  }
  struct Loaded {
    State state;
    std::string bytes, hash;
    std::int64_t rev;
  };
  Loaded load() {
    Query app(*db, "PRAGMA application_id");
    if (!app.step() || app.integer(0) != application_id)
      fail("not_a_pilot_database");
    Query ver(*db, "PRAGMA user_version");
    if (!ver.step() || ver.integer(0) != 1)
      fail("unsupported_database_version");
    Query journal(*db, "PRAGMA journal_mode");
    if (!journal.step() || journal.string(0) != "delete")
      fail("unsupported_journal_mode");
    Query q(*db, "SELECT revision, sha256, length(CAST(snapshot AS BLOB)), "
                 "snapshot FROM saved WHERE id=1");
    if (!q.step())
      fail("saved_document_missing");
    const auto rev = q.integer(0), size = q.integer(2);
    auto hash = q.string(1);
    if (rev < 0 || size < 0 || size > 24 * static_cast<std::int64_t>(mib))
      fail("saved_document_limit");
    auto bytes = q.string(3);
    if (sha(bytes) != hash)
      fail("saved_hash_mismatch");
    State s = decode(bytes);
    if (read_file(workspace / "origem-documento.json") != s.source)
      fail("frozen_source_changed");
    Query count(*db,
                "SELECT count(*), COALESCE(sum(length(data)),0) FROM blobs");
    if (!count.step() || count.integer(0) > 64 ||
        count.integer(1) > 32 * static_cast<std::int64_t>(mib))
      fail("blob_total_limit");
    Query blob(*db, "SELECT sha256, length(data), data FROM blobs");
    while (blob.step()) {
      const auto n = blob.integer(1);
      if (n <= 0 || n > 8 * static_cast<std::int64_t>(mib))
        fail("png_byte_limit");
      s.blobs.emplace(blob.string(0), blob.bytes(2));
    }
    validate(s);
    return {std::move(s), std::move(bytes), std::move(hash), rev};
  }
  bool dirty() const { return encode_json(encode(working)) != saved_bytes; }
  V view() const {
    const auto p = projection(working);
    V::Array paragraphs, figures, rows, groups;
    for (const auto &b : p->paragraphs())
      paragraphs.emplace_back(V::Object{
          {"id", b.occurrence_id},
          {"object", b.object_id},
          {"paragraph", b.paragraph_id},
          {"text", std::string(*p->paragraph_text(b.occurrence_id))}});
    for (const auto &f : working.figures)
      figures.emplace_back(
          V::Object{{"id", f.occurrence},
                    {"object", f.object},
                    {"resource", f.resource},
                    {"sha256", f.hash},
                    {"caption", f.caption},
                    {"alt", f.alt},
                    {"decorative", f.decorative},
                    {"width", f.width},
                    {"height", f.height},
                    {"bytes", number(working.blobs.at(f.hash).size())}});
    for (const auto &r : working.rows)
      rows.emplace_back(V::Object{{"id", r.layout},
                                  {"occurrence", r.occurrence},
                                  {"children", array(r.children)},
                                  {"first", r.first},
                                  {"second", r.second},
                                  {"gap", r.gap}});
    for (const auto &g : p->reading_groups())
      groups.emplace_back(array(g.occurrence_ids));
    return V::Object{
        {"revision", revision},
        {"saved_revision", persisted},
        {"dirty", dirty()},
        {"source_sha256", working.source_hash},
        {"source_body_sha256", sha(p->source_snapshot().content)},
        {"source_document", p->source().document_id},
        {"paragraphs", std::move(paragraphs)},
        {"figures", std::move(figures)},
        {"rows", std::move(rows)},
        {"reading_groups", std::move(groups)},
        {"author_order", array(working.author)},
        {"visual_order", array(working.visual)},
        {"selection", working.selection},
        {"aligned", working.author == working.visual},
        {"private_alternatives", number(p->source_snapshot().resmas.size())},
        {"visibility_profiles",
         number(p->source_snapshot().visibility_profiles.size())},
        {"unsaved_recovery", false}};
  }
  void increment() {
    if (revision == std::numeric_limits<std::int64_t>::max())
      fail("revision_limit");
    ++revision;
  }
  void expected(const V &v) const {
    if (v.at("expected").as_integer() != revision)
      fail("stale_session_revision");
  }
  V request(const V &v) {
    try {
      const auto op = v.at("op").as_string();
      if (op == "view") {
        fields(v, {"op"});
        return V::Object{{"accepted", true}, {"state", view()}};
      }
      if (op == "image") {
        fields(v, {"op", "resource"});
        const auto id = v.at("resource").as_string();
        const auto it =
            std::find_if(working.figures.begin(), working.figures.end(),
                         [&](const auto &f) { return f.resource == id; });
        if (it == working.figures.end())
          fail("resource_not_found");
        return V::Object{{"accepted", true},
                         {"mime", "image/png"},
                         {"sha256", it->hash},
                         {"data", base64(working.blobs.at(it->hash))}};
      }
      expected(v);
      if (revision == std::numeric_limits<std::int64_t>::max())
        fail("revision_limit");
      if (op == "save") {
        fields(v, {"op", "expected"});
        validate(working);
        auto bytes = encode_json(encode(working));
        auto hash = sha(bytes);
        Tx tx(*db);
        const auto current = load();
        if (current.rev != persisted || current.hash != saved_hash)
          fail("persisted_revision_conflict");
        if (current.bytes != saved_bytes)
          fail("persisted_snapshot_conflict");
        if (persisted == std::numeric_limits<std::int64_t>::max())
          fail("revision_limit");
        const auto next = persisted + 1;
        for (const auto &[h, b] : working.blobs) {
          Query insert(*db, "INSERT INTO blobs(sha256,data) VALUES(?,?) ON "
                            "CONFLICT(sha256) DO NOTHING");
          insert.bind(1, h);
          insert.bind(2, b);
          (void)insert.step();
        }
        Query update(*db, "UPDATE saved SET revision=?,sha256=?,snapshot=? "
                          "WHERE id=1 AND revision=? AND sha256=?");
        update.bind(1, next);
        update.bind(2, hash);
        update.bind(3, bytes);
        update.bind(4, persisted);
        update.bind(5, saved_hash);
        (void)update.step();
        if (sqlite3_changes(db->handle()) != 1)
          fail("persisted_revision_conflict");
        const auto checked = load();
        if (checked.bytes != bytes || checked.rev != next)
          fail("save_readback_mismatch");
        tx.commit();
        saved_bytes = std::move(bytes);
        saved_hash = std::move(hash);
        persisted = next;
        increment();
      } else if (op == "reopen") {
        fields(v, {"op", "expected", "discard_unsaved"});
        if (dirty() && !v.at("discard_unsaved").as_boolean())
          fail("unsaved_confirmation_required");
        auto current = load();
        working = std::move(current.state);
        saved_bytes = std::move(current.bytes);
        saved_hash = std::move(current.hash);
        persisted = current.rev;
        increment();
      } else {
        State candidate = working;
        if (op == "import") {
          fields(v, {"op", "expected", "data", "before", "caption", "alt",
                     "decorative"});
          if (candidate.author != candidate.visual)
            fail("authorial_requires_alignment");
          auto bytes = base64(v.at("data").as_string());
          const auto [w, h] = png(bytes);
          const auto hash = sha(bytes);
          Figure f{project::new_uuid(),
                   project::new_uuid(),
                   project::new_uuid(),
                   hash,
                   v.at("caption").as_string(),
                   v.at("alt").as_string(),
                   v.at("decorative").as_boolean(),
                   w,
                   h};
          insert(candidate, f, v.at("before").as_string());
          candidate.blobs.emplace(hash, std::move(bytes));
        } else if (op == "duplicate") {
          fields(v, {"op", "expected", "id", "before"});
          if (candidate.author != candidate.visual)
            fail("authorial_requires_alignment");
          auto f = figure(candidate, v.at("id").as_string());
          f.occurrence = project::new_uuid();
          insert(candidate, f, v.at("before").as_string());
        } else if (op == "move") {
          fields(v, {"op", "expected", "id", "before", "intent"});
          const auto id = v.at("id").as_string(),
                     before = v.at("before").as_string(),
                     intent = v.at("intent").as_string();
          (void)figure(candidate, id);
          if (intent != "authorial" && intent != "visual")
            fail("invalid_intent");
          if (intent == "authorial" && candidate.author != candidate.visual)
            fail("authorial_requires_alignment");
          unpair(candidate, id);
          const auto p = projection(candidate);
          auto g = graph(candidate, *p);
          std::string anchor = before;
          if (!before.empty()) {
            for (const auto &r : candidate.rows) {
              if (r.children[0] == before)
                anchor = r.occurrence;
              else if (r.children[1] == before)
                fail("destination_inside_row");
            }
          }
          CompositionEditor editor(g);
          auto r = editor.apply(
              0, {ReorderOccurrences{
                     g.root_layout_id,
                     {id},
                     anchor.empty() ? std::nullopt
                                    : std::optional<std::string>(anchor)}});
          if (!r.accepted)
            fail("object_move_refused");
          candidate.visual.clear();
          for (const auto &child : editor.current().layouts.front().children) {
            const auto row = std::find_if(
                candidate.rows.begin(), candidate.rows.end(),
                [&](const auto &a) { return a.occurrence == child; });
            if (row == candidate.rows.end())
              candidate.visual.push_back(child);
            else
              candidate.visual.insert(candidate.visual.end(),
                                      row->children.begin(),
                                      row->children.end());
          }
          if (intent == "authorial")
            candidate.author = candidate.visual;
          candidate.selection = id;
        } else if (op == "arrange") {
          fields(v, {"op", "expected", "id", "paragraph", "first", "second",
                     "gap"});
          const auto id = v.at("id").as_string(),
                     para = v.at("paragraph").as_string();
          (void)figure(candidate, id);
          const auto p = projection(candidate);
          if (std::none_of(
                  p->paragraphs().begin(), p->paragraphs().end(),
                  [&](const auto &b) { return b.occurrence_id == para; }))
            fail("paragraph_not_found");
          unpair(candidate, id);
          unpair(candidate, para);
          const auto a = std::find(candidate.visual.begin(),
                                   candidate.visual.end(), id),
                     b = std::find(candidate.visual.begin(),
                                   candidate.visual.end(), para);
          if (std::abs(a - b) != 1)
            fail("row_requires_adjacent_items");
          const auto first = std::min(a, b);
          candidate.rows.push_back({project::new_uuid(),
                                    project::new_uuid(),
                                    project::new_uuid(),
                                    {*first, *std::next(first)},
                                    real(v.at("first")),
                                    real(v.at("second")),
                                    real(v.at("gap"))});
          candidate.selection = id;
        } else if (op == "stack") {
          fields(v, {"op", "expected", "id"});
          (void)figure(candidate, v.at("id").as_string());
          unpair(candidate, v.at("id").as_string());
          candidate.selection = v.at("id").as_string();
        } else if (op == "restore") {
          fields(v, {"op", "expected"});
          candidate.visual = candidate.author;
          candidate.rows.clear();
        } else if (op == "select") {
          fields(v, {"op", "expected", "id"});
          candidate.selection = v.at("id").as_string();
        } else
          fail("unsupported_operation");
        validate(candidate);
        working = std::move(candidate);
        increment();
      }
      return V::Object{{"accepted", true}, {"state", view()}};
    } catch (const std::exception &e) {
      return V::Object{
          {"accepted", false}, {"error", e.what()}, {"state", view()}};
    }
  }
  static void insert(State &s, const Figure &f, const std::string &before) {
    const auto at = before.empty()
                        ? s.visual.end()
                        : std::find(s.visual.begin(), s.visual.end(), before);
    if (!before.empty() && at == s.visual.end())
      fail("destination_not_found");
    for (const auto &r : s.rows)
      if (r.children[1] == before)
        fail("destination_inside_row");
    s.visual.insert(at, f.occurrence);
    s.author = s.visual;
    s.figures.push_back(f);
    s.selection = f.occurrence;
  }
};
Session::Session(const std::filesystem::path &path)
    : impl_(std::make_unique<Impl>(path)) {}
Session::~Session() = default;
persistence::json::Value Session::request(const persistence::json::Value &v) {
  return impl_->request(v);
}
} // namespace inde::objects::pilot
