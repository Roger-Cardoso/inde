#include "object_composite_pilot.hpp"
#include "inde/application/document_reading_order.hpp"
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
  explicit Db(const std::filesystem::path &p, bool readonly = false) {
    Fd f;
    f.n = ::open(p.c_str(),
                 (readonly ? O_RDONLY : O_RDWR) | O_NOFOLLOW | O_CLOEXEC);
    if (f.n < 0)
      fail("database_missing");
    secure_file(f.n);
    struct stat st {};
    if (::fstat(f.n, &st) != 0 || st.st_size > 80 * static_cast<off_t>(mib))
      fail("database_size_limit");
    const int rc = sqlite3_open_v2(
        p.c_str(), &db_,
        (readonly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE) |
            SQLITE_OPEN_NOFOLLOW,
        nullptr);
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
struct Cell {
  std::string id, row, column, text;
};
struct Table {
  std::string occurrence, object, title;
  bool header = true;
  std::int64_t payload_revision = 0;
  std::vector<std::string> rows, columns;
  std::vector<Cell> cells;
};
struct State {
  std::string project, source, source_hash, selection;
  std::vector<std::string> author, visual;
  std::vector<Figure> figures, archived_figures;
  std::vector<Table> tables, archived_tables;
  std::string selected_cell;
  bool p2b = false, p2c = false;
  std::string working_text, context = "structure";
  std::vector<Row> rows;
  std::map<std::string, std::shared_ptr<const Bytes>> blobs;
};
std::unique_ptr<compat::DocumentReadProjection> projection(const State &s) {
  auto d =
      persistence::decode_document_snapshot(s.p2c ? s.working_text : s.source);
  auto r = compat::read_document(s.project, d);
  if (!r.projection || r.projection->source().snapshot_sha256 !=
                           (s.p2c ? sha(s.working_text) : s.source_hash))
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
  auto assets = s.figures;
  assets.insert(assets.end(), s.archived_figures.begin(),
                s.archived_figures.end());
  for (const auto &f : assets) {
    if (objects.insert(f.object).second)
      g.objects.push_back({f.object, ObjectKind::Image, f.resource, {}});
    if (resources.insert(f.resource).second)
      g.resources.push_back(
          {f.resource, f.hash, "image/png",
           static_cast<std::uint64_t>(s.blobs.at(f.hash)->size())});
    if (std::any_of(s.figures.begin(), s.figures.end(), [&](const auto &a) {
          return a.occurrence == f.occurrence;
        }))
      g.occurrences.push_back({f.occurrence, f.object, {}});
  }
  for (const auto &t : s.tables) {
    g.objects.push_back({t.object, ObjectKind::Table, {}, {}});
    g.occurrences.push_back({t.occurrence, t.object, {}});
  }
  for (const auto &t : s.archived_tables)
    g.objects.push_back({t.object, ObjectKind::Table, {}, {}});
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
void validate_owner_reorder(const State &s) {
  if (!s.p2c)
    return;
  text(s.working_text, 16 * mib);
  if (s.context != "text" && s.context != "structure" &&
      s.context != "composition")
    fail("invalid_editing_context");
  if (s.working_text == s.source)
    return;
  application::DocumentEditState expected{
      persistence::decode_document_snapshot(s.source), {}};
  const auto actual = persistence::decode_document_snapshot(s.working_text);
  if (actual.id != expected.document.id)
    fail("working_document_identity_changed");
  const auto target = application::reading_blocks(actual);
  auto blocks = application::reading_blocks(expected.document);
  if (target.size() != blocks.size() || target.size() > 256)
    fail("working_reading_groups_changed");
  for (std::size_t i = 0; i < target.size(); ++i) {
    blocks = application::reading_blocks(expected.document);
    if (blocks[i].id != target[i].id)
      expected =
          application::move_reading_block(expected, target[i].id, blocks[i].id);
  }
  if (persistence::encode_document_snapshot(expected.document) !=
      s.working_text)
    fail("working_text_not_owner_reorder");
}
V encode(const State &s);
void validate(const State &s, bool decode_media = true) {
  if (s.figures.size() + s.archived_figures.size() > 256 ||
      s.tables.size() + s.archived_tables.size() > 64 || s.rows.size() > 128 ||
      s.blobs.size() > 64)
    fail("entity_limit");
  text(s.source, 16 * mib);
  validate_owner_reorder(s);
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
  std::size_t string_bytes = s.source.size() + s.selection.size() +
                             s.selected_cell.size() + s.project.size() +
                             s.source_hash.size();
  for (const auto &id : s.author)
    string_bytes += id.size();
  for (const auto &id : s.visual)
    string_bytes += id.size();
  for (const auto &r : s.rows) {
    string_bytes += r.layout.size() + r.object.size() + r.occurrence.size();
    for (const auto &id : r.children)
      string_bytes += id.size();
  }
  for (const auto &[hash, bytes] : s.blobs) {
    (void)bytes;
    string_bytes += hash.size();
  }
  if (!s.p2b && (!s.tables.empty() || !s.archived_tables.empty() ||
                 !s.archived_figures.empty() || !s.selected_cell.empty()))
    fail("p2b_profile_required");
  auto assets = s.figures;
  assets.insert(assets.end(), s.archived_figures.begin(),
                s.archived_figures.end());
  std::set<std::string> figure_ids;
  for (const auto &f : assets) {
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
        f.occurrence == f.resource || !figure_ids.insert(f.occurrence).second)
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
    if (decode_media && !decoded.contains(f.hash)) {
      const auto &b = *s.blobs.at(f.hash);
      if (sha(b) != f.hash)
        fail("blob_hash_mismatch");
      decoded.emplace(f.hash, png(b));
    }
    if (decode_media && decoded.at(f.hash) != std::pair{f.width, f.height})
      fail("png_dimensions_mismatch");
  }
  for (const auto &f : s.figures)
    if (!expected.insert(f.occurrence).second)
      fail("identity_collision");
  std::set<std::string> nested_ids = expected;
  for (const auto &o : p->graph().objects)
    nested_ids.insert(o.id);
  for (const auto &l : p->graph().layouts)
    nested_ids.insert(l.id);
  for (const auto &f : assets) {
    nested_ids.insert(f.object);
    nested_ids.insert(f.resource);
  }
  for (const auto &r : s.rows) {
    nested_ids.insert(r.layout);
    nested_ids.insert(r.object);
    nested_ids.insert(r.occurrence);
  }
  for (const auto &f : s.archived_figures) {
    if (!nested_ids.insert(f.occurrence).second)
      fail("retired_occurrence_collision");
  }
  const auto table_check = [&](const Table &t, bool active) {
    const auto identity = [&](const std::string &id) {
      text(id, 256);
      if (id.empty() || !nested_ids.insert(id).second)
        fail("table_identity_collision");
    };
    identity(t.occurrence);
    identity(t.object);
    text(t.title);
    if (t.payload_revision < 0 || t.rows.empty() || t.rows.size() > 64 ||
        t.columns.empty() || t.columns.size() > 32 ||
        t.cells.size() != t.rows.size() * t.columns.size())
      fail("invalid_table_shape");
    for (const auto &id : t.rows) {
      identity(id);
      string_bytes += id.size();
    }
    for (const auto &id : t.columns) {
      identity(id);
      string_bytes += id.size();
    }
    std::set<std::pair<std::string, std::string>> coordinates;
    string_bytes += t.title.size() + t.occurrence.size() + t.object.size();
    for (const auto &c : t.cells) {
      identity(c.id);
      text(c.text, 16384);
      if (g_utf8_strlen(c.text.data(), static_cast<gssize>(c.text.size())) >
          4096)
        fail("cell_text_limit");
      if (std::find(t.rows.begin(), t.rows.end(), c.row) == t.rows.end() ||
          std::find(t.columns.begin(), t.columns.end(), c.column) ==
              t.columns.end() ||
          !coordinates.emplace(c.row, c.column).second)
        fail("invalid_cell_coordinates");
      string_bytes +=
          c.id.size() + c.row.size() + c.column.size() + c.text.size();
      if (string_bytes > 8 * mib)
        fail("document_string_limit");
    }
    if (active && !expected.insert(t.occurrence).second)
      fail("identity_collision");
  };
  for (const auto &t : s.tables)
    table_check(t, true);
  for (const auto &t : s.archived_tables)
    table_check(t, false);
  if (string_bytes > 8 * mib)
    fail("document_string_limit");
  if (!s.selected_cell.empty()) {
    const auto t =
        std::find_if(s.tables.begin(), s.tables.end(), [&](const auto &t) {
          return t.occurrence == s.selection;
        });
    if (t == s.tables.end() ||
        std::none_of(t->cells.begin(), t->cells.end(),
                     [&](const auto &c) { return c.id == s.selected_cell; }))
      fail("cell_selection_not_found");
  }
  std::size_t total = 0;
  for (const auto &[hash, b] : s.blobs) {
    if (!hashes.contains(hash))
      fail("unreferenced_blob");
    total += b->size();
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
  if (encode_json(encode(s)).size() > 24 * mib)
    fail("snapshot_byte_limit");
}
V figure_value(const Figure &f) {
  return V::Object{{"occurrence", f.occurrence}, {"object", f.object},
                   {"resource", f.resource},     {"sha256", f.hash},
                   {"caption", f.caption},       {"alt", f.alt},
                   {"decorative", f.decorative}, {"width", f.width},
                   {"height", f.height}};
}
Figure decode_figure(const V &f) {
  fields(f, {"occurrence", "object", "resource", "sha256", "caption", "alt",
             "decorative", "width", "height"});
  const auto w = f.at("width").as_integer(), h = f.at("height").as_integer();
  if (w <= 0 || h <= 0 || w > 8192 || h > 8192)
    fail("png_dimensions_limit");
  return {f.at("occurrence").as_string(),
          f.at("object").as_string(),
          f.at("resource").as_string(),
          f.at("sha256").as_string(),
          f.at("caption").as_string(),
          f.at("alt").as_string(),
          f.at("decorative").as_boolean(),
          static_cast<int>(w),
          static_cast<int>(h)};
}
V table_value(const Table &t) {
  V::Array cells;
  for (const auto &c : t.cells)
    cells.emplace_back(V::Object{
        {"id", c.id}, {"row", c.row}, {"column", c.column}, {"text", c.text}});
  return V::Object{{"occurrence", t.occurrence},
                   {"object", t.object},
                   {"title", t.title},
                   {"header", t.header},
                   {"payload_revision", t.payload_revision},
                   {"row_ids", array(t.rows)},
                   {"column_ids", array(t.columns)},
                   {"cells", std::move(cells)}};
}
Table decode_table(const V &v) {
  fields(v, {"occurrence", "object", "title", "header", "payload_revision",
             "row_ids", "column_ids", "cells"});
  Table t;
  t.occurrence = v.at("occurrence").as_string();
  t.object = v.at("object").as_string();
  t.title = v.at("title").as_string();
  t.header = v.at("header").as_boolean();
  t.payload_revision = v.at("payload_revision").as_integer();
  t.rows = strings(v.at("row_ids"), 64);
  t.columns = strings(v.at("column_ids"), 32);
  if (v.at("cells").as_array().size() > 2048)
    fail("invalid_table_shape");
  for (const auto &c : v.at("cells").as_array()) {
    fields(c, {"id", "row", "column", "text"});
    t.cells.push_back({c.at("id").as_string(), c.at("row").as_string(),
                       c.at("column").as_string(), c.at("text").as_string()});
  }
  return t;
}
V encode(const State &s) {
  V::Array figures, rows;
  for (const auto &f : s.figures)
    figures.push_back(figure_value(f));
  for (const auto &r : s.rows)
    rows.emplace_back(V::Object{{"layout", r.layout},
                                {"object", r.object},
                                {"occurrence", r.occurrence},
                                {"children", array(r.children)},
                                {"first", r.first},
                                {"second", r.second},
                                {"gap", r.gap}});
  V::Object result{{"format", s.p2c   ? "INDE-OBJ01-COMPOSITE-P2C-1"
                              : s.p2b ? "INDE-OBJ01-COMPOSITE-P2B-1"
                                      : "INDE-OBJ01-COMPOSITE-P2A-1"},
                   {"project", s.project},
                   {"source", s.source},
                   {"source_sha256", s.source_hash},
                   {"author_order", array(s.author)},
                   {"visual_order", array(s.visual)},
                   {"figures", std::move(figures)},
                   {"rows", std::move(rows)},
                   {"selection", s.selection}};
  if (s.p2b) {
    V::Array archived, tables, retired;
    for (const auto &f : s.archived_figures)
      archived.push_back(figure_value(f));
    for (const auto &t : s.tables)
      tables.push_back(table_value(t));
    for (const auto &t : s.archived_tables)
      retired.push_back(table_value(t));
    result.emplace("archived_figures", std::move(archived));
    result.emplace("tables", std::move(tables));
    result.emplace("archived_tables", std::move(retired));
    result.emplace("selected_cell", s.selected_cell);
  }
  if (s.p2c) {
    result.emplace("working_text", s.working_text);
    result.emplace("context", s.context);
  }
  return result;
}
State decode(const std::string &bytes) {
  text(bytes, 24 * mib);
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
  const auto format = v.at("format").as_string();
  State s;
  s.p2c = format == "INDE-OBJ01-COMPOSITE-P2C-1";
  s.p2b = s.p2c || format == "INDE-OBJ01-COMPOSITE-P2B-1";
  if (!s.p2b && format != "INDE-OBJ01-COMPOSITE-P2A-1")
    fail("unsupported_snapshot_version");
  if (s.p2c)
    fields(v, {"format", "project", "source", "source_sha256", "author_order",
               "visual_order", "figures", "rows", "selection",
               "archived_figures", "tables", "archived_tables", "selected_cell",
               "working_text", "context"});
  else if (s.p2b)
    fields(v,
           {"format", "project", "source", "source_sha256", "author_order",
            "visual_order", "figures", "rows", "selection", "archived_figures",
            "tables", "archived_tables", "selected_cell"});
  else
    fields(v, {"format", "project", "source", "source_sha256", "author_order",
               "visual_order", "figures", "rows", "selection"});
  s.project = v.at("project").as_string();
  s.source = v.at("source").as_string();
  s.source_hash = v.at("source_sha256").as_string();
  s.selection = v.at("selection").as_string();
  if (s.p2c) {
    s.working_text = v.at("working_text").as_string();
    s.context = v.at("context").as_string();
  }
  s.author = strings(v.at("author_order"));
  s.visual = strings(v.at("visual_order"));
  if (v.at("figures").as_array().size() > 256 ||
      v.at("rows").as_array().size() > 128)
    fail("entity_limit");
  for (const auto &f : v.at("figures").as_array())
    s.figures.push_back(decode_figure(f));
  for (const auto &r : v.at("rows").as_array()) {
    fields(r, {"layout", "object", "occurrence", "children", "first", "second",
               "gap"});
    s.rows.push_back({r.at("layout").as_string(), r.at("object").as_string(),
                      r.at("occurrence").as_string(),
                      strings(r.at("children"), 2), real(r.at("first")),
                      real(r.at("second")), real(r.at("gap"))});
  }
  if (s.p2b) {
    if (v.at("archived_figures").as_array().size() > 256 ||
        v.at("tables").as_array().size() > 64 ||
        v.at("archived_tables").as_array().size() > 64)
      fail("entity_limit");
    s.selected_cell = v.at("selected_cell").as_string();
    text(s.selected_cell, 256);
    for (const auto &f : v.at("archived_figures").as_array())
      s.archived_figures.push_back(decode_figure(f));
    for (const auto &t : v.at("tables").as_array())
      s.tables.push_back(decode_table(t));
    for (const auto &t : v.at("archived_tables").as_array())
      s.archived_tables.push_back(decode_table(t));
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
// Undo changes active occurrences, not the existence of immutable resources.
// Retained records are library entries, never hidden reading-order items.
void retain_assets(State &target, const State &current) {
  for (const auto &[hash, bytes] : current.blobs)
    target.blobs.emplace(hash, bytes);
  const auto remember_figure = [&](const Figure &f) {
    const auto same = [&](const auto &a) {
      return a.occurrence == f.occurrence;
    };
    if (std::none_of(target.figures.begin(), target.figures.end(), same) &&
        std::none_of(target.archived_figures.begin(),
                     target.archived_figures.end(), same))
      target.archived_figures.push_back(f);
  };
  for (const auto &f : current.figures)
    remember_figure(f);
  for (const auto &f : current.archived_figures)
    remember_figure(f);
  const auto remember_table = [&](const Table &t) {
    const auto same = [&](const auto &a) {
      return a.occurrence == t.occurrence;
    };
    if (std::none_of(target.tables.begin(), target.tables.end(), same) &&
        std::none_of(target.archived_tables.begin(),
                     target.archived_tables.end(), same))
      target.archived_tables.push_back(t);
  };
  for (const auto &t : current.tables)
    remember_table(t);
  for (const auto &t : current.archived_tables)
    remember_table(t);
}

} // namespace
struct Session::Impl {
  std::filesystem::path workspace;
  Fd lease;
  std::unique_ptr<Db> db;
  State working;
  std::string saved_hash, saved_bytes;
  std::int64_t persisted = 0, revision = 0;
  bool p2b = false, p2c = false, recovered = false, uncertain = false,
       storage_error = false;
  std::int64_t generation = 0;
  std::string journal_hash;
  std::optional<State> uncertain_candidate;
  PersistenceHook hook;
  std::vector<State> undo, redo;
  static constexpr std::size_t history_limit = 32, history_bytes = 32 * mib;

  explicit Impl(std::filesystem::path path, Profile profile,
                const std::filesystem::path &seed, PersistenceHook callback)
      : workspace(
            std::filesystem::absolute(std::move(path)).lexically_normal()),
        p2b(profile != Profile::P2A), p2c(profile == Profile::P2C),
        hook(std::move(callback)) {
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
    if (!seed.empty() && (!p2b || exists))
      fail("seed_requires_new_p2b_workspace");
    if (!exists) {
      if (seed.empty())
        working = fixture();
      else {
        const auto source_path =
            std::filesystem::absolute(seed).lexically_normal();
        for (const auto &part : source_path)
          if (part.extension() == ".inde")
            fail("legacy_project_path_forbidden");
        struct stat src {};
        if (::lstat(source_path.c_str(), &src) != 0 || !S_ISDIR(src.st_mode) ||
            src.st_uid != ::geteuid() || (src.st_mode & 077) != 0)
          fail("unsafe_seed_directory");
        Db original(source_path / "obj01-piloto.sqlite3", true);
        original.exec("BEGIN");
        auto copied = read_saved(original, source_path, p2c ? 2 : 1);
        original.exec("ROLLBACK");
        working = std::move(copied.state);
      }
      working.p2b = p2b;
      working.p2c = p2c;
      if (p2c) {
        working.working_text = working.source;
        working.context = "structure";
      }
      validate(working);
      create_file(workspace / "origem-documento.json", working.source);
      create_file(database, "");
      db = std::make_unique<Db>(database);
      db->exec("PRAGMA synchronous=FULL");
      Tx tx(*db);
      db->exec(
          p2c   ? "PRAGMA application_id=1329744433; PRAGMA user_version=3;"
          : p2b ? "PRAGMA application_id=1329744433; PRAGMA user_version=2;"
                : "PRAGMA application_id=1329744433; PRAGMA user_version=1;");
      db->exec("CREATE TABLE saved(id INTEGER PRIMARY KEY CHECK(id=1), "
               "revision INTEGER NOT NULL CHECK(revision>=0), sha256 TEXT NOT "
               "NULL, snapshot TEXT NOT NULL); CREATE TABLE blobs(sha256 TEXT "
               "PRIMARY KEY, data BLOB NOT NULL);");
      if (p2c)
        db->exec("CREATE TABLE working(id INTEGER PRIMARY KEY CHECK(id=1), "
                 "generation INTEGER NOT NULL, base_revision INTEGER NOT NULL, "
                 "base_sha256 TEXT NOT NULL, sha256 TEXT NOT NULL, snapshot "
                 "TEXT NOT NULL)");
      saved_bytes = encode_json(encode(working));
      saved_hash = sha(saved_bytes);
      for (const auto &[hash, bytes] : working.blobs) {
        Query blob(*db, "INSERT INTO blobs VALUES(?,?)");
        blob.bind(1, hash);
        blob.bind(2, *bytes);
        (void)blob.step();
      }
      Query q(*db, "INSERT INTO saved VALUES(1,0,?,?)");
      q.bind(1, saved_hash);
      q.bind(2, saved_bytes);
      (void)q.step();
      if (p2c) {
        Query w(*db, "INSERT INTO working VALUES(1,0,0,?,?,?)");
        w.bind(1, saved_hash);
        w.bind(2, saved_hash);
        w.bind(3, saved_bytes);
        (void)w.step();
      }
      tx.commit();
      Fd folder;
      folder.n = ::open(workspace.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
      if (folder.n < 0 || ::fsync(folder.n) != 0)
        fail("workspace_sync_failed");
    } else
      db = std::make_unique<Db>(database);
    db->exec("PRAGMA synchronous=FULL");
    auto loaded = load();
    working = std::move(loaded.state);
    saved_bytes = std::move(loaded.bytes);
    saved_hash = std::move(loaded.hash);
    persisted = loaded.rev;
    if (p2c) {
      Tx tx(*db);
      auto current = read_work();
      working = std::move(current.state);
      generation = current.rev;
      journal_hash = std::move(current.hash);
      recovered = dirty();
      tx.commit();
    }
  }
  struct Loaded {
    State state;
    std::string bytes, hash;
    std::int64_t rev;
  };
  static Loaded read_saved(Db &database, const std::filesystem::path &origin,
                           int version, bool work = false) {
    Query app(database, "PRAGMA application_id");
    if (!app.step() || app.integer(0) != application_id)
      fail("not_a_pilot_database");
    Query ver(database, "PRAGMA user_version");
    if (!ver.step() || ver.integer(0) != version)
      fail("unsupported_database_version");
    Query journal(database, "PRAGMA journal_mode");
    if (!journal.step() || journal.string(0) != "delete")
      fail("unsupported_journal_mode");
    Query q(database, work ? "SELECT generation,sha256,length(CAST(snapshot AS "
                             "BLOB)),snapshot FROM working WHERE id=1"
                           : "SELECT revision,sha256,length(CAST(snapshot AS "
                             "BLOB)),snapshot FROM saved WHERE id=1");
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
    if ((s.p2c ? 3 : s.p2b ? 2 : 1) != version)
      fail("profile_snapshot_mismatch");
    if (read_file(origin / "origem-documento.json") != s.source)
      fail("frozen_source_changed");
    Query count(database,
                "SELECT count(*), COALESCE(sum(length(data)),0) FROM blobs");
    if (!count.step() || count.integer(0) > 64 ||
        count.integer(1) > 32 * static_cast<std::int64_t>(mib))
      fail("blob_total_limit");
    Query blob(database, "SELECT sha256, length(data), data FROM blobs");
    while (blob.step()) {
      const auto n = blob.integer(1);
      if (n <= 0 || n > 8 * static_cast<std::int64_t>(mib))
        fail("png_byte_limit");
      auto hash = blob.string(0);
      auto data = blob.bytes(2);
      const auto refers = [&](const auto &assets) {
        return std::any_of(assets.begin(), assets.end(),
                           [&](const auto &f) { return f.hash == hash; });
      };
      if (version != 3 || refers(s.figures) || refers(s.archived_figures))
        s.blobs.emplace(hash, std::make_shared<const Bytes>(std::move(data)));
      else {
        if (sha(data) != hash)
          fail("unclaimed_blob_hash_mismatch");
        (void)png(data);
      }
    }
    validate(s);
    return {std::move(s), std::move(bytes), std::move(hash), rev};
  }
  Loaded load() { return read_saved(*db, workspace, p2c ? 3 : p2b ? 2 : 1); }
  Loaded read_work(std::int64_t base = -1, const std::string &hash = {}) {
    auto loaded = read_saved(*db, workspace, 3, true);
    Query q(*db, "SELECT base_revision,base_sha256 FROM working WHERE id=1");
    if (!q.step() || q.integer(0) != (base < 0 ? persisted : base) ||
        q.string(1) != (base < 0 ? saved_hash : hash))
      fail("working_base_conflict");
    return loaded;
  }
  void point(std::string_view op, PersistenceStage stage) {
    if (hook)
      hook(op, stage);
  }
  template <class Body>
  void transaction(State &candidate, std::string_view op, Body body) {
    Tx tx(*db);
    bool attempted = false;
    try {
      point(op, PersistenceStage::BeforeWrite);
      body();
      point(op, PersistenceStage::BeforeCommit);
      attempted = true;
      tx.commit();
      point(op, PersistenceStage::AfterCommit);
      storage_error = false;
    } catch (...) {
      storage_error = true;
      if (attempted) {
        uncertain = true;
        uncertain_candidate.emplace(std::move(candidate));
        fail("persistence_outcome_uncertain");
      }
      throw;
    }
  }
  void insert_blobs(const State &s) {
    for (const auto &[hash, bytes] : s.blobs) {
      Query q(*db,
              "INSERT INTO blobs VALUES(?,?) ON CONFLICT(sha256) DO NOTHING");
      q.bind(1, hash);
      q.bind(2, *bytes);
      (void)q.step();
    }
  }
  struct Reference {
    std::int64_t base, gen;
    std::string saved, journal;
  };
  Reference reference() const {
    return {persisted, generation, saved_hash, journal_hash};
  }
  void checkpoint(State &candidate, const Reference &expected) {
    validate(candidate);
    if (expected.gen == std::numeric_limits<std::int64_t>::max())
      fail("generation_limit");
    auto bytes = encode_json(encode(candidate));
    auto hash = sha(bytes);
    transaction(candidate, "checkpoint", [&] {
      const auto base = load();
      const auto current = read_work(base.rev, base.hash);
      if (base.rev != expected.base || base.hash != expected.saved)
        fail("persisted_revision_conflict");
      if (current.rev != expected.gen || current.hash != expected.journal)
        fail("working_generation_conflict");
      insert_blobs(candidate);
      point("checkpoint", PersistenceStage::AfterBlobs);
      Query q(*db, "UPDATE working SET "
                   "generation=?,base_revision=?,base_sha256=?,sha256=?,"
                   "snapshot=? WHERE id=1 AND generation=? AND sha256=?");
      q.bind(1, expected.gen + 1);
      q.bind(2, expected.base);
      q.bind(3, expected.saved);
      q.bind(4, hash);
      q.bind(5, bytes);
      q.bind(6, expected.gen);
      q.bind(7, expected.journal);
      (void)q.step();
      if (sqlite3_changes(db->handle()) != 1)
        fail("working_generation_conflict");
      point("checkpoint", PersistenceStage::AfterSnapshot);
      const auto checked = read_work(expected.base, expected.saved);
      if (checked.bytes != bytes || checked.rev != expected.gen + 1)
        fail("checkpoint_readback_mismatch");
    });
    generation = expected.gen + 1;
    journal_hash = std::move(hash);
  }
  void checkpoint(State &candidate) { checkpoint(candidate, reference()); }
  void save_composite() {
    State candidate = working;
    validate(candidate);
    if (persisted == std::numeric_limits<std::int64_t>::max() ||
        generation == std::numeric_limits<std::int64_t>::max())
      fail("revision_limit");
    auto bytes = encode_json(encode(candidate));
    auto hash = sha(bytes);
    transaction(candidate, "save", [&] {
      const auto base = load();
      const auto current = read_work(base.rev, base.hash);
      if (base.rev != persisted || base.hash != saved_hash)
        fail("persisted_revision_conflict");
      if (current.rev != generation || current.hash != journal_hash)
        fail("working_generation_conflict");
      insert_blobs(candidate);
      point("save", PersistenceStage::AfterBlobs);
      Query q(*db, "UPDATE saved SET revision=?,sha256=?,snapshot=? WHERE id=1 "
                   "AND revision=? AND sha256=?");
      q.bind(1, persisted + 1);
      q.bind(2, hash);
      q.bind(3, bytes);
      q.bind(4, persisted);
      q.bind(5, saved_hash);
      (void)q.step();
      if (sqlite3_changes(db->handle()) != 1)
        fail("persisted_revision_conflict");
      Query w(*db, "UPDATE working SET "
                   "generation=?,base_revision=?,base_sha256=?,sha256=?,"
                   "snapshot=? WHERE id=1 AND generation=? AND sha256=?");
      w.bind(1, generation + 1);
      w.bind(2, persisted + 1);
      w.bind(3, hash);
      w.bind(4, hash);
      w.bind(5, bytes);
      w.bind(6, generation);
      w.bind(7, journal_hash);
      (void)w.step();
      if (sqlite3_changes(db->handle()) != 1)
        fail("working_generation_conflict");
      point("save", PersistenceStage::AfterSnapshot);
      const auto saved = load();
      const auto work = read_work(persisted + 1, hash);
      if (saved.bytes != bytes || work.bytes != bytes ||
          work.rev != generation + 1)
        fail("save_readback_mismatch");
    });
    saved_bytes = std::move(bytes);
    saved_hash = hash;
    journal_hash = std::move(hash);
    ++persisted;
    ++generation;
    recovered = false;
    increment();
  }
  void reopen_composite(bool saved, bool discard) {
    if ((dirty() || uncertain) && !discard)
      fail("unsaved_confirmation_required");
    try {
      Loaded base, current;
      {
        Tx tx(*db);
        base = load();
        current = read_work(base.rev, base.hash);
        tx.commit();
      }
      State candidate = saved ? base.state : current.state;
      if (saved)
        checkpoint(candidate, {base.rev, current.rev, base.hash, current.hash});
      else {
        generation = current.rev;
        journal_hash = current.hash;
      }
      working = std::move(candidate);
      persisted = base.rev;
      saved_hash = std::move(base.hash);
      saved_bytes = std::move(base.bytes);
      uncertain = false;
      uncertain_candidate.reset();
      undo.clear();
      redo.clear();
      recovered = !saved && dirty();
      storage_error = false;
      increment();
    } catch (...) {
      storage_error = true;
      throw;
    }
  }
  bool dirty() const { return encode_json(encode(working)) != saved_bytes; }
  V view() const {
    const auto p = projection(working);
    V::Array paragraphs, figures, rows, groups, tables;
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
                    {"bytes", number(working.blobs.at(f.hash)->size())}});
    for (const auto &r : working.rows)
      rows.emplace_back(V::Object{{"id", r.layout},
                                  {"occurrence", r.occurrence},
                                  {"children", array(r.children)},
                                  {"first", r.first},
                                  {"second", r.second},
                                  {"gap", r.gap}});
    for (const auto &g : p->reading_groups())
      groups.emplace_back(array(g.occurrence_ids));
    for (const auto &t : working.tables) {
      auto v = table_value(t).as_object();
      v.emplace("id", t.occurrence);
      tables.emplace_back(std::move(v));
    }
    V::Array text_groups;
    for (const auto &g : p->reading_groups())
      text_groups.emplace_back(V::Object{
          {"id", g.source_block_id}, {"occurrences", array(g.occurrence_ids)}});
    return V::Object{
        {"text_groups", std::move(text_groups)},
        {"profile", p2c   ? "P2-C"
                    : p2b ? "P2-B"
                          : "P2-A"},
        {"tables", std::move(tables)},
        {"selected_cell", working.selected_cell},
        {"can_undo", !undo.empty()},
        {"can_redo", !redo.empty()},
        {"history_entries", number(undo.size() + redo.size())},
        {"retained_figures", number(working.archived_figures.size())},
        {"retained_tables", number(working.archived_tables.size())},
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
        {"unsaved_recovery", p2c},
        {"context", working.context},
        {"recovery_generation", generation},
        {"recovered_on_open", recovered},
        {"persistence_status", uncertain       ? "uncertain"
                               : storage_error ? "error"
                               : p2c           ? "protected"
                                               : "memory_only"},
        {"uncertain_candidate_available", uncertain_candidate.has_value()}};
  }
  static bool fits(const State &current, const std::vector<State> &past,
                   const std::vector<State> &future) {
    if (past.size() + future.size() > history_limit)
      return false;
    std::size_t meta = 0;
    std::map<std::string, std::size_t> resources;
    const auto account = [&](const State &s) {
      meta += encode_json(encode(s)).size();
      for (const auto &[h, b] : s.blobs)
        resources.emplace(h, b->size());
    };
    account(current);
    for (const auto &s : past)
      account(s);
    for (const auto &s : future)
      account(s);
    std::size_t blobs = 0;
    for (const auto &[hash, size] : resources) {
      (void)hash;
      blobs += size;
    }
    return meta <= history_bytes && blobs <= 32 * mib;
  }
  void publish(State candidate, bool remember) {
    validate(candidate);
    if (p2b) {
      auto past = undo, future = redo;
      if (remember &&
          encode_json(encode(candidate)) != encode_json(encode(working))) {
        past.push_back(working);
        future.clear();
      }
      // Keep every advertised history entry valid under the retained library.
      // Media bytes are immutable and were verified on admission/load; only
      // this trusted private-history path can skip decoding them again.
      const auto normalize = [&](std::vector<State> &entries) {
        std::size_t discard = 0;
        for (std::size_t i = 0; i < entries.size(); ++i) {
          retain_assets(entries[i], candidate);
          try {
            validate(entries[i], false);
          } catch (const std::bad_alloc &) {
            throw;
          } catch (const std::exception &) {
            discard = i + 1;
          }
        }
        entries.erase(entries.begin(),
                      entries.begin() + static_cast<std::ptrdiff_t>(discard));
      };
      normalize(past);
      normalize(future);
      while (!fits(candidate, past, future)) {
        if (!past.empty())
          past.erase(past.begin());
        else if (!future.empty())
          future.erase(future.begin());
        else
          fail("history_budget_limit");
      }
      if (p2c)
        checkpoint(candidate);
      working = std::move(candidate);
      undo.swap(past);
      redo.swap(future);
    } else
      working = std::move(candidate);
    increment();
  }
  void replay(bool forward) {
    if (!p2b)
      fail("p2b_profile_required");
    const auto &source = forward ? redo : undo;
    if (source.empty())
      fail(forward ? "redo_empty" : "undo_empty");
    State candidate = source.back();
    retain_assets(candidate, working);
    validate(candidate);
    auto past = undo, future = redo;
    if (forward) {
      future.pop_back();
      past.push_back(working);
    } else {
      past.pop_back();
      future.push_back(working);
    }
    if (!fits(candidate, past, future))
      fail("history_budget_limit");
    if (p2c) {
      candidate.context = working.context;
      checkpoint(candidate);
    }
    working = std::move(candidate);
    undo.swap(past);
    redo.swap(future);
    increment();
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
                         {"data", base64(*working.blobs.at(it->hash))}};
      }
      expected(v);
      if (revision == std::numeric_limits<std::int64_t>::max())
        fail("revision_limit");
      if (p2c && uncertain && op != "reopen" && op != "reopen_working")
        fail("persistence_requires_reopen");
      if (p2c) {
        const bool text_edit = op == "edit_cell";
        const bool structure = op == "create_table" || op == "import" ||
                               op == "duplicate" || op == "move" ||
                               op == "remove" || op == "reorder_text";
        const bool composition =
            op == "arrange" || op == "stack" || op == "restore";
        if ((text_edit && working.context != "text") ||
            (structure && working.context != "structure") ||
            (composition && working.context != "composition"))
          fail("editing_context_required");
      }
      if (op == "save" && p2c) {
        fields(v, {"op", "expected"});
        save_composite();
      } else if ((op == "reopen" || op == "reopen_working") && p2c) {
        fields(v, {"op", "expected", "discard_unsaved"});
        reopen_composite(op == "reopen", v.at("discard_unsaved").as_boolean());
      } else if (op == "export_legacy") {
        fields(v, {"op", "expected"});
        fail("composite_publication_not_supported");
      } else if (op == "undo" || op == "redo") {
        fields(v, {"op", "expected"});
        replay(op == "redo");
      } else if (op == "save") {
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
          insert.bind(2, *b);
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
        undo.clear();
        redo.clear();
        increment();
      } else {
        State candidate = working;
        bool remember = true;
        if (op == "set_context") {
          if (!p2c)
            fail("p2c_profile_required");
          fields(v, {"op", "expected", "context"});
          candidate.context = v.at("context").as_string();
          remember = false;
        } else if (op == "reorder_text") {
          if (!p2c)
            fail("p2c_profile_required");
          fields(v, {"op", "expected", "group", "before"});
          if (candidate.author != candidate.visual)
            fail("authorial_requires_alignment");
          const auto p = projection(candidate);
          const auto id = v.at("group").as_string(),
                     before = v.at("before").as_string();
          const auto group = std::find_if(
              p->reading_groups().begin(), p->reading_groups().end(),
              [&](const auto &g) { return g.source_block_id == id; });
          if (group == p->reading_groups().end())
            fail("reading_group_not_found");
          if (std::find(group->occurrence_ids.begin(),
                        group->occurrence_ids.end(),
                        before) != group->occurrence_ids.end())
            fail("reading_group_target_selected");
          for (const auto &child : group->occurrence_ids)
            unpair(candidate, child);
          for (const auto &r : candidate.rows)
            if (r.children[1] == before)
              fail("destination_inside_row");
          for (const auto &child : group->occurrence_ids)
            std::erase(candidate.author, child);
          const auto target = before.empty()
                                  ? candidate.author.end()
                                  : std::find(candidate.author.begin(),
                                              candidate.author.end(), before);
          if (!before.empty() && target == candidate.author.end())
            fail("destination_not_found");
          const auto at =
              candidate.author.insert(target, group->occurrence_ids.begin(),
                                      group->occurrence_ids.end());
          const auto after =
              at + static_cast<std::ptrdiff_t>(group->occurrence_ids.size());
          std::string owner_before;
          for (auto it = after; it != candidate.author.end(); ++it) {
            const auto b = std::find_if(
                p->paragraphs().begin(), p->paragraphs().end(),
                [&](const auto &b) { return b.occurrence_id == *it; });
            if (b != p->paragraphs().end()) {
              owner_before = b->paragraph_id;
              break;
            }
          }
          application::DocumentEditState old{p->source_snapshot(), {}};
          try {
            candidate.working_text = persistence::encode_document_snapshot(
                application::move_reading_block(old, id, owner_before)
                    .document);
          } catch (const std::bad_alloc &) {
            throw;
          } catch (const std::exception &) {
            fail("owner_text_move_refused");
          }
          candidate.visual = candidate.author;
          candidate.selection = group->occurrence_ids.front();
          candidate.selected_cell.clear();
        } else if (op == "create_table") {
          if (!p2b)
            fail("p2b_profile_required");
          fields(v, {"op", "expected", "before", "title", "header", "cells"});
          if (candidate.author != candidate.visual)
            fail("authorial_requires_alignment");
          if (v.at("cells").as_array().size() != 6)
            fail("initial_table_requires_2x3");
          Table t;
          t.occurrence = project::new_uuid();
          t.object = project::new_uuid();
          t.title = v.at("title").as_string();
          t.header = v.at("header").as_boolean();
          for (int i = 0; i < 2; ++i)
            t.rows.push_back(project::new_uuid());
          for (int i = 0; i < 3; ++i)
            t.columns.push_back(project::new_uuid());
          std::size_t index = 0;
          for (const auto &row : t.rows)
            for (const auto &column : t.columns)
              t.cells.push_back(
                  {project::new_uuid(), row, column,
                   v.at("cells").as_array()[index++].as_string()});
          insert_id(candidate, t.occurrence, v.at("before").as_string());
          candidate.tables.push_back(std::move(t));
          candidate.selected_cell.clear();
        } else if (op == "edit_cell" || op == "select_cell") {
          if (!p2b)
            fail("p2b_profile_required");
          if (op == "edit_cell")
            fields(v, {"op", "expected", "id", "cell", "text"});
          else
            fields(v, {"op", "expected", "id", "cell"});
          const auto id = v.at("id").as_string(),
                     cell = v.at("cell").as_string();
          const auto t =
              std::find_if(candidate.tables.begin(), candidate.tables.end(),
                           [&](const auto &t) { return t.occurrence == id; });
          if (t == candidate.tables.end())
            fail("table_not_found");
          const auto c =
              std::find_if(t->cells.begin(), t->cells.end(),
                           [&](const auto &c) { return c.id == cell; });
          if (c == t->cells.end())
            fail("cell_not_found");
          if (op == "edit_cell" && c->text != v.at("text").as_string()) {
            if (t->payload_revision == std::numeric_limits<std::int64_t>::max())
              fail("revision_limit");
            c->text = v.at("text").as_string();
            ++t->payload_revision;
          } else
            remember = false;
          candidate.selection = id;
          candidate.selected_cell = cell;
        } else if (op == "remove") {
          if (!p2b)
            fail("p2b_profile_required");
          fields(v, {"op", "expected", "id"});
          if (candidate.author != candidate.visual)
            fail("authorial_requires_alignment");
          const auto id = v.at("id").as_string();
          const auto f =
              std::find_if(candidate.figures.begin(), candidate.figures.end(),
                           [&](const auto &f) { return f.occurrence == id; });
          const auto t =
              std::find_if(candidate.tables.begin(), candidate.tables.end(),
                           [&](const auto &t) { return t.occurrence == id; });
          if (f != candidate.figures.end()) {
            candidate.archived_figures.push_back(*f);
            candidate.figures.erase(f);
          } else if (t != candidate.tables.end()) {
            candidate.archived_tables.push_back(*t);
            candidate.tables.erase(t);
          } else
            fail("source_removal_out_of_scope");
          unpair(candidate, id);
          std::erase(candidate.author, id);
          std::erase(candidate.visual, id);
          if (candidate.selection == id) {
            candidate.selection.clear();
            candidate.selected_cell.clear();
          }
        } else if (op == "import") {

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
          candidate.blobs.emplace(
              hash, std::make_shared<const Bytes>(std::move(bytes)));
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
          if (!p2b ||
              std::none_of(candidate.tables.begin(), candidate.tables.end(),
                           [&](const auto &t) { return t.occurrence == id; }))
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
          if (candidate.selection != id)
            candidate.selected_cell.clear();
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
          candidate.selected_cell.clear();
          remember = false;
        } else
          fail("unsupported_operation");
        publish(std::move(candidate), remember);
      }
      return V::Object{{"accepted", true}, {"state", view()}};
    } catch (const std::exception &e) {
      return V::Object{
          {"accepted", false}, {"error", e.what()}, {"state", view()}};
    }
  }
  static void insert_id(State &s, const std::string &id,
                        const std::string &before) {
    const auto at = before.empty()
                        ? s.visual.end()
                        : std::find(s.visual.begin(), s.visual.end(), before);
    if (!before.empty() && at == s.visual.end())
      fail("destination_not_found");
    for (const auto &r : s.rows)
      if (r.children[1] == before)
        fail("destination_inside_row");
    s.visual.insert(at, id);
    s.author = s.visual;
    s.selection = id;
    s.selected_cell.clear();
  }
  static void insert(State &s, const Figure &f, const std::string &before) {
    insert_id(s, f.occurrence, before);
    s.figures.push_back(f);
  }
};
Session::Session(const std::filesystem::path &path, Profile profile,
                 const std::filesystem::path &seed, PersistenceHook hook)
    : impl_(std::make_unique<Impl>(path, profile, seed, std::move(hook))) {}
Session::~Session() = default;
persistence::json::Value Session::request(const persistence::json::Value &v) {
  return impl_->request(v);
}
} // namespace inde::objects::pilot
