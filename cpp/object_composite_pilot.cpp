#include "object_composite_pilot.hpp"
#include "inde/application/document_reading_order.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include "object_commands.hpp"
#include "object_document_adapter.hpp"
#include "object_formula.hpp"
#include "object_formula_cards.hpp"
#include "object_formula_hub.hpp"
#include "object_table_design.hpp"
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
#include <string_view>
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
struct FormulaUse {
  std::string occurrence, object;
};
struct FormulaTextRun {
  std::string text, formula;
};
struct FormulaText {
  std::string object;
  std::vector<FormulaTextRun> runs;
};
struct Presentation {
  double width = 100;
  std::string horizontal = "start", text_align = "left", vertical = "start";
};
struct TableAxes {
  std::vector<std::string> removed_rows, removed_columns;
};
struct State {
  std::string project, source, source_hash, selection;
  std::vector<std::string> author, visual;
  std::vector<Figure> figures, archived_figures;
  std::vector<Table> tables, archived_tables;
  std::string selected_cell;
  bool p2b = false, p2c = false, p2d = false, p2d2 = false;
  std::string working_text, context = "structure";
  std::vector<formula::Record> formulas;
  std::vector<FormulaUse> formula_uses, retired_formula_uses;
  std::map<std::string, FormulaText> formula_text;
  std::map<std::string, std::string> formula_cells;
  std::vector<Row> rows;
  // Versioned private composition overlay; the captured owner document stays
  // complete. Old snapshots keep their exact shape until this overlay is used.
  bool composition_v2 = false;
  bool table_design_v1 = false;
  bool table_structure_v1 = false;
  bool equations_v1 = false;
  bool live_document_v1 = false;
  std::map<std::string, V> equations;
  std::vector<std::string> removed_equations;
  std::map<std::string, TableAxes> table_structure;
  std::map<std::string, V> table_styles;
  std::vector<std::string> removed_text;
  std::map<std::string, Presentation> presentation;
  std::map<std::string, std::shared_ptr<const Bytes>> blobs;
};
const TableAxes &table_axes(const State &s, const Table &t) {
  static const TableAxes empty;
  const auto at = s.table_structure.find(t.occurrence);
  return at == s.table_structure.end() ? empty : at->second;
}
std::vector<std::string> visible_axes(const std::vector<std::string> &all,
                                      const std::vector<std::string> &removed) {
  auto out = all;
  std::erase_if(out, [&](const auto &id) {
    return std::find(removed.begin(), removed.end(), id) != removed.end();
  });
  return out;
}
bool visible_cell(const State &s, const Table &t, const Cell &c) {
  const auto &a = table_axes(s, t);
  return std::find(a.removed_rows.begin(), a.removed_rows.end(), c.row) ==
             a.removed_rows.end() &&
         std::find(a.removed_columns.begin(), a.removed_columns.end(),
                   c.column) == a.removed_columns.end();
}
Table visible_table(const State &s, const Table &t) {
  auto out = t;
  const auto &a = table_axes(s, t);
  out.rows = visible_axes(t.rows, a.removed_rows);
  out.columns = visible_axes(t.columns, a.removed_columns);
  std::erase_if(out.cells,
                [&](const auto &c) { return !visible_cell(s, t, c); });
  return out;
}
std::set<std::string>
removed_paragraphs(const State &s, const compat::DocumentReadProjection &p) {
  std::set<std::string> removed, groups;
  for (const auto &id : s.removed_text) {
    const auto g =
        std::find_if(p.reading_groups().begin(), p.reading_groups().end(),
                     [&](const auto &g) { return g.source_block_id == id; });
    if (g == p.reading_groups().end() || !groups.insert(id).second)
      fail("invalid_removed_text_group");
    removed.insert(g->occurrence_ids.begin(), g->occurrence_ids.end());
  }
  return removed;
}
void sync_formula_text(State &s) {
  if (!s.p2d2)
    return;
  const auto evaluated = formula::evaluate(s.formulas);
  const auto sync = [&](const std::string &key, const std::string &id) {
    if (evaluated.at(id).command) {
      const auto at = s.formula_text.find(key);
      if (at != s.formula_text.end()) {
        if (std::any_of(at->second.runs.begin(), at->second.runs.end(),
                        [&](const auto &r) {
                          return r.formula.empty() ? !r.text.empty()
                                                   : r.formula != id;
                        }))
          fail("formula_command_must_be_alone");
        s.formula_text.erase(at);
      }
    } else if (!s.formula_text.contains(key) ||
               s.formula_text.at(key).object != id)
      s.formula_text[key] = {id, {{{}, id}}};
  };
  for (const auto *uses : {&s.formula_uses, &s.retired_formula_uses})
    for (const auto &u : *uses)
      sync(u.occurrence, u.object);
  for (const auto &[cell, id] : s.formula_cells)
    sync(cell, id);
}
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
  const auto removed = removed_paragraphs(s, source);
  std::erase_if(g.occurrences,
                [&](const auto &o) { return removed.contains(o.id); });
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
  for (const auto &f : s.formulas)
    g.objects.push_back({f.id, ObjectKind::Formula, {}, {}});
  for (const auto &f : s.formula_uses)
    g.occurrences.push_back({f.occurrence, f.object, {}});
  for (const auto &[id, e] : s.equations) {
    g.objects.push_back({e.at("object").as_string(), ObjectKind::Equation, {}, {}});
    if (std::find(s.removed_equations.begin(), s.removed_equations.end(), id) == s.removed_equations.end())
      g.occurrences.push_back({id, e.at("object").as_string(), {}});
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
struct OwnerReadValidation {
  const compat::DocumentReadProjection &current;
  std::string_view original_snapshot, original_document_id;
};
std::unique_ptr<compat::DocumentReadProjection>
validate_owner_reorder(const State &s,
                       const OwnerReadValidation *validated_owner = nullptr) {
  if (!s.p2c)
    return {};
  text(s.working_text, 16 * mib);
  if (s.context != "text" && s.context != "structure" &&
      s.context != "composition")
    fail("invalid_editing_context");
  if (s.working_text == s.source)
    return {};
  if (s.live_document_v1 && validated_owner) {
    if (!s.p2d2)
      fail("live_document_profile_required");
    if (validated_owner->current.source().document_id !=
        validated_owner->original_document_id)
      fail("working_document_identity_changed");
    return {};
  }
  application::DocumentEditState expected{
      persistence::decode_document_snapshot(s.source), {}};
  const auto actual = persistence::decode_document_snapshot(s.working_text);
  if (actual.id != expected.document.id)
    fail("working_document_identity_changed");
  if (s.live_document_v1) {
    if (!s.p2d2)
      fail("live_document_profile_required");
    auto checked = compat::read_document(s.project, actual);
    if (!checked.projection)
      fail("invalid_live_document");
    if (checked.projection->source().snapshot_sha256 != sha(s.working_text))
      fail("invalid_source_capture");
    return std::move(checked.projection);
  }
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
  return {};
}
V encode(const State &s);
void validate(const State &s, bool decode_media = true,
              const OwnerReadValidation *validated_owner = nullptr) {
  if (s.figures.size() + s.archived_figures.size() > 256 ||
      s.tables.size() + s.archived_tables.size() > 64 || s.rows.size() > 128 ||
      s.blobs.size() > 64)
    fail("entity_limit");
  text(s.source, 16 * mib);
  // Only in-memory owner adoption supplies this immutable, already checked
  // capture. Disk readers and all other callers retain the full decoding path.
  if (validated_owner &&
      (!s.live_document_v1 || !s.p2c ||
       validated_owner->original_snapshot != s.source ||
       validated_owner->current.source().project_id != s.project ||
       validated_owner->current.source().snapshot_sha256 != sha(s.working_text)))
    fail("owner_projection_mismatch");
  auto owned_projection = validate_owner_reorder(s, validated_owner);
  text(s.selection, 256);
  if (s.source.size() + s.selection.size() > 8 * mib)
    fail("document_string_limit");
  if (sha(s.source) != s.source_hash)
    fail("source_hash_mismatch");
  // A normal live-source validation just built this exact checked projection.
  // Reuse it rather than decoding/validating the owner a second time.
  if (!validated_owner && !owned_projection)owned_projection = projection(s);
  const auto *p = validated_owner ? &validated_owner->current : owned_projection.get();
  if ((!s.p2d2 && s.table_design_v1) ||
      (s.table_design_v1 && !s.composition_v2) ||
      (!s.table_design_v1 && !s.table_styles.empty()))
    fail("table_design_version_required");
  if ((s.table_structure_v1 && !s.table_design_v1) ||
      (!s.table_structure_v1 && !s.table_structure.empty()))
    fail("table_structure_version_required");
  if ((!s.p2d2 && s.composition_v2) ||
      (!s.composition_v2 &&
       (!s.removed_text.empty() || !s.presentation.empty())))
    fail("composition_version_required");
  const auto removed = removed_paragraphs(s, *p);
  std::set<std::string> expected;
  std::vector<std::string> paragraph_order;
  for (const auto &b : p->paragraphs()) {
    if (removed.contains(b.occurrence_id))
      continue;
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
                             s.source_hash.size() + s.working_text.size() +
                             s.context.size();
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
  nested_ids.insert(removed.begin(), removed.end());
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
  if ((!s.equations_v1 && (!s.equations.empty() || !s.removed_equations.empty())) ||
      (s.equations_v1 && !s.table_structure_v1) || s.equations.size() > 64)
    fail("invalid_equation_version");
  std::set<std::string> hidden_equations;
  for (const auto &id : s.removed_equations)
    if (!s.equations.contains(id) || !hidden_equations.insert(id).second)
      fail("invalid_removed_equation");
  for (const auto &[id, e] : s.equations) {
    fields(e, {"object", "source", "mode", "label", "revision"});
    text(id, 256); text(e.at("object").as_string(), 256);
    text(e.at("source").as_string(), 16384); text(e.at("label").as_string(), 256);
    if (id.empty() || e.at("object").as_string().empty() ||
        !nested_ids.insert(id).second || !nested_ids.insert(e.at("object").as_string()).second ||
        e.at("source").as_string().empty() || e.at("revision").as_integer() < 0 ||
        (e.at("mode").as_string() != "inline" && e.at("mode").as_string() != "block"))
      fail("invalid_equation");
    if (!hidden_equations.contains(id) && !expected.insert(id).second)
      fail("identity_collision");
    string_bytes += id.size() + encode_json(e).size();
  }
  if (s.table_structure.size() > s.tables.size() + s.archived_tables.size())
    fail("invalid_table_structure");
  for (const auto &[id, a] : s.table_structure) {
    const Table *target = nullptr;
    for (const auto *list : {&s.tables, &s.archived_tables})
      for (const auto &t : *list)
        if (t.occurrence == id)
          target = &t;
    if (!target)
      fail("invalid_table_structure");
    const auto check = [](const auto &all, const auto &removed) {
      std::set<std::string> seen;
      for (const auto &axis : removed)
        if (std::find(all.begin(), all.end(), axis) == all.end() ||
            !seen.insert(axis).second)
          fail("invalid_table_structure");
      if (seen.size() >= all.size())
        fail("table_requires_visible_axis");
    };
    check(target->rows, a.removed_rows);
    check(target->columns, a.removed_columns);
    string_bytes += id.size();
    for (const auto *axes : {&a.removed_rows, &a.removed_columns})
      for (const auto &axis : *axes)
        string_bytes += axis.size();
  }
  if (s.table_styles.size() > s.tables.size() + s.archived_tables.size())
    fail("invalid_table_design");
  for (const auto &[id, style] : s.table_styles) {
    const Table *target = nullptr;
    for (const auto *list : {&s.tables, &s.archived_tables})
      for (const auto &t : *list)
        if (t.occurrence == id)
          target = &t;
    if (!target)
      fail("invalid_table_design");
    std::set<std::string> cell_ids;
    for (const auto &c : target->cells)
      cell_ids.insert(c.id);
    table_design::validate(style, target->rows, target->columns, cell_ids);
    string_bytes += id.size() + encode_json(style).size();
  }
  if (!s.p2d && (!s.formulas.empty() || !s.formula_uses.empty() ||
                 !s.formula_cells.empty()))
    fail("p2d_profile_required");
  formula::validate(s.formulas);
  if (s.formula_uses.size() > 256 || s.formula_cells.size() > 2048)
    fail("formula_use_limit");
  std::set<std::string> formula_ids, cell_ids;
  for (const auto &f : s.formulas) {
    if (!nested_ids.insert(f.id).second)
      fail("formula_identity_collision");
    formula_ids.insert(f.id);
    string_bytes += f.id.size() + f.name.size() + f.source.size();
    for (const auto &[q, id] : f.bindings)
      string_bytes += q.size() + id.size();
    for (const auto &[name, value] : f.overrides) {
      (void)value;
      string_bytes += name.size();
    }
  }
  for (const auto &f : s.formula_uses) {
    text(f.occurrence, 256);
    if (f.occurrence.empty() || !formula_ids.contains(f.object) ||
        !nested_ids.insert(f.occurrence).second ||
        !expected.insert(f.occurrence).second)
      fail("formula_occurrence_invalid");
    string_bytes += f.occurrence.size() + f.object.size();
  }
  for (const auto *tables : {&s.tables, &s.archived_tables})
    for (const auto &t : *tables)
      for (const auto &c : t.cells)
        cell_ids.insert(c.id);
  for (const auto &[cell, id] : s.formula_cells) {
    if (!cell_ids.contains(cell) || !formula_ids.contains(id))
      fail("formula_cell_binding_invalid");
    string_bytes += cell.size() + id.size();
  }
  if (!s.p2d2 && (!s.formula_text.empty() || !s.retired_formula_uses.empty()))
    fail("p2d2_profile_required");
  for (const auto &f : s.formulas)
    if (f.advanced != s.p2d2)
      fail("formula_language_mismatch");
  if (s.p2d2) {
    if (s.formula_uses.size() + s.retired_formula_uses.size() > 256 ||
        s.formula_text.size() > 2304)
      fail("formula_use_limit");
    const auto evaluated = formula::evaluate(s.formulas);
    std::map<std::string, std::string> targets;
    std::map<std::string, std::size_t> active_counts;
    for (const auto *uses : {&s.formula_uses, &s.retired_formula_uses})
      for (const auto &u : *uses) {
        if (!formula_ids.contains(u.object))
          fail("formula_occurrence_invalid");
        if (!targets.emplace(u.occurrence, u.object).second)
          fail("formula_occurrence_invalid");
      }
    for (const auto &u : s.formula_uses)
      ++active_counts[u.object];
    for (const auto &u : s.retired_formula_uses) {
      text(u.occurrence, 256);
      if (u.occurrence.empty() || !nested_ids.insert(u.occurrence).second)
        fail("retired_occurrence_collision");
      string_bytes += u.occurrence.size() + u.object.size();
    }
    for (const auto &[cell, id] : s.formula_cells) {
      targets.emplace(cell, id);
      if (std::any_of(s.tables.begin(), s.tables.end(), [&](const auto &t) {
            return std::any_of(t.cells.begin(), t.cells.end(),
                               [&](const auto &c) {
                                 return c.id == cell && visible_cell(s, t, c);
                               });
          }))
        ++active_counts[id];
    }
    for (const auto &[id, result] : evaluated) {
      if (result.error == "formula_command_must_be_alone")
        fail("formula_command_must_be_alone");
      if (result.command && active_counts[id] > 1)
        fail("formula_command_must_be_alone");
      if (result.consumed && active_counts[id] != 0)
        fail("formula_consumed_still_active");
    }
    for (const auto &[key, p] : s.formula_text) {
      text(key, 256);
      if (!formula_ids.contains(p.object) ||
          (targets.contains(key) ? targets.at(key) != p.object
                                 : !cell_ids.contains(key)))
        fail("formula_text_target_invalid");
      if (p.runs.empty() || p.runs.size() > 32 ||
          evaluated.at(p.object).command)
        fail("formula_text_run_limit");
      bool own = false;
      string_bytes += key.size() + p.object.size();
      for (const auto &r : p.runs) {
        if (r.formula.empty()) {
          text(r.text, 16384);
          if (g_utf8_strlen(r.text.data(), static_cast<gssize>(r.text.size())) >
              4096)
            fail("formula_literal_limit");
          string_bytes += r.text.size();
        } else {
          text(r.formula, 256);
          if (!r.text.empty() || !formula_ids.contains(r.formula) ||
              evaluated.at(r.formula).command ||
              evaluated.at(r.formula).consumed)
            fail("formula_text_reference_invalid");
          own = own || r.formula == p.object;
          string_bytes += r.formula.size();
        }
      }
      if (!own)
        fail("formula_text_owner_result_missing");
    }
    for (const auto &[key, id] : targets)
      if (!evaluated.at(id).command && !s.formula_text.contains(key))
        fail("formula_text_missing");
  }
  if (string_bytes > 8 * mib)
    fail("document_string_limit");
  if (!s.selected_cell.empty()) {
    const auto t =
        std::find_if(s.tables.begin(), s.tables.end(), [&](const auto &t) {
          return t.occurrence == s.selection;
        });
    if (t == s.tables.end() ||
        std::none_of(t->cells.begin(), t->cells.end(), [&](const auto &c) {
          return c.id == s.selected_cell && visible_cell(s, *t, c);
        }))
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
    if (removed.contains(group.occurrence_ids.front()))
      continue;
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
      if (!expected.contains(id))
        fail("row_child_not_active");
      if (!paired.insert(id).second)
        fail("row_overlap");
      has_paragraph = has_paragraph || paragraph_ids.contains(id);
      has_figure = has_figure || std::any_of(s.figures.begin(), s.figures.end(),
                                             [&](const auto &f) {
                                               return f.occurrence == id;
                                             });
    }
    if (!s.composition_v2 && (!has_paragraph || !has_figure))
      fail("row_requires_paragraph_and_figure");
  }
  std::set<std::string> known = expected;
  known.insert(removed.begin(), removed.end());
  known.insert(s.removed_equations.begin(), s.removed_equations.end());
  for (const auto &f : s.archived_figures)
    known.insert(f.occurrence);
  for (const auto &t : s.archived_tables)
    known.insert(t.occurrence);
  for (const auto &u : s.retired_formula_uses)
    known.insert(u.occurrence);
  if (s.presentation.size() > known.size())
    fail("presentation_limit");
  for (const auto &[id, style] : s.presentation) {
    if (!known.contains(id) || !std::isfinite(style.width) ||
        style.width < 20 || style.width > 100 ||
        (style.horizontal != "start" && style.horizontal != "center" &&
         style.horizontal != "end") ||
        (style.vertical != "start" && style.vertical != "center" &&
         style.vertical != "end") ||
        (style.text_align != "left" && style.text_align != "center" &&
         style.text_align != "right" && style.text_align != "justify"))
      fail("invalid_presentation");
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
V formula_value(const formula::Record &f) {
  V::Object bindings, overrides;
  for (const auto &[name, id] : f.bindings)
    bindings.emplace(name, id);
  for (const auto &[name, value] : f.overrides)
    overrides.emplace(name, value);
  V::Object out{{"id", f.id},
                {"name", f.name},
                {"source", f.source},
                {"revision", f.revision},
                {"bindings", std::move(bindings)},
                {"overrides", std::move(overrides)}};
  if (f.advanced) {
    out.emplace("language", 2);
    out.emplace("consumed", f.consumed);
  }
  return out;
}
formula::Record decode_formula(const V &v, bool advanced = false) {
  if (advanced)
    fields(v, {"id", "name", "source", "revision", "bindings", "overrides",
               "language", "consumed"});
  else
    fields(v, {"id", "name", "source", "revision", "bindings", "overrides"});
  formula::Record r;
  r.advanced = advanced;
  if (advanced) {
    if (v.at("language").as_integer() != 2)
      fail("formula_language_mismatch");
    r.consumed = v.at("consumed").as_boolean();
  }
  r.id = v.at("id").as_string();
  r.name = v.at("name").as_string();
  r.source = v.at("source").as_string();
  r.revision = v.at("revision").as_integer();
  if (v.at("bindings").as_object().size() > 64 ||
      v.at("overrides").as_object().size() > 32)
    fail("formula_source_limit");
  for (const auto &[name, id] : v.at("bindings").as_object())
    r.bindings.emplace(name, id.as_string());
  for (const auto &[name, value] : v.at("overrides").as_object())
    r.overrides.emplace(name, real(value));
  return r;
}
V runs_value(const std::vector<FormulaTextRun> &runs) {
  V::Array out;
  for (const auto &r : runs)
    out.emplace_back(
        r.formula.empty()
            ? V(V::Object{{"kind", "text"}, {"text", r.text}})
            : V(V::Object{{"kind", "result"}, {"formula", r.formula}}));
  return out;
}
std::vector<FormulaTextRun> decode_runs(const V &v) {
  if (v.as_array().empty() || v.as_array().size() > 32)
    fail("formula_text_run_limit");
  std::vector<FormulaTextRun> runs;
  for (const auto &r : v.as_array()) {
    if (r.at("kind").as_string() == "text") {
      fields(r, {"kind", "text"});
      runs.push_back({r.at("text").as_string(), {}});
    } else if (r.at("kind").as_string() == "result") {
      fields(r, {"kind", "formula"});
      runs.push_back({{}, r.at("formula").as_string()});
    } else
      fail("formula_text_run_kind");
  }
  return runs;
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
  V::Object result{
      {"format", s.live_document_v1 ? "INDE-OBJ01-COMPOSITE-P2D2-6" :
                   s.equations_v1 ? "INDE-OBJ01-COMPOSITE-P2D2-5"
                 : s.table_structure_v1 ? "INDE-OBJ01-COMPOSITE-P2D2-4"
                 : s.table_design_v1  ? "INDE-OBJ01-COMPOSITE-P2D2-3"
                 : s.composition_v2   ? "INDE-OBJ01-COMPOSITE-P2D2-2"
                 : s.p2d2             ? "INDE-OBJ01-COMPOSITE-P2D2-1"
                 : s.p2d              ? "INDE-OBJ01-COMPOSITE-P2D-1"
                 : s.p2c              ? "INDE-OBJ01-COMPOSITE-P2C-1"
                 : s.p2b              ? "INDE-OBJ01-COMPOSITE-P2B-1"
                                      : "INDE-OBJ01-COMPOSITE-P2A-1"},
      {"project", s.project},
      {"source", s.source},
      {"source_sha256", s.source_hash},
      {"author_order", array(s.author)},
      {"visual_order", array(s.visual)},
      {"figures", std::move(figures)},
      {"rows", std::move(rows)},
      {"selection", s.selection}};
  if (s.composition_v2) {
    V::Object styles;
    for (const auto &[id, p] : s.presentation)
      styles.emplace(id, V::Object{{"width", p.width},
                                   {"horizontal", p.horizontal},
                                   {"text_align", p.text_align},
                                   {"vertical", p.vertical}});
    V::Object composition{{"removed_text", array(s.removed_text)},
                          {"presentation", std::move(styles)}};
    if (s.table_design_v1) {
      V::Object tables;
      for (const auto &[id, style] : s.table_styles)
        tables.emplace(id, style);
      composition.emplace("table_styles", std::move(tables));
    }
    if (s.table_structure_v1) {
      V::Object tables;
      for (const auto &[id, a] : s.table_structure)
        tables.emplace(
            id, V::Object{{"removed_rows", array(a.removed_rows)},
                          {"removed_columns", array(a.removed_columns)}});
      composition.emplace("table_structure", std::move(tables));
    }
    if (s.equations_v1) {
      V::Object equations;
      for (const auto &[id, e] : s.equations) equations.emplace(id, e);
      composition.emplace("equations", std::move(equations));
      composition.emplace("removed_equations", array(s.removed_equations));
    }
    result.emplace("composition", std::move(composition));
  }
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
  if (s.p2d) {
    V::Array formulas, uses;
    V::Object cells, texts;
    for (const auto &f : s.formulas)
      formulas.push_back(formula_value(f));
    for (const auto &f : s.formula_uses)
      uses.emplace_back(
          V::Object{{"occurrence", f.occurrence}, {"object", f.object}});
    for (const auto &[cell, id] : s.formula_cells)
      cells.emplace(cell, id);
    result.emplace("formulas", std::move(formulas));
    result.emplace("formula_uses", std::move(uses));
    result.emplace("formula_cells", std::move(cells));
  }
  if (s.p2d2) {
    V::Object texts;
    V::Array retired;
    for (const auto &[key, p] : s.formula_text)
      texts.emplace(
          key, V::Object{{"object", p.object}, {"runs", runs_value(p.runs)}});
    for (const auto &u : s.retired_formula_uses)
      retired.emplace_back(
          V::Object{{"occurrence", u.occurrence}, {"object", u.object}});
    result.emplace("formula_text", std::move(texts));
    result.emplace("retired_formula_uses", std::move(retired));
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
  s.live_document_v1 = format == "INDE-OBJ01-COMPOSITE-P2D2-6";
  s.equations_v1 = s.live_document_v1 || format == "INDE-OBJ01-COMPOSITE-P2D2-5";
  s.table_structure_v1 = s.equations_v1 || format == "INDE-OBJ01-COMPOSITE-P2D2-4";
  s.table_design_v1 =
      s.table_structure_v1 || format == "INDE-OBJ01-COMPOSITE-P2D2-3";
  s.composition_v2 =
      s.table_design_v1 || format == "INDE-OBJ01-COMPOSITE-P2D2-2";
  s.p2d2 = s.composition_v2 || format == "INDE-OBJ01-COMPOSITE-P2D2-1";
  s.p2d = s.p2d2 || format == "INDE-OBJ01-COMPOSITE-P2D-1";
  s.p2c = s.p2d || format == "INDE-OBJ01-COMPOSITE-P2C-1";
  s.p2b = s.p2c || format == "INDE-OBJ01-COMPOSITE-P2B-1";
  if (!s.p2b && format != "INDE-OBJ01-COMPOSITE-P2A-1")
    fail("unsupported_snapshot_version");
  if (s.p2d2) {
    auto body = v.as_object();
    if (s.composition_v2) {
      const auto &c = v.at("composition");
      auto composition = c.as_object();
      if (s.equations_v1) {
        const auto &equations = c.at("equations").as_object();
        if (equations.size() > 64) fail("equation_limit");
        for (const auto &[id, e] : equations) s.equations.emplace(id, e);
        s.removed_equations = strings(c.at("removed_equations"), 64);
        composition.erase("equations"); composition.erase("removed_equations");
      }
      if (s.table_structure_v1) {
        const auto &tables = c.at("table_structure").as_object();
        if (tables.size() > 64)
          fail("invalid_table_structure");
        for (const auto &[id, a] : tables) {
          fields(a, {"removed_rows", "removed_columns"});
          s.table_structure.emplace(
              id, TableAxes{strings(a.at("removed_rows"), 64),
                            strings(a.at("removed_columns"), 32)});
        }
        composition.erase("table_structure");
      }
      if (s.table_design_v1) {
        const auto &styles = c.at("table_styles").as_object();
        if (styles.size() > 64)
          fail("invalid_table_design");
        for (const auto &[id, style] : styles)
          s.table_styles.emplace(id, style);
        composition.erase("table_styles");
      }
      fields(V(composition), {"removed_text", "presentation"});
      s.removed_text = strings(c.at("removed_text"), 10000);
      if (c.at("presentation").as_object().size() > 10512)
        fail("presentation_limit");
      for (const auto &[id, p] : c.at("presentation").as_object()) {
        fields(p, {"width", "horizontal", "text_align", "vertical"});
        s.presentation.emplace(id, Presentation{real(p.at("width")),
                                                p.at("horizontal").as_string(),
                                                p.at("text_align").as_string(),
                                                p.at("vertical").as_string()});
      }
      body.erase("composition");
    }
    fields(V(body), {"format",        "project",
                     "source",        "source_sha256",
                     "author_order",  "visual_order",
                     "figures",       "rows",
                     "selection",     "archived_figures",
                     "tables",        "archived_tables",
                     "selected_cell", "working_text",
                     "context",       "formulas",
                     "formula_uses",  "formula_cells",
                     "formula_text",  "retired_formula_uses"});
  } else if (s.p2d)
    fields(v,
           {"format", "project", "source", "source_sha256", "author_order",
            "visual_order", "figures", "rows", "selection", "archived_figures",
            "tables", "archived_tables", "selected_cell", "working_text",
            "context", "formulas", "formula_uses", "formula_cells"});
  else if (s.p2c)
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
  if (s.p2d) {
    if (v.at("formulas").as_array().size() > 64 ||
        v.at("formula_uses").as_array().size() > 256 ||
        v.at("formula_cells").as_object().size() > 2048)
      fail("formula_use_limit");
    for (const auto &f : v.at("formulas").as_array())
      s.formulas.push_back(decode_formula(f, s.p2d2));
    for (const auto &f : v.at("formula_uses").as_array()) {
      fields(f, {"occurrence", "object"});
      s.formula_uses.push_back(
          {f.at("occurrence").as_string(), f.at("object").as_string()});
    }
    for (const auto &[cell, id] : v.at("formula_cells").as_object())
      s.formula_cells.emplace(cell, id.as_string());
  }
  if (s.p2d2) {
    if (v.at("formula_text").as_object().size() > 2304 ||
        v.at("retired_formula_uses").as_array().size() > 256)
      fail("formula_use_limit");
    for (const auto &[key, p] : v.at("formula_text").as_object()) {
      fields(p, {"object", "runs"});
      s.formula_text.emplace(key, FormulaText{p.at("object").as_string(),
                                              decode_runs(p.at("runs"))});
    }
    for (const auto &u : v.at("retired_formula_uses").as_array()) {
      fields(u, {"occurrence", "object"});
      s.retired_formula_uses.push_back(
          {u.at("occurrence").as_string(), u.at("object").as_string()});
    }
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
void require_active(const State &s, const std::string &id) {
  if (std::find(s.author.begin(), s.author.end(), id) == s.author.end())
    fail("occurrence_not_active");
}
// Move/restore a complete owner reading block, never an inferred paragraph
// slice. Removed groups stay in working_text, including all semantic offsets.
void place_text(State &s, const compat::DocumentReadProjection &p,
                const compat::ReadingGroupBinding &group,
                const std::string &before) {
  if (s.author != s.visual)
    fail("authorial_requires_alignment");
  if (std::find(group.occurrence_ids.begin(), group.occurrence_ids.end(),
                before) != group.occurrence_ids.end())
    fail("reading_group_target_selected");
  for (const auto &child : group.occurrence_ids)
    unpair(s, child);
  for (const auto &r : s.rows)
    if (r.children[1] == before)
      fail("destination_inside_row");
  for (const auto &child : group.occurrence_ids)
    std::erase(s.author, child);
  const auto target = before.empty()
                          ? s.author.end()
                          : std::find(s.author.begin(), s.author.end(), before);
  if (!before.empty() && target == s.author.end())
    fail("destination_not_found");
  const auto at = s.author.insert(target, group.occurrence_ids.begin(),
                                  group.occurrence_ids.end());
  const auto after =
      at + static_cast<std::ptrdiff_t>(group.occurrence_ids.size());
  std::string owner_before;
  for (auto it = after; it != s.author.end(); ++it) {
    const auto b =
        std::find_if(p.paragraphs().begin(), p.paragraphs().end(),
                     [&](const auto &b) { return b.occurrence_id == *it; });
    if (b != p.paragraphs().end()) {
      owner_before = b->paragraph_id;
      break;
    }
  }
  try {
    s.working_text = persistence::encode_document_snapshot(
        application::move_reading_block({p.source_snapshot(), {}},
                                        group.source_block_id, owner_before)
            .document);
  } catch (const std::bad_alloc &) {
    throw;
  } catch (const std::exception &) {
    fail("owner_text_move_refused");
  }
  s.visual = s.author;
  s.selection = group.occurrence_ids.front();
  s.selected_cell.clear();
}
// Structural drag moves a complete row and every owner reading group it
// touches. The two orders are edited independently; a visual arrangement is
// never silently promoted to the authorial order of the whole document.
std::set<std::string> structural_unit(const State &s,
                                    const compat::DocumentReadProjection &p,
                                    const std::string &id) {
  std::set<std::string> ids{id};
  bool changed = true;
  while (changed) {
    const auto count = ids.size();
    const auto extend = [&](const auto &members) {
      if (std::any_of(members.begin(), members.end(),
                      [&](const auto &v) { return ids.contains(v); }))
        ids.insert(members.begin(), members.end());
    };
    for (const auto &group : p.reading_groups()) extend(group.occurrence_ids);
    for (const auto &row : s.rows) extend(row.children);
    changed = ids.size() != count;
  }
  return ids;
}
void move_unit(State &s, const std::string &id, const std::string &reference,
               bool after) {
  require_active(s, id);
  if (!reference.empty()) require_active(s, reference);
  const auto p = projection(s);
  const auto unit = structural_unit(s, *p, id);
  const auto target = reference.empty() ? std::set<std::string>{}
                                      : structural_unit(s, *p, reference);
  if (std::any_of(target.begin(), target.end(),
                  [&](const auto &v) { return unit.contains(v); }))
    fail("reading_group_target_selected");
  const auto relocate = [&](auto &order) {
    std::vector<std::string> moving;
    for (const auto &v : order) if (unit.contains(v)) moving.push_back(v);
    std::erase_if(order, [&](const auto &v) { return unit.contains(v); });
    auto at = order.end();
    if (!reference.empty()) {
      at = std::find_if(order.begin(), order.end(),
                        [&](const auto &v) { return target.contains(v); });
      if (at == order.end()) fail("destination_not_found");
      if (after)
        for (auto it = at; it != order.end(); ++it)
          if (target.contains(*it)) at = std::next(it);
    }
    order.insert(at, moving.begin(), moving.end());
  };
  relocate(s.author);
  relocate(s.visual);
  std::string owner_before;
  bool passed = false;
  for (const auto &v : s.author) {
    if (unit.contains(v)) { passed = true; continue; }
    if (!passed) continue;
    const auto found = std::find_if(p->paragraphs().begin(), p->paragraphs().end(),
                                    [&](const auto &b) { return b.occurrence_id == v; });
    if (found != p->paragraphs().end()) { owner_before = found->paragraph_id; break; }
  }
  auto document = p->source_snapshot();
  try {
    for (const auto &group : p->reading_groups())
      if (unit.contains(group.occurrence_ids.front()))
        document = application::move_reading_block({document, {}},
                    group.source_block_id, owner_before).document;
  } catch (const std::bad_alloc &) { throw; }
  catch (const std::exception &) { fail("owner_text_move_refused"); }
  s.working_text = persistence::encode_document_snapshot(document);
  s.selection = id;
  s.selected_cell.clear();
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
  for (const auto &f : current.formulas)
    if (std::none_of(target.formulas.begin(), target.formulas.end(),
                     [&](const auto &v) { return v.id == f.id; }))
      target.formulas.push_back(f);
  if (current.p2d2) {
    for (const auto *uses :
         {&current.formula_uses, &current.retired_formula_uses})
      for (const auto &u : *uses)
        if (std::none_of(
                target.formula_uses.begin(), target.formula_uses.end(),
                [&](const auto &v) { return v.occurrence == u.occurrence; }) &&
            std::none_of(target.retired_formula_uses.begin(),
                         target.retired_formula_uses.end(), [&](const auto &v) {
                           return v.occurrence == u.occurrence;
                         }))
          target.retired_formula_uses.push_back(u);
    // A prior table snapshot may predate a newly inserted cell. Its phrase
    // belongs to the future/redo state, not to an unknown past endpoint.
    std::set<std::string> endpoints;
    for (const auto *uses :
         {&target.formula_uses, &target.retired_formula_uses})
      for (const auto &u : *uses)
        endpoints.insert(u.occurrence);
    for (const auto *tables : {&target.tables, &target.archived_tables})
      for (const auto &t : *tables)
        for (const auto &c : t.cells)
          endpoints.insert(c.id);
    for (const auto &[key, p] : current.formula_text)
      if (endpoints.contains(key))
        target.formula_text.try_emplace(key, p);
  }
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
  bool p2b = false, p2c = false, p2d = false, p2d2 = false, recovered = false,
       uncertain = false, storage_error = false;
  std::int64_t generation = 0;
  std::string journal_hash;
  std::optional<State> uncertain_candidate;
  PersistenceHook hook;
  std::vector<State> undo, redo;
  static constexpr std::size_t history_limit = 32, history_bytes = 32 * mib;

  bool native_document = false;
  explicit Impl(std::filesystem::path path, Profile profile,
                const std::filesystem::path &seed, PersistenceHook callback,
                const project::Document *owner = nullptr,
                const std::string &owner_project = {})
      : workspace(
            std::filesystem::absolute(std::move(path)).lexically_normal()),
        p2b(profile != Profile::P2A),
        p2c(profile == Profile::P2C || profile == Profile::P2D ||
            profile == Profile::P2D2),
        p2d(profile == Profile::P2D || profile == Profile::P2D2),
        p2d2(profile == Profile::P2D2), hook(std::move(callback)) {
    native_document = owner != nullptr;
    for (const auto &part : workspace)
      if (!native_document && part.extension() == ".inde")
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
      if (owner) {
        working.project = owner_project;
        working.source = persistence::encode_document_snapshot(*owner);
        working.source_hash = sha(working.source);
        const auto p = projection(working);
        for (const auto &b : p->paragraphs()) working.author.push_back(b.occurrence_id);
        working.visual = working.author;
      } else if (seed.empty())
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
        auto copied = read_saved(original, source_path,
                                 p2d2  ? 4
                                 : p2d ? 3
                                 : p2c ? 2
                                       : 1);
        original.exec("ROLLBACK");
        working = std::move(copied.state);
      }
      working.p2b = p2b;
      working.p2c = p2c;
      working.p2d = p2d;
      working.p2d2 = p2d2;
      if (p2d2) {
        for (auto &f : working.formulas)
          formula::rich::materialize_legacy(f);
        const auto evaluated = formula::evaluate(working.formulas);
        for (const auto &u : working.formula_uses)
          if (!evaluated.at(u.object).command)
            working.formula_text.emplace(
                u.occurrence, FormulaText{u.object, {{{}, u.object}}});
        for (const auto &[cell, id] : working.formula_cells)
          if (!evaluated.at(id).command)
            working.formula_text.emplace(cell, FormulaText{id, {{{}, id}}});
      }
      if (p2c) {
        if (working.working_text.empty())
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
          p2d2  ? "PRAGMA application_id=1329744433; PRAGMA user_version=5;"
          : p2d ? "PRAGMA application_id=1329744433; PRAGMA user_version=4;"
          : p2c ? "PRAGMA application_id=1329744433; PRAGMA user_version=3;"
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
    if ((s.p2d2 ? 5 : s.p2d ? 4 : s.p2c ? 3 : s.p2b ? 2 : 1) != version)
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
      if (version < 3 || refers(s.figures) || refers(s.archived_figures))
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
  Loaded load() {
    return read_saved(*db, workspace,
                      p2d2  ? 5
                      : p2d ? 4
                      : p2c ? 3
                      : p2b ? 2
                            : 1);
  }
  Loaded read_work(std::int64_t base = -1, const std::string &hash = {}) {
    auto loaded = read_saved(*db, workspace, p2d2 ? 5 : p2d ? 4 : 3, true);
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
    const auto removed = removed_paragraphs(working, *p);
    V::Array paragraphs, figures, rows, groups, tables;
    for (const auto &b : p->paragraphs())
      if (!removed.contains(b.occurrence_id))
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
      if (!removed.contains(g.occurrence_ids.front()))
        groups.emplace_back(array(g.occurrence_ids));
    for (const auto &t : working.tables) {
      const auto shown = visible_table(working, t);
      auto v = table_value(shown).as_object();
      v.emplace("id", t.occurrence);
      if (p2d2) {
        std::set<std::string> cells;
        for (const auto &c : shown.cells)
          cells.insert(c.id);
        v.emplace(
            "appearance",
            table_design::project(working.table_styles.contains(t.occurrence)
                                      ? working.table_styles.at(t.occurrence)
                                      : table_design::defaults(),
                                  t.columns, shown.columns, shown.rows, cells));
        V::Array removed_rows, removed_columns;
        const auto &axes = table_axes(working, t);
        const auto describe = [&](const auto &all, const auto &removed,
                                  bool row, V::Array &out) {
          for (std::size_t i = 0; i < all.size(); ++i)
            if (std::find(removed.begin(), removed.end(), all[i]) !=
                removed.end()) {
              std::string label;
              std::size_t filled = 0, formulas = 0;
              for (const auto &c : t.cells)
                if ((row ? c.row : c.column) == all[i]) {
                  if (!c.text.empty()) {
                    ++filled;
                    if (label.empty())
                      label = c.text.substr(0, 120);
                  }
                  if (working.formula_cells.contains(c.id))
                    ++formulas;
                }
              // A preview ends on a complete UTF-8 scalar, never in a byte
              // sequence.
              while (!label.empty() &&
                     !g_utf8_validate(label.data(),
                                      static_cast<gssize>(label.size()),
                                      nullptr))
                label.pop_back();
              out.emplace_back(V::Object{{"id", all[i]},
                                         {"index", number(i)},
                                         {"label", label},
                                         {"filled_cells", number(filled)},
                                         {"formula_cells", number(formulas)}});
            }
        };
        describe(t.rows, axes.removed_rows, true, removed_rows);
        describe(t.columns, axes.removed_columns, false, removed_columns);
        v.emplace("structure",
                  V::Object{{"retained_rows", number(t.rows.size())},
                            {"retained_columns", number(t.columns.size())},
                            {"removed_rows", std::move(removed_rows)},
                            {"removed_columns", std::move(removed_columns)}});
      }
      tables.emplace_back(std::move(v));
    }
    V::Array text_groups;
    for (const auto &g : p->reading_groups())
      if (!removed.contains(g.occurrence_ids.front()))
        text_groups.emplace_back(
            V::Object{{"id", g.source_block_id},
                      {"occurrences", array(g.occurrence_ids)}});
    V::Object styles;
    for (const auto &[id, s] : working.presentation)
      styles.emplace(id, V::Object{{"width", s.width},
                                   {"horizontal", s.horizontal},
                                   {"text_align", s.text_align},
                                   {"vertical", s.vertical}});
    V::Array equations;
    for (const auto &[id, e] : working.equations)
      if (std::find(working.removed_equations.begin(), working.removed_equations.end(), id) == working.removed_equations.end()) {
        auto v = e.as_object(); v.emplace("id", id); equations.emplace_back(v);
      }
    V::Array removed_items;
    for (const auto &g : p->reading_groups())
      if (removed.contains(g.occurrence_ids.front())) {
        std::string label;
        for (const auto &id : g.occurrence_ids) {
          if (!label.empty())
            label += " / ";
          label += *p->paragraph_text(id);
        }
        removed_items.emplace_back(
            V::Object{{"id", g.source_block_id},
                      {"kind", "text"},
                      {"label", label},
                      {"count", number(g.occurrence_ids.size())}});
      }
    for (const auto &f : working.archived_figures)
      removed_items.emplace_back(V::Object{{"id", f.occurrence},
                                           {"kind", "image"},
                                           {"label", f.caption},
                                           {"count", 1}});
    for (const auto &t : working.archived_tables)
      removed_items.emplace_back(V::Object{{"id", t.occurrence},
                                           {"kind", "table"},
                                           {"label", t.title},
                                           {"count", 1}});
    for (const auto &id : working.removed_equations)
      removed_items.emplace_back(V::Object{{"id", id}, {"kind", "equation"}, {"label", working.equations.at(id).at("label")}, {"count", 1}});
    for (const auto &u : working.retired_formula_uses) {
      const auto f =
          std::find_if(working.formulas.begin(), working.formulas.end(),
                       [&](const auto &f) { return f.id == u.object; });
      if (f != working.formulas.end() && !f->consumed)
        removed_items.emplace_back(V::Object{{"id", u.occurrence},
                                             {"kind", "formula"},
                                             {"label", f->name},
                                             {"count", 1}});
    }
    V::Array formulas, uses;
    V::Object cells, texts;
    if (p2d) {
      const auto evaluated = formula::evaluate(working.formulas);
      for (const auto &f : working.formulas) {
        const auto &r = evaluated.at(f.id);
        auto value = formula_value(f).as_object();
        V::Array variables;
        for (const auto &var : r.variables) {
          const auto typed = [](const std::string &kind,
                                const std::string &display,
                                double number) -> V {
            if (kind == "text")
              return display;
            if (kind == "boolean")
              return display == "verdadeiro";
            if (kind == "number")
              return number;
            return V{};
          };
          variables.emplace_back(V::Object{
              {"name", var.name},
              {"initial", p2d2 ? typed(var.initial_type, var.initial_display,
                                       var.initial.value_or(0))
                          : var.initial ? V(*var.initial)
                                        : V{}},
              {"current",
               p2d2 ? typed(var.current_type, var.current_display, var.current)
                    : V(var.current)},
              {"overridden", var.overridden},
              {"initial_display", var.initial_display},
              {"current_display", var.current_display}});
        }
        value.emplace("valid", r.valid);
        if (p2d2) {
          value.emplace("command", r.command);
          value.emplace("consumed", r.consumed);
          value.emplace("type", r.type);
          const auto cards = formula::cards::recognize(f.source);
          value.emplace("cards", cards ? *cards : V{});
          const auto model = formula::hub::recognize(f.source);
          value.emplace("hub", model ? *model
                                     : V(V::Object{{"kind", "free"},
                                                   {"source", f.source}}));
        }
        value.emplace("display", r.valid ? r.display : "");
        value.emplace("error", r.error);
        value.emplace("offset", number(r.offset));
        value.emplace("actions", number(r.actions));
        value.emplace("variables", std::move(variables));
        formulas.emplace_back(std::move(value));
      }
      for (const auto &f : working.formula_uses)
        uses.emplace_back(
            V::Object{{"id", f.occurrence}, {"object", f.object}});
      for (const auto &[cell, id] : working.formula_cells)
        cells.emplace(cell, id);
    }
    if (p2d2)
      for (const auto &[key, p] : working.formula_text)
        texts.emplace(
            key, V::Object{{"object", p.object}, {"runs", runs_value(p.runs)}});
    return V::Object{
        {"formula_text", std::move(texts)},
        {"formula_cards_supported", p2d2},
        {"formula_hub_supported", p2d2},
        {"object_composition_supported", p2d2},
        {"table_design_supported", p2d2},
        {"table_structure_supported", p2d2},
        {"table_design_defaults", p2d2 ? table_design::defaults() : V{}},
        {"table_design_presets",
         p2d2 ? table_design::presets() : V(V::Array{})},
        {"native_document", native_document},
        {"document_title", p->source_snapshot().title},
        {"equations", std::move(equations)},
        {"equations_supported", p2d2},
        {"composition_version", working.live_document_v1 ? 6 : working.equations_v1 ? 5
                                : working.table_structure_v1 ? 4
                                : working.table_design_v1  ? 3
                                : working.composition_v2   ? 2
                                                           : 1},
        {"presentation", std::move(styles)},
        {"removed_items", std::move(removed_items)},
        {"formulas", std::move(formulas)},
        {"formula_uses", std::move(uses)},
        {"formula_cells", std::move(cells)},
        {"text_groups", std::move(text_groups)},
        {"profile", p2d2  ? "P2-D2"
                    : p2d ? "P2-D"
                    : p2c ? "P2-C"
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
    sync_formula_text(candidate);
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
  V preview_formula(const std::string &source, const std::string &id,
                    const std::string &name = "") {
    auto records = working.formulas;
    auto at = std::find_if(records.begin(), records.end(),
                           [&](const auto &f) { return f.id == id; });
    if (!id.empty() && at == records.end())
      fail("formula_not_found");
    if (at == records.end()) {
      formula::Record f;
      f.id = project::new_uuid();
      f.name = name.empty() ? "__FormulaPreview0" : name;
      std::size_t index = 0;
      while (name.empty() &&
             std::any_of(records.begin(), records.end(),
                         [&](const auto &r) { return r.name == f.name; }))
        f.name = "__FormulaPreview" + std::to_string(++index);
      f.advanced = true;
      records.push_back(f);
      at = std::prev(records.end());
    }
    if (!name.empty() && at->name != name)
      fail("formula_symbol_immutable");
    if (at->consumed)
      fail("formula_command_consumed");
    at->source = source;
    const auto preview_id = at->id;
    formula::bind(*at, records);
    const auto before = formula::evaluate(records);
    const auto &r = before.at(preview_id);
    V::Array changes, preview_variables;
    for (const auto &v : r.variables)
      preview_variables.emplace_back(V::Object{{"name", v.name},
                                               {"type", v.current_type},
                                               {"display", v.current_display}});
    if (r.valid && r.command) {
      const auto after =
          formula::evaluate(formula::execute(records, preview_id));
      for (const auto &f : records)
        if (f.id != preview_id)
          for (const auto &v : after.at(f.id).variables) {
            const auto &vars = before.at(f.id).variables;
            const auto old =
                std::find_if(vars.begin(), vars.end(),
                             [&](const auto &x) { return x.name == v.name; });
            if (old != vars.end() &&
                (old->current_display != v.current_display ||
                 old->current_type != v.current_type ||
                 (v.current_type == "number" && old->current != v.current)))
              changes.emplace_back(V::Object{{"formula", f.name},
                                             {"variable", v.name},
                                             {"before", old->current_display},
                                             {"after", v.current_display}});
          }
    }
    return V::Object{{"accepted", true},
                     {"preview", V::Object{{"valid", r.valid},
                                           {"display", r.display},
                                           {"error", r.error},
                                           {"command", r.command},
                                           {"changes", changes},
                                           {"variables", preview_variables}}}};
  }
  V request(const V &input) {
    try {
      auto v = input;
      auto op = v.at("op").as_string();
      std::optional<std::string> author_before;
      if (v.as_object().contains("author_before")) {
        if (!native_document || !working.live_document_v1 ||
            (op != "import" && op != "duplicate" && op != "create_table" &&
             op != "create_table_sized" && op != "create_equation" &&
             op != "create_formula" && op != "create_formula_hub" &&
             op != "create_formula_cards" && op != "attach_formula" &&
             op != "restore_removed")) fail("unsupported_insertion_intent");
        author_before = v.at("author_before").as_string();
        auto object = v.as_object();
        object.erase("author_before");
        v = V(std::move(object));
      }
      V authoring_model;
      bool visual_cards = false;
      const bool from_hub = op == "create_formula_hub" ||
                            op == "edit_formula_hub" ||
                            op == "preview_formula_hub";
      if (from_hub || op == "create_formula_cards" ||
          op == "edit_formula_cards" || op == "preview_formula_cards") {
        if (!p2d2)
          fail("p2d2_profile_required");
        expected(v);
        const bool create =
            op == "create_formula_hub" || op == "create_formula_cards";
        const bool preview =
            op == "preview_formula_hub" || op == "preview_formula_cards";
        const auto key = from_hub ? "model" : "cards";
        if (create)
          fields(v, {"op", "expected", "name", key, "before", "table", "cell",
                     "prefix", "suffix"});
        else if (preview) {
          if (from_hub && v.as_object().contains("name"))
            fields(v, {"op", "expected", "id", key, "name"});
          else
            fields(v, {"op", "expected", "id", key});
        } else
          fields(v,
                 {"op", "expected", "id", key, "target", "prefix", "suffix"});
        authoring_model =
            from_hub
                ? v.at(key)
                : V(V::Object{{"kind", "condition"}, {"cards", v.at(key)}});
        const auto source = formula::hub::compile(authoring_model);
        if (preview)
          return preview_formula(source, v.at("id").as_string(),
                                 from_hub && v.as_object().contains("name")
                                     ? v.at("name").as_string()
                                     : "");
        visual_cards = authoring_model.at("kind").as_string() != "free";
        auto converted = v.as_object();
        converted.erase(key);
        converted.emplace("source", source);
        if (create) {
          op = "create_formula";
          converted.at("op") = op;
        }
        v = V(std::move(converted));
      }
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
        const bool text_edit =
            op == "edit_cell" || op == "edit_formula" ||
            op == "execute_formula" || op == "edit_formula_text" ||
            op == "edit_formula_cards" || op == "edit_formula_hub" ||
            op == "reset_formula_variable" || op == "edit_equation";
        const bool structure =
            op == "create_equation" || op == "create_table" || op == "create_table_sized" ||
            op == "insert_table_axis" || op == "remove_table_axis" ||
            op == "restore_table_axis" || op == "import" || op == "duplicate" ||
            op == "move" || op == "move_unit" || op == "place_beside" ||
            op == "remove" || op == "restore_removed" ||
            op == "reorder_text" || op == "create_formula" ||
            op == "attach_formula" || op == "detach_formula";
        const bool composition =
            op == "arrange" || op == "arrange_objects" ||
            op == "edit_table_design" || op == "set_presentation" ||
            op == "reset_presentation" || op == "stack" || op == "restore";
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
          const auto p = projection(candidate);
          const auto id = v.at("group").as_string();
          const auto group = std::find_if(
              p->reading_groups().begin(), p->reading_groups().end(),
              [&](const auto &g) { return g.source_block_id == id; });
          if (group == p->reading_groups().end() ||
              std::find(candidate.removed_text.begin(),
                        candidate.removed_text.end(),
                        id) != candidate.removed_text.end())
            fail("reading_group_not_found");
          place_text(candidate, *p, *group, v.at("before").as_string());
        } else if (op == "edit_formula_text") {
          if (!p2d2)
            fail("p2d2_profile_required");
          fields(v, {"op", "expected", "target", "runs"});
          const auto target = v.at("target").as_string();
          const auto use = std::find_if(
              candidate.formula_uses.begin(), candidate.formula_uses.end(),
              [&](const auto &u) { return u.occurrence == target; });
          const auto cell = candidate.formula_cells.find(target);
          bool active_cell = false;
          for (const auto &t : candidate.tables)
            for (const auto &c : t.cells)
              if (c.id == target && visible_cell(candidate, t, c)) {
                active_cell = true;
                candidate.selection = t.occurrence;
                candidate.selected_cell = c.id;
              }
          if (use == candidate.formula_uses.end() &&
              (cell == candidate.formula_cells.end() || !active_cell))
            fail("formula_text_target_invalid");
          const auto id =
              use != candidate.formula_uses.end() ? use->object : cell->second;
          if (use != candidate.formula_uses.end()) {
            candidate.selection = target;
            candidate.selected_cell.clear();
          }
          candidate.formula_text[target] = {id, decode_runs(v.at("runs"))};
        } else if (op == "create_formula" || op == "attach_formula" ||
                   op == "detach_formula" || op == "edit_formula" ||
                   op == "edit_formula_cards" || op == "edit_formula_hub" ||
                   op == "execute_formula" || op == "reset_formula_variable") {
          if (!p2d)
            fail("p2d_profile_required");
          const auto find_formula = [&](const std::string &id) {
            const auto at = std::find_if(
                candidate.formulas.begin(), candidate.formulas.end(),
                [&](const auto &f) { return f.id == id; });
            if (at == candidate.formulas.end())
              fail("formula_not_found");
            return at;
          };
          const auto bump = [](auto &revision) {
            if (revision == std::numeric_limits<std::int64_t>::max())
              fail("revision_limit");
            ++revision;
          };
          if (op == "create_formula" || op == "attach_formula" ||
              op == "detach_formula") {
            if (op == "create_formula" && p2d2)
              fields(v, {"op", "expected", "name", "source", "before", "table",
                         "cell", "prefix", "suffix"});
            else if (op == "create_formula")
              fields(v, {"op", "expected", "name", "source", "before", "table",
                         "cell"});
            else if (op == "attach_formula")
              fields(v, {"op", "expected", "id", "before", "table", "cell"});
            else
              fields(v, {"op", "expected", "table", "cell"});
            std::string object;
            if (op == "create_formula") {
              formula::Record f;
              f.id = project::new_uuid();
              f.name = v.at("name").as_string();
              f.source = v.at("source").as_string();
              f.advanced = p2d2;
              object = f.id;
              candidate.formulas.push_back(std::move(f));
              for (auto &r : candidate.formulas)
                formula::bind(r, candidate.formulas);
              if (visual_cards) {
                const auto result =
                    formula::evaluate(candidate.formulas).at(object);
                if (!result.valid)
                  throw std::runtime_error(result.error);
              }
            } else if (op == "attach_formula")
              object = find_formula(v.at("id").as_string())->id;
            std::string presentation_target;
            const auto table = v.at("table").as_string(),
                       cell = v.at("cell").as_string();
            if (author_before && (!table.empty() || !cell.empty()))
              fail("unsupported_insertion_intent");
            if (table.empty() && cell.empty() && op != "detach_formula") {
              if (!(native_document && candidate.live_document_v1) && candidate.author != candidate.visual)
                fail("authorial_requires_alignment");
              const auto occurrence = project::new_uuid();
              insert_id(candidate, occurrence, v.at("before").as_string(), author_before);
              candidate.formula_uses.push_back({occurrence, object});
              presentation_target = occurrence;
            } else {
              if (op != "detach_formula" && !v.at("before").as_string().empty())
                fail("formula_cell_destination");
              const auto t = std::find_if(
                  candidate.tables.begin(), candidate.tables.end(),
                  [&](const auto &t) { return t.occurrence == table; });
              if (t == candidate.tables.end())
                fail("table_not_found");
              if (std::none_of(
                      t->cells.begin(), t->cells.end(), [&](const auto &c) {
                        return c.id == cell && visible_cell(candidate, *t, c);
                      }))
                fail("cell_not_found");
              if (op == "detach_formula") {
                if (candidate.formula_cells.erase(cell) != 1)
                  fail("formula_cell_missing");
              } else if (!candidate.formula_cells.emplace(cell, object).second)
                fail("formula_cell_already_bound");
              bump(t->payload_revision);
              candidate.selection = table;
              candidate.selected_cell = cell;
              presentation_target = cell;
            }
            if (p2d2 && op == "create_formula") {
              const auto result =
                  formula::evaluate(candidate.formulas).at(object);
              const auto prefix = v.at("prefix").as_string(),
                         suffix = v.at("suffix").as_string();
              if (result.command) {
                if (!prefix.empty() || !suffix.empty())
                  fail("formula_command_must_be_alone");
              } else
                candidate.formula_text[presentation_target] = {
                    object, {{prefix, {}}, {{}, object}, {suffix, {}}}};
            }
          } else {
            const auto f = find_formula(v.at("id").as_string());
            if (p2d2 && f->consumed)
              fail("formula_command_consumed");
            if (op == "edit_formula" || op == "edit_formula_cards" ||
                op == "edit_formula_hub") {
              if (op == "edit_formula_cards" || op == "edit_formula_hub") {
                fields(v, {"op", "expected", "id", "source", "target", "prefix",
                           "suffix"});
                // A visual edit may only replace a fully representable source.
                if (op == "edit_formula_cards" &&
                    !formula::cards::recognize(f->source))
                  fail("formula_cards_advanced_source");
                formula::hub::preserve_symbols(f->source, authoring_model);
                const auto target = v.at("target").as_string();
                if (!target.empty()) {
                  bool active = std::any_of(
                      candidate.formula_uses.begin(),
                      candidate.formula_uses.end(), [&](const auto &u) {
                        return u.occurrence == target && u.object == f->id;
                      });
                  for (const auto &t : candidate.tables)
                    for (const auto &c : t.cells)
                      if (c.id == target && visible_cell(candidate, t, c) &&
                          candidate.formula_cells.contains(target) &&
                          candidate.formula_cells.at(target) == f->id)
                        active = true;
                  if (!active)
                    fail("formula_text_target_invalid");
                  // Refuse to flatten a richer phrase containing other results.
                  const bool command =
                      formula::evaluate(candidate.formulas).at(f->id).command;
                  if (command) {
                    if (!v.at("prefix").as_string().empty() ||
                        !v.at("suffix").as_string().empty())
                      fail("formula_command_must_be_alone");
                  }
                  const auto runs =
                      command ? std::vector<FormulaTextRun>{}
                              : candidate.formula_text.at(target).runs;
                  std::size_t own = 0;
                  for (const auto &r : runs) {
                    if (!r.formula.empty()) {
                      if (r.formula != f->id || ++own != 1)
                        fail("formula_cards_complex_phrase");
                    }
                  }
                  if (!command)
                    candidate.formula_text[target] = {
                        f->id,
                        {{v.at("prefix").as_string(), {}},
                         {{}, f->id},
                         {v.at("suffix").as_string(), {}}}};
                } else if (!v.at("prefix").as_string().empty() ||
                           !v.at("suffix").as_string().empty())
                  fail("formula_text_target_invalid");
              } else
                fields(v, {"op", "expected", "id", "source"});
              f->source = v.at("source").as_string();
              bump(f->revision);
              for (auto &r : candidate.formulas)
                formula::bind(r, candidate.formulas);
              if (visual_cards) {
                const auto result =
                    formula::evaluate(candidate.formulas).at(f->id);
                if (!result.valid)
                  throw std::runtime_error(result.error);
              }
            } else if (op == "execute_formula") {
              fields(v, {"op", "expected", "id"});
              const auto command_id = f->id;
              candidate.formulas =
                  formula::execute(candidate.formulas, command_id);
              if (p2d2) {
                std::vector<std::string> removed;
                for (const auto &u : candidate.formula_uses)
                  if (u.object == command_id) {
                    removed.push_back(u.occurrence);
                    candidate.retired_formula_uses.push_back(u);
                  }
                for (const auto &id : removed) {
                  unpair(candidate, id);
                  std::erase(candidate.author, id);
                  std::erase(candidate.visual, id);
                  if (candidate.selection == id) {
                    candidate.selection.clear();
                    candidate.selected_cell.clear();
                  }
                }
                std::erase_if(candidate.formula_uses, [&](const auto &u) {
                  return u.object == command_id;
                });
                std::erase_if(candidate.formula_cells, [&](const auto &v) {
                  return v.second == command_id;
                });
              }
            } else {
              fields(v, {"op", "expected", "id", "variable"});
              if (p2d2)
                formula::rich::reset(*f, v.at("variable").as_string());
              else if (f->overrides.erase(v.at("variable").as_string()) != 1)
                fail("formula_override_missing");
              if (!p2d2)
                bump(f->revision);
            }
          }
        } else if (op == "create_equation" || op == "edit_equation") {
          if (!p2d2) fail("p2d2_profile_required");
          if (op == "create_equation") fields(v, {"op", "expected", "source", "mode", "label", "before"});
          else fields(v, {"op", "expected", "id", "source", "mode", "label"});
          candidate.composition_v2 = candidate.table_design_v1 = candidate.table_structure_v1 = candidate.equations_v1 = true;
          std::string id;
          std::int64_t revision = 0;
          std::string object;
          if (op == "create_equation") {
            if (!(native_document && candidate.live_document_v1) && candidate.author != candidate.visual) fail("authorial_requires_alignment");
            id = project::new_uuid(); object = project::new_uuid();
            insert_id(candidate, id, v.at("before").as_string(), author_before);
          } else {
            id = v.at("id").as_string(); require_active(candidate, id);
            const auto e = candidate.equations.find(id);
            if (e == candidate.equations.end()) fail("equation_not_found");
            object = e->second.at("object").as_string(); revision = e->second.at("revision").as_integer();
            if (revision == std::numeric_limits<std::int64_t>::max()) fail("revision_limit");
            ++revision;
          }
          candidate.equations[id] = V::Object{{"object", object}, {"source", v.at("source")}, {"mode", v.at("mode")}, {"label", v.at("label")}, {"revision", revision}};
          candidate.selection = id; candidate.selected_cell.clear();
        } else if (op == "create_table" || op == "create_table_sized") {
          if (!p2b)
            fail("p2b_profile_required");
          const bool sized = op == "create_table_sized";
          if (sized && !p2d2)
            fail("p2d2_profile_required");
          if (sized)
            fields(v, {"op", "expected", "before", "title", "header", "rows",
                       "columns"});
          else
            fields(v, {"op", "expected", "before", "title", "header", "cells"});
          if (!(native_document && candidate.live_document_v1) && candidate.author != candidate.visual)
            fail("authorial_requires_alignment");
          if (!sized && v.at("cells").as_array().size() != 6)
            fail("initial_table_requires_2x3");
          const auto rows = sized ? v.at("rows").as_integer() : 2,
                     columns = sized ? v.at("columns").as_integer() : 3;
          if (rows < 1 || rows > 64 || columns < 1 || columns > 32)
            fail("invalid_table_dimensions");
          if (sized)
            candidate.composition_v2 = candidate.table_design_v1 =
                candidate.table_structure_v1 = true;
          Table t;
          t.occurrence = project::new_uuid();
          t.object = project::new_uuid();
          t.title = v.at("title").as_string();
          t.header = v.at("header").as_boolean();
          for (int i = 0; i < rows; ++i)
            t.rows.push_back(project::new_uuid());
          for (int i = 0; i < columns; ++i)
            t.columns.push_back(project::new_uuid());
          std::size_t index = 0;
          for (const auto &row : t.rows)
            for (const auto &column : t.columns)
              t.cells.push_back(
                  {project::new_uuid(), row, column,
                   sized ? std::string{}
                         : v.at("cells").as_array()[index++].as_string()});
          insert_id(candidate, t.occurrence, v.at("before").as_string(), author_before);
          candidate.tables.push_back(std::move(t));
          candidate.selected_cell.clear();
        } else if (op == "insert_table_axis" || op == "remove_table_axis" ||
                   op == "restore_table_axis") {
          if (!p2d2)
            fail("p2d2_profile_required");
          if (op == "insert_table_axis")
            fields(v, {"op", "expected", "id", "axis", "before"});
          else
            fields(v, {"op", "expected", "id", "axis", "axis_id"});
          const auto id = v.at("id").as_string(),
                     axis = v.at("axis").as_string();
          if (axis != "row" && axis != "column")
            fail("invalid_table_axis");
          const auto t =
              std::find_if(candidate.tables.begin(), candidate.tables.end(),
                           [&](const auto &t) { return t.occurrence == id; });
          if (t == candidate.tables.end())
            fail("table_not_found");
          if (t->payload_revision == std::numeric_limits<std::int64_t>::max())
            fail("revision_limit");
          auto &a = candidate.table_structure[id];
          auto &all = axis == "row" ? t->rows : t->columns;
          auto &removed = axis == "row" ? a.removed_rows : a.removed_columns;
          if (op == "insert_table_axis") {
            if (all.size() >= (axis == "row" ? 64U : 32U))
              fail("table_retained_axis_limit");
            const auto before = v.at("before").as_string();
            const auto at = before.empty()
                                ? all.end()
                                : std::find(all.begin(), all.end(), before);
            if (!before.empty() &&
                (at == all.end() || std::find(removed.begin(), removed.end(),
                                              before) != removed.end()))
              fail("table_axis_not_active");
            const auto index = static_cast<std::size_t>(at - all.begin());
            const auto fresh = project::new_uuid();
            all.insert(at, fresh);
            if (axis == "row")
              for (const auto &column : t->columns)
                t->cells.push_back({project::new_uuid(), fresh, column, {}});
            else {
              for (const auto &row : t->rows)
                t->cells.push_back({project::new_uuid(), row, fresh, {}});
              if (candidate.table_styles.contains(id))
                candidate.table_styles[id] = table_design::with_inserted_column(
                    candidate.table_styles.at(id), index);
            }
          } else {
            const auto chosen = v.at("axis_id").as_string();
            const auto at = std::find(removed.begin(), removed.end(), chosen);
            if (std::find(all.begin(), all.end(), chosen) == all.end())
              fail("table_axis_not_found");
            if (op == "remove_table_axis") {
              if (at != removed.end())
                fail("table_axis_not_active");
              if (all.size() - removed.size() <= 1)
                fail("table_requires_visible_axis");
              removed.push_back(chosen);
            } else {
              if (at == removed.end())
                fail("table_axis_not_retired");
              removed.erase(at);
            }
          }
          ++t->payload_revision;
          candidate.composition_v2 = candidate.table_design_v1 =
              candidate.table_structure_v1 = true;
          candidate.selection = id;
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
          const auto c = std::find_if(
              t->cells.begin(), t->cells.end(), [&](const auto &c) {
                return c.id == cell && visible_cell(candidate, *t, c);
              });
          if (c == t->cells.end())
            fail("cell_not_found");
          if (op == "edit_cell" && candidate.formula_cells.contains(cell))
            fail("formula_cell_requires_editor");
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
          if (!p2d2 && candidate.author != candidate.visual)
            fail("authorial_requires_alignment");
          const auto id = v.at("id").as_string();
          const auto f =
              std::find_if(candidate.figures.begin(), candidate.figures.end(),
                           [&](const auto &f) { return f.occurrence == id; });
          const auto t =
              std::find_if(candidate.tables.begin(), candidate.tables.end(),
                           [&](const auto &t) { return t.occurrence == id; });
          if (candidate.equations.contains(id)) {
            require_active(candidate, id); candidate.removed_equations.push_back(id);
          } else if (f != candidate.figures.end()) {
            candidate.archived_figures.push_back(*f);
            candidate.figures.erase(f);
          } else if (t != candidate.tables.end()) {
            candidate.archived_tables.push_back(*t);
            candidate.tables.erase(t);
          } else if (const auto use = std::find_if(
                         candidate.formula_uses.begin(),
                         candidate.formula_uses.end(),
                         [&](const auto &u) { return u.occurrence == id; });
                     use != candidate.formula_uses.end()) {
            if (p2d2)
              candidate.retired_formula_uses.push_back(*use);
            candidate.formula_uses.erase(use);
          } else {
            if (!p2d2)
              fail("source_removal_out_of_scope");
            const auto p = projection(candidate);
            const auto g =
                std::find_if(p->reading_groups().begin(),
                             p->reading_groups().end(), [&](const auto &g) {
                               return std::find(g.occurrence_ids.begin(),
                                                g.occurrence_ids.end(),
                                                id) != g.occurrence_ids.end();
                             });
            if (g == p->reading_groups().end())
              fail("occurrence_not_active");
            require_active(candidate, id);
            candidate.composition_v2 = true;
            candidate.removed_text.push_back(g->source_block_id);
            for (const auto &child : g->occurrence_ids) {
              unpair(candidate, child);
              std::erase(candidate.author, child);
              std::erase(candidate.visual, child);
              if (candidate.selection == child)
                candidate.selection.clear();
            }
            candidate.selected_cell.clear();
          }
          unpair(candidate, id);
          std::erase(candidate.author, id);
          std::erase(candidate.visual, id);
          if (candidate.selection == id) {
            candidate.selection.clear();
            candidate.selected_cell.clear();
          }
        } else if (op == "restore_removed") {
          if (!p2d2)
            fail("p2d2_profile_required");
          fields(v, {"op", "expected", "id", "before"});
          if (!(native_document && candidate.live_document_v1) && candidate.author != candidate.visual)
            fail("authorial_requires_alignment");
          const auto id = v.at("id").as_string(),
                     before = v.at("before").as_string();
          candidate.composition_v2 = true;
          const auto hidden = std::find(candidate.removed_text.begin(),
                                        candidate.removed_text.end(), id);
          const auto f =
              std::find_if(candidate.archived_figures.begin(),
                           candidate.archived_figures.end(),
                           [&](const auto &f) { return f.occurrence == id; });
          const auto t =
              std::find_if(candidate.archived_tables.begin(),
                           candidate.archived_tables.end(),
                           [&](const auto &t) { return t.occurrence == id; });
          const auto u =
              std::find_if(candidate.retired_formula_uses.begin(),
                           candidate.retired_formula_uses.end(),
                           [&](const auto &u) { return u.occurrence == id; });
          if (hidden != candidate.removed_text.end()) {
            if (author_before) fail("unsupported_insertion_intent");
            const auto p = projection(candidate);
            const auto g = std::find_if(
                p->reading_groups().begin(), p->reading_groups().end(),
                [&](const auto &g) { return g.source_block_id == id; });
            candidate.removed_text.erase(hidden);
            place_text(candidate, *p, *g, before);
          } else {
            const auto equation = std::find(candidate.removed_equations.begin(), candidate.removed_equations.end(), id);
            if (equation != candidate.removed_equations.end()) {
              candidate.removed_equations.erase(equation);
            } else if (f != candidate.archived_figures.end()) {
              candidate.figures.push_back(*f);
              candidate.archived_figures.erase(f);
            } else if (t != candidate.archived_tables.end()) {
              candidate.tables.push_back(*t);
              candidate.archived_tables.erase(t);
            } else if (u != candidate.retired_formula_uses.end()) {
              const auto formula = std::find_if(
                  candidate.formulas.begin(), candidate.formulas.end(),
                  [&](const auto &f) { return f.id == u->object; });
              if (formula == candidate.formulas.end() || formula->consumed)
                fail("formula_command_consumed");
              candidate.formula_uses.push_back(*u);
              candidate.retired_formula_uses.erase(u);
            } else
              fail("removed_item_not_found");
            insert_id(candidate, id, before, author_before);
          }
        } else if (op == "import") {

          fields(v, {"op", "expected", "data", "before", "caption", "alt",
                     "decorative"});
          if (!(native_document && candidate.live_document_v1) && candidate.author != candidate.visual)
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
          insert(candidate, f, v.at("before").as_string(), author_before);
          candidate.blobs.emplace(
              hash, std::make_shared<const Bytes>(std::move(bytes)));
        } else if (op == "duplicate") {
          fields(v, {"op", "expected", "id", "before"});
          if (!(native_document && candidate.live_document_v1) && candidate.author != candidate.visual)
            fail("authorial_requires_alignment");
          auto f = figure(candidate, v.at("id").as_string());
          f.occurrence = project::new_uuid();
          insert(candidate, f, v.at("before").as_string(), author_before);
        } else if (op == "move_unit") {
          if (!p2d2) fail("p2d2_profile_required");
          fields(v, {"op", "expected", "id", "reference", "after"});
          move_unit(candidate, v.at("id").as_string(),
                    v.at("reference").as_string(), v.at("after").as_boolean());
        } else if (op == "move") {
          fields(v, {"op", "expected", "id", "before", "intent"});
          const auto id = v.at("id").as_string(),
                     before = v.at("before").as_string(),
                     intent = v.at("intent").as_string();
          const auto source = projection(candidate);
          const auto paragraph = std::find_if(
              source->paragraphs().begin(), source->paragraphs().end(),
              [&](const auto &p) { return p.occurrence_id == id; });
          if (p2d2 && paragraph != source->paragraphs().end()) {
            require_active(candidate, id);
            if (intent == "authorial") {
              const auto g = std::find_if(
                  source->reading_groups().begin(),
                  source->reading_groups().end(), [&](const auto &g) {
                    return std::find(g.occurrence_ids.begin(),
                                     g.occurrence_ids.end(),
                                     id) != g.occurrence_ids.end();
                  });
              place_text(candidate, *source, *g, before);
            }
          }
          const bool owner_move = p2d2 &&
                                  paragraph != source->paragraphs().end() &&
                                  intent == "authorial";
          if (!owner_move) {
            if (std::none_of(
                    candidate.formula_uses.begin(),
                    candidate.formula_uses.end(),
                    [&](const auto &u) { return u.occurrence == id; }) &&
                !candidate.equations.contains(id) &&
                !(p2d2 && paragraph != source->paragraphs().end()) &&
                (!p2b ||
                 std::none_of(
                     candidate.tables.begin(), candidate.tables.end(),
                     [&](const auto &t) { return t.occurrence == id; })))
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
            for (const auto &child :
                 editor.current().layouts.front().children) {
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
          }
        } else if (op == "arrange_objects" || op == "place_beside") {
          if (!p2d2)
            fail("p2d2_profile_required");
          if (op == "place_beside") fields(v, {"op", "expected", "id", "other", "side"});
          else fields(v, {"op", "expected", "id", "other", "side", "first", "second", "gap"});
          const auto id = v.at("id").as_string(),
                     other = v.at("other").as_string(),
                     side = v.at("side").as_string();
          require_active(candidate, id);
          require_active(candidate, other);
          if (id == other || (side != "left" && side != "right"))
            fail("invalid_row_target");
          std::optional<Row> existing_row;
          if (op == "place_beside") {
            const auto p = projection(candidate);
            for (const auto &group : p->reading_groups())
              if (group.occurrence_ids.size() != 1 &&
                  (std::find(group.occurrence_ids.begin(), group.occurrence_ids.end(), id) != group.occurrence_ids.end() ||
                   std::find(group.occurrence_ids.begin(), group.occurrence_ids.end(), other) != group.occurrence_ids.end()))
                fail("reading_group_split");
            for (const auto &row : candidate.rows) {
              if (std::find(row.children.begin(), row.children.end(), other) != row.children.end() &&
                  std::find(row.children.begin(), row.children.end(), id) == row.children.end())
                fail("row_target_occupied");
              if (std::find(row.children.begin(), row.children.end(), other) != row.children.end() &&
                  std::find(row.children.begin(), row.children.end(), id) != row.children.end())
                existing_row = row;
            }
          }
          candidate.composition_v2 = true;
          unpair(candidate, id);
          unpair(candidate, other);
          std::erase(candidate.visual, id);
          const auto at = std::find(candidate.visual.begin(),
                                    candidate.visual.end(), other);
          candidate.visual.insert(side == "left" ? at : std::next(at), id);
          const auto children = side == "left" ? std::vector<std::string>{id, other}
                                               : std::vector<std::string>{other, id};
          if (existing_row) {
            existing_row->children = children;
            candidate.rows.push_back(*existing_row);
          } else candidate.rows.push_back(
              {project::new_uuid(), project::new_uuid(), project::new_uuid(), children,
               op == "place_beside" ? .5 : real(v.at("first")),
               op == "place_beside" ? .5 : real(v.at("second")),
               op == "place_beside" ? 16 : real(v.at("gap"))});
          candidate.selection = id;
          candidate.selected_cell.clear();
        } else if (op == "edit_table_design") {
          if (!p2d2)
            fail("p2d2_profile_required");
          fields(v, {"op", "expected", "id", "title", "header", "appearance",
                     "placement"});
          const auto id = v.at("id").as_string();
          const auto t =
              std::find_if(candidate.tables.begin(), candidate.tables.end(),
                           [&](const auto &t) { return t.occurrence == id; });
          if (t == candidate.tables.end())
            fail("table_not_found");
          const auto &placement = v.at("placement");
          fields(placement, {"width", "horizontal", "text_align", "vertical"});
          const auto title = v.at("title").as_string();
          const auto header = v.at("header").as_boolean();
          if (t->title != title || t->header != header) {
            if (t->payload_revision == std::numeric_limits<std::int64_t>::max())
              fail("revision_limit");
            ++t->payload_revision;
            t->title = title;
            t->header = header;
          }
          candidate.composition_v2 = candidate.table_design_v1 = true;
          const auto shown = visible_table(candidate, *t);
          std::set<std::string> cells;
          for (const auto &c : shown.cells)
            cells.insert(c.id);
          table_design::validate(v.at("appearance"), shown.rows, shown.columns,
                                 cells);
          const auto old = candidate.table_styles.contains(id)
                               ? candidate.table_styles.at(id)
                               : table_design::defaults();
          candidate.table_styles[id] =
              table_design::merge_visible(old, v.at("appearance"), t->columns,
                                          shown.columns, shown.rows, cells);
          candidate.presentation[id] = {real(placement.at("width")),
                                        placement.at("horizontal").as_string(),
                                        placement.at("text_align").as_string(),
                                        placement.at("vertical").as_string()};
          candidate.selection = id;
          candidate.selected_cell.clear();
        } else if (op == "set_presentation" || op == "reset_presentation") {
          if (!p2d2)
            fail("p2d2_profile_required");
          const auto id = v.at("id").as_string();
          require_active(candidate, id);
          candidate.composition_v2 = true;
          if (op == "set_presentation") {
            fields(v, {"op", "expected", "id", "width", "horizontal",
                       "text_align", "vertical"});
            candidate.presentation[id] = {
                real(v.at("width")), v.at("horizontal").as_string(),
                v.at("text_align").as_string(), v.at("vertical").as_string()};
          } else {
            fields(v, {"op", "expected", "id"});
            candidate.presentation.erase(id);
          }
          candidate.selection = id;
          candidate.selected_cell.clear();
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
          if (p2d2)
            require_active(candidate, v.at("id").as_string());
          else
            (void)figure(candidate, v.at("id").as_string());
          unpair(candidate, v.at("id").as_string());
          candidate.selection = v.at("id").as_string();
          candidate.selected_cell.clear();
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
                        const std::string &before,
                        const std::optional<std::string> &author_before = {}) {
    const auto at = before.empty()
                        ? s.visual.end()
                        : std::find(s.visual.begin(), s.visual.end(), before);
    if (!before.empty() && at == s.visual.end())
      fail("destination_not_found");
    for (const auto &r : s.rows)
      if (r.children[1] == before)
        fail("destination_inside_row");
    const auto author_target = author_before.value_or(before);
    const auto author_at = author_target.empty() ? s.author.end()
        : std::find(s.author.begin(), s.author.end(), author_target);
    if (!author_target.empty() && author_at == s.author.end())
      fail("author_destination_not_found");
    // Each order keeps its own existing sequence. Inserting an object never
    // promotes the visual arrangement to the reading order of the document.
    s.author.insert(author_at, id);
    s.visual.insert(at, id);
    s.selection = id;
    s.selected_cell.clear();
  }
  static void insert(State &s, const Figure &f, const std::string &before,
                     const std::optional<std::string> &author_before = {}) {
    insert_id(s, f.occurrence, before, author_before);
    s.figures.push_back(f);
  }
};
Session::Session(const std::filesystem::path &path, Profile profile,
                 const std::filesystem::path &seed, PersistenceHook hook)
    : impl_(std::make_unique<Impl>(path, profile, seed, std::move(hook))) {}
bool Session::plain_text_compatible(const std::filesystem::path &workspace,
    const std::string &project_id, const project::Document &document) noexcept {
  try {
    Db database(workspace / "obj01-piloto.sqlite3", true);
    database.exec("PRAGMA query_only=ON; BEGIN");
    const auto saved = Impl::read_saved(database, workspace, 5);
    const auto working = Impl::read_saved(database, workspace, 5, true);
    Query base(database, "SELECT base_revision,base_sha256 FROM working WHERE id=1");
    if (!base.step() || base.integer(0) != saved.rev || base.string(1) != saved.hash) return false;
    Query assets(database, "SELECT count(*) FROM blobs");
    if (!assets.step() || assets.integer(0) != 0) return false;
    const auto plain = [&](const State &s) {
      if (s.project != project_id || !s.p2d2 || s.author != s.visual ||
          !s.figures.empty() || !s.archived_figures.empty() || !s.tables.empty() ||
          !s.archived_tables.empty() || !s.rows.empty() || !s.formulas.empty() ||
          !s.formula_uses.empty() || !s.retired_formula_uses.empty() ||
          !s.formula_text.empty() || !s.formula_cells.empty() || !s.equations.empty() ||
          !s.removed_equations.empty() || !s.removed_text.empty() ||
          !s.presentation.empty() || !s.table_styles.empty() || !s.table_structure.empty()) return false;
      const auto p = projection(s);
      std::vector<std::string> expected;
      for (const auto &row : p->paragraphs()) expected.push_back(row.occurrence_id);
      return expected == s.author && project::same_document_state(p->source_snapshot(), document, false);
    };
    return plain(saved.state) && plain(working.state);
  } catch (...) { return false; }
}
Session::Session(const std::filesystem::path &path,
                 const std::string &project_id,
                 const project::Document &document, DocumentBinding binding)
    : impl_(std::make_unique<Impl>(path, Profile::P2D2, std::filesystem::path{},
                                   PersistenceHook{}, &document, project_id)) {
  if (binding == DocumentBinding::Live)
    synchronize_document(document);
  else if (impl_->working.source !=
           persistence::encode_document_snapshot(document))
    fail("native_document_source_changed");
}
project::Document Session::current_document() const {
  return persistence::decode_document_snapshot(impl_->working.working_text);
}
void Session::synchronize_document(const project::Document &document) {
  if (!impl_->native_document)
    fail("native_document_required");
  if (impl_->uncertain)
    fail("persistence_outcome_uncertain");
  const auto bytes = persistence::encode_document_snapshot(document);
  const auto original =
      persistence::decode_document_snapshot(impl_->working.source);
  if (document.id != original.id)
    fail("working_document_identity_changed");
  if (impl_->working.live_document_v1 && impl_->working.working_text == bytes)
    return;
  // A successful owner commit can change timestamps only. Keep the prior
  // reading orders in object history so a native text move stays reversible.
  if (impl_->working.live_document_v1 &&
      project::same_document_state(current_document(), document, false)) {
    auto candidate = impl_->working;
    candidate.working_text = bytes;
    if (!impl_->fits(candidate, impl_->undo, impl_->redo))
      fail("history_budget_limit");
    if (impl_->revision == std::numeric_limits<std::int64_t>::max())
      fail("revision_limit");
    impl_->checkpoint(candidate);
    impl_->working = std::move(candidate);
    impl_->increment();
    return;
  }
  const auto checked_owner = compat::read_document(impl_->working.project, document);
  if (!checked_owner.projection ||
      checked_owner.projection->source().snapshot_sha256 != sha(bytes))
    fail("invalid_live_document");
  const auto &after = checked_owner.projection;
  const OwnerReadValidation owner_validation{*after,impl_->working.source,original.id};
  std::set<std::string> new_ids, group_ids;
  std::map<std::string, std::string> group_for;
  for (const auto &b : after->paragraphs())
    new_ids.insert(b.occurrence_id);
  for (const auto &g : after->reading_groups()) {
    group_ids.insert(g.source_block_id);
    for (const auto &id : g.occurrence_ids)
      group_for[id] = g.source_block_id;
  }
  // Styles usually retain many states with the exact same owner. Reuse one
  // prior capture inside this operation only; a different snapshot/project
  // immediately invalidates it, including histories of authorial moves.
  std::string prior_bytes, prior_project;
  std::unique_ptr<compat::DocumentReadProjection> prior_projection;
  const auto rebase = [&](State &s) {
    if (!prior_projection || prior_bytes != s.working_text || prior_project != s.project) {
      prior_projection = projection(s);
      prior_bytes = s.working_text;
      prior_project = s.project;
    }
    const auto &before = prior_projection;
    const bool aligned = s.author == s.visual;
    s.live_document_v1 = true;
    s.composition_v2 = s.table_design_v1 = s.table_structure_v1 =
        s.equations_v1 = true;
    s.working_text = bytes;
    std::set<std::string> old_ids;
    for (const auto &b : before->paragraphs())
      old_ids.insert(b.occurrence_id);
    std::erase_if(s.removed_text,
                  [&](const auto &id) { return !group_ids.contains(id); });
    const auto hidden = removed_paragraphs(s, *after);
    const auto stale = [&](const auto &id) {
      return old_ids.contains(id) &&
             (!new_ids.contains(id) || hidden.contains(id));
    };
    std::map<std::string, std::vector<std::string>> anchored;
    for (std::size_t i = 0; i < s.author.size(); ++i) {
      const auto &id = s.author[i];
      if (old_ids.contains(id))
        continue;
      std::string anchor;
      for (std::size_t j = i + 1; j < s.author.size(); ++j)
        if (new_ids.contains(s.author[j]) && !hidden.contains(s.author[j])) {
          anchor = group_for.at(s.author[j]);
          break;
        }
      anchored[anchor].push_back(id);
    }
    s.author.clear();
    for (const auto &g : after->reading_groups()) {
      if (hidden.contains(g.occurrence_ids.front()))
        continue;
      const auto &items = anchored[g.source_block_id];
      s.author.insert(s.author.end(), items.begin(), items.end());
      s.author.insert(s.author.end(), g.occurrence_ids.begin(),
                      g.occurrence_ids.end());
    }
    s.author.insert(s.author.end(), anchored[""].begin(), anchored[""].end());
    if (aligned)
      s.visual = s.author;
    else {
      std::erase_if(s.visual, stale);
      for (std::size_t i = 0; i < s.author.size(); ++i) {
        const auto &id = s.author[i];
        if (std::find(s.visual.begin(), s.visual.end(), id) != s.visual.end())
          continue;
        auto at = s.visual.end();
        for (std::size_t j = i + 1; j < s.author.size(); ++j) {
          at = std::find(s.visual.begin(), s.visual.end(), s.author[j]);
          if (at != s.visual.end())
            break;
        }
        s.visual.insert(at, id);
      }
    }
    std::erase_if(s.presentation, [&](const auto &entry) {
      return old_ids.contains(entry.first) && !new_ids.contains(entry.first);
    });
    std::erase_if(s.rows, [&](const auto &row) {
      const auto at =
          std::find(s.visual.begin(), s.visual.end(), row.children.front());
      return at == s.visual.end() || std::next(at) == s.visual.end() ||
             *std::next(at) != row.children.back();
    });
    if (stale(s.selection)) {
      s.selection.clear();
      s.selected_cell.clear();
    }
    validate(s, false, &owner_validation);
  };
  auto candidate = impl_->working;
  auto past = impl_->undo, future = impl_->redo;
  rebase(candidate);
  for (auto &s : past)
    rebase(s);
  for (auto &s : future)
    rebase(s);
  if (!impl_->fits(candidate, past, future))
    fail("history_budget_limit");
  if (impl_->revision == std::numeric_limits<std::int64_t>::max())
    fail("revision_limit");
  impl_->checkpoint(candidate);
  impl_->working = std::move(candidate);
  impl_->undo.swap(past);
  impl_->redo.swap(future);
  impl_->increment();
}
Session::~Session() = default;
persistence::json::Value Session::request(const persistence::json::Value &v) {
  return impl_->request(v);
}
} // namespace inde::objects::pilot
