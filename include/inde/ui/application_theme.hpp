#pragma once
#include <sigc++/sigc++.h>

namespace inde::ui {

// Instala a paleta do INDE sobre uma base GTK compatível. Os executáveis preparam
// a base antes da inicialização GTK; fontes/escala/ícones do desktop permanecem.
// System acompanha a preferência claro/escuro, não o CSS externo do desktop.
void install_application_theme();
enum class ThemeMode { System, Light, Dark };
void set_application_theme(ThemeMode);
[[nodiscard]] ThemeMode application_theme() noexcept;
[[nodiscard]] bool application_theme_is_dark() noexcept;
sigc::signal<void()> &signal_application_theme_changed();

} // namespace inde::ui
