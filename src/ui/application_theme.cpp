#include "inde/ui/application_theme.hpp"
#include "inde/ui/ui_preferences.hpp"
#include "theme_compatibility.hpp"
#include <gtkmm.h>
#include <array>
#include <string>
namespace inde::ui {
namespace {
Glib::RefPtr<Gtk::CssProvider> provider;
ThemeMode mode = ThemeMode::System;
bool dark = false, system_dark = false, system_preference = false, applying = false;
sigc::signal<void()> changed;
// Shared foreground/background pairs cover nested GTK labels and popovers.
std::string colors(bool use_dark) {
  const std::array<const char *, 17> light{
      "#f5f6f8", "#ffffff", "#eceff3", "#ffffff", "#18212e", "#526174",
      "#6a7788", "#c9d1dc", "#205abe", "#ffffff", "#e8effb", "#174b9e",
      "#b12d3c", "#fcecee", "#176447", "#e5f3eb", "#7d8999"};
  const std::array<const char *, 17> night{
      "#15191f", "#1e242c", "#2a323d", "#1a2028", "#e7edf5", "#b1bdcc",
      "#91a0b2", "#49576a", "#85b4ff", "#102344", "#263d5a", "#c2dbff",
      "#ff9ba5", "#3c252e", "#a2dfbf", "#213d32", "#69798d"};
  const std::array<const char *, 17> names{
      "bg", "surface", "hover", "paper", "fg", "muted", "disabled", "border",
      "accent", "on_accent", "accent_soft", "accent_fg", "danger", "danger_bg",
      "success", "success_bg", "control_border"};
  std::string css;
  for (std::size_t i = 0; i < names.size(); ++i)
    css += std::string("@define-color inde_") + names[i] + " " +
           (use_dark ? night[i] : light[i]) + ";\n";
  return css;
}
constexpr auto component_css = R"css(
window.inde-window .calendar-picker { padding: 6px; }
window.inde-window button.calendar-day { background: transparent; border-color: transparent;
  border-radius: 6px; padding: 4px; min-height: 28px; }
window.inde-window button.calendar-day:hover { background: @inde_hover; }
window.inde-window button.calendar-day.calendar-in-range { background: @inde_accent_soft; color: @inde_accent_fg; }
window.inde-window button.calendar-day.calendar-selected { background: @inde_accent; color: @inde_on_accent;
  border-color: @inde_accent; font-weight: 700; }
window.inde-window button.calendar-day.calendar-marked { text-decoration-line: underline; }
window.inde-window button.calendar-day.calendar-cut { border-bottom: 2px solid @inde_accent; }
window.inde-window .calendar-weekday { color: @inde_muted; font-size: 0.85em; }
window.inde-window { background: @inde_bg; color: @inde_fg; }
window.inde-window label, window.inde-window image { color: inherit; }
window.inde-window .dim-label, window.inde-window .page-subtitle,
window.inde-window .context-summary, window.inde-window .modal-secondary {
  color: @inde_muted; opacity: 1;
}
headerbar.inde-header { min-height: 44px; padding: 0 12px; background: @inde_surface;
  color: @inde_fg; border-bottom: 1px solid @inde_border; box-shadow: none; }
.header-title { color: @inde_fg; font-weight: 600; }
.header-subtitle { color: @inde_muted; font-size: 0.85em; }
.page-title, .section-title { color: @inde_fg; font-weight: 600; }
window.inde-window button { background: @inde_surface; color: @inde_fg;
  background-image: none; border: 1px solid @inde_control_border; border-radius: 8px;
  box-shadow: none; padding: 6px 10px; min-height: 22px; text-shadow: none; }
window.inde-window button:hover { background: @inde_hover; }
window.inde-window button.header-action, window.inde-window button.flat,
window.inde-window button.context-menu-item { background: transparent;
  border-color: transparent; box-shadow: none; }
window.inde-window button.header-action:hover, window.inde-window button.flat:hover,
window.inde-window button.context-menu-item:hover { background: @inde_hover; }
window.inde-window button:disabled { color: @inde_disabled; background: @inde_bg;
  border-color: @inde_border; opacity: 1; }
window.inde-window button:disabled label, window.inde-window button:disabled image {
  color: @inde_disabled; opacity: 1; }
window.inde-window button.suggested-action { background: @inde_accent;
  color: @inde_on_accent; border-color: @inde_accent; }
window.inde-window button.suggested-action:hover { background: shade(@inde_accent, 0.94); }
window.inde-window button.suggested-action:disabled { background: @inde_hover;
  color: @inde_disabled; border-color: @inde_border; }
window.inde-window button.destructive-action, window.inde-window button.quiet-destructive {
  color: @inde_danger; }
window.inde-window button.destructive-action:hover,
window.inde-window button.quiet-destructive:hover { background: @inde_danger_bg; }
window.inde-window entry, window.inde-window searchentry,
window.inde-window spinbutton, window.inde-window dropdown {
  background: @inde_paper; color: @inde_fg; caret-color: @inde_fg;
  border: 1px solid @inde_control_border; border-radius: 7px; box-shadow: none; min-height: 30px; }
window.inde-window entry:disabled { color: @inde_disabled; background: @inde_bg; }
window.inde-window entry placeholder { color: @inde_muted; }
window.inde-window textview, window.inde-window textview text {
  background: @inde_paper; color: @inde_fg; caret-color: @inde_fg; }
window.inde-window textview selection, window.inde-window entry selection {
  background: @inde_accent; color: @inde_on_accent; }
window.inde-window check, window.inde-window radio { background: @inde_paper;
  border-color: @inde_control_border; color: @inde_on_accent; }
window.inde-window check:checked, window.inde-window radio:checked {
  background: @inde_accent; border-color: @inde_accent; }
window.inde-window button:focus-visible, window.inde-window entry:focus,
window.inde-window searchentry:focus, window.inde-window combobox:focus,
window.inde-window listview:focus, window.inde-window flowboxchild:focus,
window.inde-window expander:focus-visible { outline: 2px solid @inde_accent;
  outline-offset: 2px; }
window.inde-window list, window.inde-window listview { background: transparent; color: @inde_fg; }
window.inde-window row { color: @inde_fg; border-radius: 6px; }
window.inde-window row:hover { background: @inde_hover; }
window.inde-window row:selected { background: @inde_accent_soft; color: @inde_accent_fg; }
window.inde-window separator { background: @inde_border; min-height: 1px; min-width: 1px; }
window.inde-window notebook, window.inde-window notebook > stack {
  background: @inde_surface; color: @inde_fg; border-color: @inde_border; }
window.inde-window scrolledwindow { background: transparent; border-color: @inde_border; }
window.inde-window viewport { background: @inde_surface; color: @inde_fg; }
window.inde-window frame > border { border-color: @inde_border; }
window.inde-window notebook > header { background: @inde_surface; color: @inde_fg; border-color: @inde_border; }
window.inde-window notebook > header tab { color: @inde_muted; }
window.inde-window notebook > header tab:checked { color: @inde_accent_fg; border-bottom-color: @inde_accent; }
window.inde-window progressbar trough { background: @inde_hover; }
window.inde-window progressbar progress { background: @inde_accent; }
window.inde-window scrollbar slider { background: @inde_disabled; }
window.inde-window tooltip { background: @inde_surface; color: @inde_fg; }
popover > contents { background: @inde_surface; color: @inde_fg;
  border: 1px solid @inde_border; border-radius: 10px; box-shadow: 0 4px 18px alpha(black, 0.12); }
popover.context-menu { background: transparent; border: none; box-shadow: none; padding: 0; }
popover.context-menu > contents { box-shadow: none; margin: 0; padding: 0; }
popover.context-menu viewport { background: transparent; }
popover label, popover modelbutton { color: @inde_fg; }
popover modelbutton:hover { background: @inde_hover; }
popover modelbutton:disabled { color: @inde_disabled; }
.workspace-switcher { padding: 6px 12px; }
window.inde-window .workspace-switcher button { min-width: 76px; background: transparent;
  border-color: transparent; color: @inde_muted; }
window.inde-window .workspace-switcher button:checked { background: @inde_accent_soft;
  color: @inde_accent_fg; font-weight: 600; }
.workspace-page { padding: 2px; }
.comparison-selection-bar { background: @inde_surface; color: @inde_fg; border: 1px solid @inde_border; border-radius: 10px; padding: 12px; box-shadow: 0 4px 12px alpha(black, 0.18); }
.toolbar-surface, .timeline-toolbar { background: @inde_surface; color: @inde_fg;
  border: 1px solid @inde_border; border-radius: 10px; padding: 8px; }
.tool-section, .content-card, .placeholder-card { background: @inde_surface;
  color: @inde_fg; border: 1px solid @inde_border; border-radius: 10px; padding: 14px; }
.tool-section .heading { color: @inde_muted; font-weight: 600; }
button.entity-card, button.catalog-card, button.document-group-card {
  background: @inde_surface; color: @inde_fg; border: 1px solid @inde_border; border-radius: 10px; }
button.entity-card label, button.catalog-card label, button.document-group-card label { color: @inde_fg; }
button.entity-card label.dim-label, button.catalog-card label.dim-label,
button.document-group-card label.dim-label { color: @inde_muted; opacity: 1; }
button.entity-card:hover, button.catalog-card:hover, button.document-group-card:hover { background: @inde_hover; }
button.catalog-card.selected-card, .content-card.accent-card {
  background: @inde_accent_soft; color: @inde_accent_fg; border-color: @inde_accent; }
.catalog-cover, .catalog-cover-placeholder, .editorial-profile-cover { background: @inde_hover; border-radius: 8px; }
.catalog-cover-placeholder image { color: @inde_muted; }
.writing-paper { background: @inde_paper; border-color: @inde_border; border-radius: 10px; }
textview.writing-editor, textview.writing-editor > text { background: @inde_paper;
  color: @inde_fg; caret-color: @inde_fg; font-family: serif; font-size: 12pt; }
textview.writing-editor text selection { background: @inde_accent_soft; color: @inde_accent_fg; }
.writing-format-toolbar { padding: 8px; }
.writing-format-toolbar button.format-active { background: @inde_accent;
  color: @inde_on_accent; border-color: @inde_accent; }
.writing-format-toolbar button.format-mixed { background: @inde_accent_soft;
  color: @inde_accent_fg; border-color: @inde_accent; }
.writing-tools-sheet, .inspector-surface { background: @inde_surface;
  color: @inde_fg; border: 1px solid @inde_border; border-radius: 10px; }
.filter-sheet, .modal-card { background: @inde_surface; color: @inde_fg;
  border: 1px solid @inde_border; border-radius: 12px; box-shadow: 0 8px 28px alpha(black, 0.16); }
.modal-scrim { background: alpha(black, 0.38); }
.modal-header { padding: 16px 20px; border-bottom: 1px solid @inde_border; }
.modal-body { padding: 4px 8px; }
.modal-footer { padding: 14px 20px; border-top: 1px solid @inde_border; }
button.filter-toggle { color: @inde_fg; }
button.filter-toggle.filter-enabled { background: @inde_success_bg; color: @inde_success; }
button.filter-toggle.filter-disabled { background: @inde_bg; color: @inde_muted; }
.detail-toolbar { padding: 4px 0 8px; border-bottom: 1px solid @inde_border; }
.detail-content { padding: 2px 6px 20px; }
.boxed-list > row { margin: 2px 0; }
.status-bar { padding: 3px 12px; color: @inde_muted; font-size: 0.85em; }
.review-row { padding: 14px; }
.review-kind { color: @inde_accent_fg; font-weight: 600; }
.workshop-summary, .workshop-acknowledge { color: @inde_fg; background: @inde_surface; padding: 8px; }
entry.proofreading-spelling-error { box-shadow: inset 0 -2px @inde_danger; }
entry.proofreading-grammar-error { border-bottom-color: @inde_accent; }
)css";
void apply() {
  applying = true;
  dark = mode == ThemeMode::Dark || (mode == ThemeMode::System && system_dark);
  // The app palette must not write back to GTK's system preference: theme
  // notifications can be deferred, feeding our own setting back into System.
  provider->load_from_data(colors(dark) + component_css);
  applying = false;
  changed.emit();
}
}
void install_application_theme() {
  if (provider) return;
  const auto display = Gdk::Display::get_default();
  if (!display) return;
  if (auto settings = Gtk::Settings::get_default()) {
    system_preference = settings->property_gtk_application_prefer_dark_theme().get_value();
    system_dark = system_preference;
    const auto theme = settings->property_gtk_theme_name().get_value();
    system_dark = system_dark || theme_name_is_dark(theme.raw());
    settings->property_gtk_theme_name().signal_changed().connect([settings] {
      if (applying) return;
      system_dark = system_preference || theme_name_is_dark(settings->property_gtk_theme_name().get_value().raw());
      if (provider && mode == ThemeMode::System) apply();
    });
    settings->property_gtk_application_prefer_dark_theme().signal_changed().connect([settings] {
      if (applying) return;
      system_preference = settings->property_gtk_application_prefer_dark_theme().get_value();
      system_dark = system_preference || theme_name_is_dark(settings->property_gtk_theme_name().get_value().raw());
      if (provider && mode == ThemeMode::System) apply();
    });
  }
  const auto saved = ui_preference("appearance", "theme", "system");
  mode = saved == "dark" ? ThemeMode::Dark : saved == "light" ? ThemeMode::Light : ThemeMode::System;
  provider = Gtk::CssProvider::create();
  Gtk::StyleContext::add_provider_for_display(display, provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  apply();
}
void set_application_theme(ThemeMode value) {
  install_application_theme();
  mode = value;
  set_ui_preference("appearance", "theme", value == ThemeMode::Dark ? "dark" : value == ThemeMode::Light ? "light" : "system");
  if (provider) apply();
}
ThemeMode application_theme() noexcept { return mode; }
bool application_theme_is_dark() noexcept { return dark; }
sigc::signal<void()> &signal_application_theme_changed() { return changed; }
}
