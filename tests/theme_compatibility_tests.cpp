#include "../src/ui/theme_compatibility.hpp"
#include <glib.h>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
struct Environment {
  std::optional<std::string> original;
  Environment() {
    if (const auto *value = g_getenv("GTK_THEME")) original = value;
  }
  ~Environment() {
    if (original) g_setenv("GTK_THEME", original->c_str(), true);
    else g_unsetenv("GTK_THEME");
  }
};
}
int main() {
  try {
    Environment restore;
    g_unsetenv("GTK_THEME");
    require(inde::ui::configure_gtk_theme_base(), "Missing base was not configured");
    require(std::string(g_getenv("GTK_THEME")) == "Adwaita", "Wrong compatible base");
    require(!inde::ui::configure_gtk_theme_base(), "Repeated call changed the base");
    g_setenv("GTK_THEME", "", true);
    require(inde::ui::configure_gtk_theme_base(), "Empty override was not treated as unset");
    for (const auto *choice : {"Adwaita:dark", "HighContrast", "Orchis-Dark-Compact"}) {
      g_setenv("GTK_THEME", choice, true);
      require(!inde::ui::configure_gtk_theme_base(), "Explicit override was replaced");
      require(std::string(g_getenv("GTK_THEME")) == choice, "Override was lost");
    }
    require(inde::ui::theme_name_is_dark("Orchis-Dark-Compact"), "Uppercase Dark ignored");
    require(inde::ui::theme_name_is_dark("ADWAITA:DARK"), "Uppercase variant ignored");
    require(inde::ui::theme_name_is_dark("Adwaita-dark"), "Lowercase dark ignored");
    require(!inde::ui::theme_name_is_dark("Adwaita"), "Light theme classified as dark");
    require(!inde::ui::theme_name_is_dark(""), "Empty theme classified as dark");
    std::cout << "PASS: compatible base, unset/empty/idempotent, explicit overrides and case-insensitive dark\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
