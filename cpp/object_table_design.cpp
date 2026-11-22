#include "object_table_design.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string_view>
namespace inde::objects::pilot::table_design {
namespace {
using V = Value;
[[noreturn]] void fail() { throw std::runtime_error("invalid_table_design"); }
void fields(const V &v, std::initializer_list<const char *> names) {
  if (v.as_object().size() != names.size())
    fail();
  for (const auto *n : names)
    (void)v.at(n);
}
double real(const V &v) {
  if (const auto *d = std::get_if<double>(&v.storage()))
    return *d;
  return static_cast<double>(v.as_integer());
}
void range(const V &v, double lo, double hi) {
  const auto d = real(v);
  if (!std::isfinite(d) || d < lo || d > hi)
    fail();
}
void one_of(const V &v, std::initializer_list<std::string_view> values) {
  const auto &s = v.as_string();
  if (std::none_of(values.begin(), values.end(),
                   [&](const auto &a) { return s == a; }))
    fail();
}
void color(const V &v) {
  const auto &s = v.as_string();
  if (s.size() != 7 || s[0] != '#' ||
      !std::all_of(s.begin() + 1, s.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
      }))
    fail();
}
} // namespace
Value defaults() {
  return V::Object{{"font", "sans"},
                   {"font_size", 15},
                   {"line_height", 1.5},
                   {"text_color", "#243d38"},
                   {"fill_color", "#fffefa"},
                   {"header_fill", "#e7ede7"},
                   {"header_text", "#243d38"},
                   {"band_fill", "#f3f6f1"},
                   {"border_color", "#ccd7cd"},
                   {"border_width", 1},
                   {"border_kind", "horizontal"},
                   {"border_style", "solid"},
                   {"padding_x", 12},
                   {"padding_y", 10},
                   {"min_row_height", 42},
                   {"cell_vertical", "top"},
                   {"zebra", true},
                   {"header_bold", true},
                   {"show_title", true},
                   {"caption_align", "left"},
                   {"caption_side", "top"},
                   {"column_weights", V::Array{}},
                   {"row_heights", V::Object{}},
                   {"cells", V::Object{}}};
}
Value presets() {
  V::Array out;
  const auto add = [&](const char *id, const char *label, V::Object values) {
    auto v = defaults().as_object();
    for (auto &[k, value] : values)
      v[k] = value;
    out.emplace_back(
        V::Object{{"id", id}, {"label", label}, {"appearance", V(v)}});
  };
  add("editorial", "Editorial", {});
  add("notebook", "Caderno",
      {{"font", "serif"},
       {"font_size", 17},
       {"fill_color", "#fcf8ef"},
       {"header_fill", "#ede4d0"},
       {"band_fill", "#f5efdf"},
       {"border_color", "#d9cfb9"},
       {"padding_y", 12}});
  add("grid", "Grade",
      {{"border_kind", "grid"},
       {"header_fill", "#2d5a50"},
       {"header_text", "#ffffff"},
       {"fill_color", "#ffffff"},
       {"band_fill", "#edf4f1"},
       {"border_color", "#aebfb6"}});
  add("night", "Noite",
      {{"fill_color", "#192a36"},
       {"text_color", "#ecf0ed"},
       {"header_fill", "#263f4d"},
       {"header_text", "#ffffff"},
       {"band_fill", "#203441"},
       {"border_color", "#4c6370"},
       {"border_kind", "horizontal"}});
  return out;
}
void validate(const Value &v, const std::vector<std::string> &rows,
              const std::vector<std::string> &columns,
              const std::set<std::string> &cells) {
  fields(v,
         {"font",         "font_size",      "line_height",    "text_color",
          "fill_color",   "header_fill",    "header_text",    "band_fill",
          "border_color", "border_width",   "border_kind",    "border_style",
          "padding_x",    "padding_y",      "min_row_height", "cell_vertical",
          "zebra",        "header_bold",    "show_title",     "caption_align",
          "caption_side", "column_weights", "row_heights",    "cells"});
  one_of(v.at("font"), {"sans", "serif", "mono"});
  range(v.at("font_size"), 10, 32);
  range(v.at("line_height"), 1, 2.2);
  for (const auto *k : {"text_color", "fill_color", "header_fill",
                        "header_text", "band_fill", "border_color"})
    color(v.at(k));
  range(v.at("border_width"), 0, 4);
  one_of(v.at("border_kind"), {"grid", "horizontal", "outer", "none"});
  one_of(v.at("border_style"), {"solid", "dashed", "dotted"});
  range(v.at("padding_x"), 0, 32);
  range(v.at("padding_y"), 0, 32);
  range(v.at("min_row_height"), 24, 240);
  one_of(v.at("cell_vertical"), {"top", "middle", "bottom"});
  for (const auto *k : {"zebra", "header_bold", "show_title"})
    (void)v.at(k).as_boolean();
  one_of(v.at("caption_align"), {"left", "center", "right"});
  one_of(v.at("caption_side"), {"top", "bottom"});
  const auto &weights = v.at("column_weights").as_array();
  if (!weights.empty()) {
    if (weights.size() != columns.size() || weights.size() > 32)
      fail();
    double sum = 0;
    for (const auto &w : weights) {
      range(w, .1, 100);
      sum += real(w);
    }
    if (std::abs(sum - 100) > 1e-6)
      fail();
  }
  const auto &heights = v.at("row_heights").as_object();
  if (heights.size() > rows.size())
    fail();
  for (const auto &[id, h] : heights) {
    if (std::find(rows.begin(), rows.end(), id) == rows.end())
      fail();
    range(h, 24, 480);
  }
  const auto &overrides = v.at("cells").as_object();
  if (overrides.size() > cells.size() || overrides.size() > 2048)
    fail();
  for (const auto &[id, c] : overrides) {
    if (!cells.contains(id) || c.as_object().empty() ||
        c.as_object().size() > 6)
      fail();
    for (const auto &[key, value] : c.as_object()) {
      if (key == "align")
        one_of(value, {"left", "center", "right", "justify"});
      else if (key == "vertical")
        one_of(value, {"top", "middle", "bottom"});
      else if (key == "fill" || key == "color")
        color(value);
      else if (key == "bold" || key == "italic")
        (void)value.as_boolean();
      else
        fail();
    }
  }
}
} // namespace inde::objects::pilot::table_design
