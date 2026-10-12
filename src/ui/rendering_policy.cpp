#include "rendering_policy.hpp"

#include <algorithm>
#include <fstream>
#include <glib.h>
#include <gtk/gtk.h>
#include <iostream>
#include <string>
#include <vector>

namespace inde::ui {
namespace {
std::string read_identifier(const std::filesystem::path &path) {
  std::ifstream input(path);
  std::string value;
  input >> value;
  return value;
}
void disable_cairo_accelerators(const char *variable, bool modern) {
  const auto *existing = g_getenv(variable);
  auto **split = g_strsplit_set(existing ? existing : "", ":;, \t", -1);
  std::vector<std::string> tokens;
  bool all = false;
  for (auto **p = split; *p; ++p)
    if (**p) {
      tokens.emplace_back(*p);
      all = all || g_ascii_strcasecmp(*p, "all") == 0;
    }
  g_strfreev(split);
  for (const auto *feature :
       modern ? std::vector<const char *>{"gl", "vulkan"}
              : std::vector<const char *>{"gl-disable", "vulkan-disable"}) {
    const auto matches = [feature](const auto &v) {
      return g_ascii_strcasecmp(v.c_str(), feature) == 0;
    };
    if (all)
      std::erase_if(tokens, matches); // "all" means subtraction of named flags.
    else if (std::none_of(tokens.begin(), tokens.end(), matches))
      tokens.emplace_back(feature);
  }
  std::string flags;
  for (const auto &v : tokens) {
    if (!flags.empty())
      flags += ",";
    flags += v;
  }
  g_setenv(variable, flags.c_str(), true);
}
} // namespace

bool detect_legacy_intel_graphics(const std::filesystem::path &drm_root) {
  std::error_code error;
  std::filesystem::directory_iterator entries(drm_root, error), end;
  while (!error && entries != end) {
    const auto name = entries->path().filename().string();
    if (name.starts_with("card") && name.size() > 4 &&
        std::all_of(name.begin() + 4, name.end(),
                    [](char c) { return c >= '0' && c <= '9'; })) {
      const auto device = entries->path() / "device";
      if (needs_cairo_renderer(read_identifier(device / "vendor"),
                               read_identifier(device / "device")))
        return true;
    }
    entries.increment(error);
  }
  return false;
}

bool configure_rendering_backend(const std::filesystem::path &drm_root) {
  bool changed = false;
  if (!g_getenv("GSK_RENDERER") && detect_legacy_intel_graphics(drm_root)) {
    changed = g_setenv("GSK_RENDERER", "cairo", false);
    if (changed)
      std::clog << "[INDE] Renderer compatibility: Cairo for Intel 8086:0102; "
                   "explicit GSK_RENDERER overrides this default.\n";
  }
  const auto *renderer = g_getenv("GSK_RENDERER");
  if (renderer && std::string_view(renderer) == "cairo") {
    // Cairo doesn't need a GL context. Avoid probing EGL/DRI3 anyway.
    // GTK 4.16 moved these feature flags from GDK_DEBUG to GDK_DISABLE.
    const bool modern =
        gtk_get_major_version() > 4 || gtk_get_minor_version() >= 16;
    const char *variable = modern ? "GDK_DISABLE" : "GDK_DEBUG";
    disable_cairo_accelerators(variable, modern);
  }
  return changed;
}
} // namespace inde::ui
