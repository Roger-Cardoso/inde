#include "theme_compatibility.hpp"
#include <glib.h>
#include <string>

namespace inde::ui {
bool configure_gtk_theme_base() {
  const auto *requested = g_getenv("GTK_THEME");
  if (requested && *requested)
    return false;
  // GtkSettings loads theme CSS as soon as it is constructed. Setting its
  // theme-name property after initialization is too late to prevent parsing
  // errors. GTK_THEME chooses the provider without replacing the desktop
  // gtk-theme-name/prefer-dark properties used by INDE's System mode.
  return g_setenv("GTK_THEME", "Adwaita", true);
}
bool theme_name_is_dark(std::string_view name) {
  std::string lower(name);
  for (auto &c : lower)
    c = g_ascii_tolower(c);
  return lower.find("dark") != std::string::npos;
}
} // namespace inde::ui
