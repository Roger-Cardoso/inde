#pragma once
#include <string_view>

namespace inde::ui {
// Before Gtk::Application/gtk_init. Use GTK's built-in base, not arbitrary
// desktop CSS requiring a newer parser. The INDE palette is installed later.
// Process-local only; a nonempty explicit GTK_THEME remains a diagnostic opt-in.
bool configure_gtk_theme_base();
bool theme_name_is_dark(std::string_view name);
} // namespace inde::ui
