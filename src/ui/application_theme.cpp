#include "inde/ui/application_theme.hpp"

#include <gtkmm.h>

namespace inde::ui {

void install_application_theme() {
  static const auto provider = Gtk::CssProvider::create();
  static bool installed = false;
  if (installed)
    return;

  provider->load_from_data(R"css(
window.inde-window {
  background: #10141b;
  color: #e8edf5;
}

headerbar.inde-header {
  min-height: 58px;
  padding: 0 10px;
  background: #151a22;
  border-bottom: 1px solid #2a3240;
  box-shadow: none;
}

.header-title {
  color: #f6f8fb;
  font-weight: 800;
  font-size: 1.04em;
}

.header-subtitle {
  color: #8f9bad;
  font-size: 0.82em;
}

.app-menubar {
  min-height: 32px;
  padding: 0 8px;
  background: #131821;
  border-bottom: 1px solid #252e3a;
}

.app-menubar > item {
  padding: 5px 10px;
  border-radius: 6px;
}

.app-menubar > item:hover {
  background: #242c38;
}

.inde-header .title {
  font-weight: 700;
}

button {
  border-radius: 8px;
  padding: 7px 12px;
  min-height: 18px;
}

button.header-action {
  background: transparent;
  border-color: transparent;
  box-shadow: none;
}

button.header-action:hover {
  background: #242b36;
}

/* O tema do sistema pode suavizar ou ocultar o foco. Este anel atende teclado
 * e baixa visão sem depender de hover e cobre os controles interativos usados
 * pelo shell, cards, listas, formulários e menus. */
window.inde-window button:focus,
window.inde-window entry:focus,
window.inde-window searchentry:focus,
window.inde-window combobox:focus,
window.inde-window listview:focus,
window.inde-window flowboxchild:focus,
window.inde-window expander:focus {
  outline: 2px solid #86b4ff;
  outline-offset: 2px;
}

.modal-scrim {
  background: rgba(6, 9, 14, 0.76);
}

.modal-card {
  background: #191f29;
  border: 1px solid #3a4658;
  border-radius: 16px;
  box-shadow: 0 22px 64px rgba(0,0,0,0.58);
}

.modal-header {
  padding: 20px 22px 16px 22px;
  border-bottom: 1px solid #2d3745;
}

.modal-secondary {
  color: #a6b1c1;
  line-height: 1.25;
}

.modal-body {
  padding: 4px 8px;
}

.modal-footer {
  padding: 14px 22px 18px 22px;
  border-top: 1px solid #2d3745;
}

.context-menu {
  background: #1b222d;
  border: 1px solid #394658;
  border-radius: 10px;
  box-shadow: 0 10px 28px rgba(0,0,0,0.46);
}

button.context-menu-item {
  min-width: 190px;
  background: transparent;
  border-color: transparent;
  box-shadow: none;
}

button.context-menu-item:hover {
  background: #293444;
}

button.context-menu-item.destructive-action {
  color: #ff9e9e;
}

button.destructive-action:disabled {
  background: #302328;
  border-color: #463138;
  color: #796b70;
}

button.unavailable-action {
  background: #252a33;
  border-color: #303846;
  color: #6f7783;
}

.workspace-switcher {
  padding: 4px;
  margin: 4px 16px 0 16px;
  background: #171c24;
  border: 1px solid #2a3240;
  border-radius: 12px;
}

.workspace-switcher button {
  min-width: 108px;
  background: transparent;
  border-color: transparent;
  box-shadow: none;
  color: #aeb8c7;
}

.workspace-switcher button:hover {
  background: #222a35;
  color: #f4f7fb;
}

.workspace-switcher button:checked {
  background: #315fba;
  color: #ffffff;
  font-weight: 700;
}

.workspace-page {
  padding: 4px;
}

.page-title {
  color: #f6f8fb;
  font-weight: 800;
}

.page-subtitle {
  color: #98a4b5;
}

.toolbar-surface,
.tool-section {
  background: #171d26;
  border: 1px solid #2d3746;
  border-radius: 12px;
  box-shadow: 0 1px 2px rgba(0,0,0,0.22);
}

.tool-section {
  padding: 10px 12px;
}

.tool-section .heading {
  color: #9daabd;
  font-size: 0.88em;
  font-weight: 700;
}

/* A barra não depende apenas do clique: ela espelha o estilo sob o cursor ou
 * na seleção para que a leitura da formatação seja imediata durante a escrita. */
.writing-format-toolbar button.format-active {
  background: #315f9d;
  border-color: #78a8ed;
  color: #ffffff;
  box-shadow: inset 0 0 0 1px rgba(222, 238, 255, 0.18);
}

.writing-format-toolbar button.format-mixed {
  background: #263d5a;
  border-color: #5e83b5;
  color: #dceaff;
}

.filter-sheet {
  background: #1a202a;
  border: 1px solid #3b495e;
  border-radius: 14px;
  box-shadow: 0 12px 32px rgba(0,0,0,0.44);
}

.content-card {
  background: #171c24;
  border: 1px solid #2b3441;
  border-radius: 12px;
  padding: 14px;
}

.content-card.accent-card {
  background: #182233;
  border-color: #355786;
}

button.entity-card,
button.catalog-card {
  background: #181e27;
  border: 1px solid #2d3746;
  border-radius: 12px;
  box-shadow: 0 1px 2px rgba(0,0,0,0.22);
}

button.entity-card:hover,
button.catalog-card:hover {
  background: #202938;
  border-color: #5576a9;
}

button.entity-card:focus,
button.catalog-card:focus {
  outline: 2px solid #6f9de8;
  outline-offset: 2px;
}

button.catalog-card.selected-card {
  background: #203a66;
  border-color: #6e9de8;
}

.catalog-cover,
.catalog-cover-placeholder {
  background: #222b37;
  border-right: 1px solid #354255;
  border-radius: 10px 0 0 10px;
}

.catalog-cover-placeholder image {
  color: #738198;
}

.editorial-profile-cover {
  background: linear-gradient(135deg, #24446f, #18283f 58%, #332d59);
  border-radius: 11px 11px 0 0;
}

.editorial-profile-cover + picture,
picture.catalog-cover {
  border-radius: 11px 11px 0 0;
}

button.floating-action {
  background: rgba(14, 19, 27, 0.88);
  border: 1px solid rgba(163, 184, 216, 0.5);
  border-radius: 999px;
  box-shadow: 0 5px 18px rgba(0,0,0,0.38);
}

button.document-group-card {
  background: #1c2633;
  border-color: #3b5776;
  border-radius: 14px;
}

button.document-group-card:hover {
  background: #243449;
  border-color: #6590c4;
}

.writing-library {
  padding-top: 2px;
}

.writing-toolbar {
  padding: 10px 12px;
}

.writing-format-toolbar {
  padding: 7px 10px;
}

.writing-format-toolbar button {
  min-width: 34px;
}

.writing-paper {
  background: #131820;
  border-color: #354152;
  border-radius: 14px;
}

.writing-tools-sheet {
  background: #181f29;
  border-left: 1px solid #3a4759;
  border-radius: 14px 0 0 14px;
  box-shadow: -12px 0 30px rgba(0,0,0,0.38);
}

textview.writing-editor,
textview.writing-editor > text {
  background: #131820;
  color: #edf1f7;
  caret-color: #9dc1ff;
  font-family: serif;
  font-size: 1.08em;
}

textview.writing-editor selection,
textview.writing-editor > text selection {
  background: #315fba;
  color: #ffffff;
}

button.quiet-destructive {
  background: transparent;
  border-color: transparent;
  color: #e7a0a0;
  box-shadow: none;
}

button.quiet-destructive:hover {
  background: #33242a;
  color: #ffb2b2;
}

button.filter-toggle {
  background: #202733;
  border-color: #364254;
  color: #aeb8c7;
}

button.filter-toggle.filter-enabled {
  background: #214f43;
  border-color: #4f9a7d;
  color: #effff8;
  font-weight: 700;
}

button.filter-toggle.filter-disabled {
  background: #252a33;
  border-color: #46505f;
  color: #b1bac7;
  font-weight: 600;
}

.section-title {
  color: #f0f3f8;
  font-weight: 750;
}

.context-summary {
  color: #9faabc;
  padding: 2px 4px;
}

.detail-toolbar {
  padding: 4px 0 8px 0;
  border-bottom: 1px solid #2a3240;
}

.detail-content {
  padding: 2px 6px 20px 6px;
}

.boxed-list {
  background: transparent;
}

.boxed-list > row {
  margin: 2px 0;
  border-radius: 8px;
}

.boxed-list > row button {
  border-radius: 8px;
  padding: 9px 12px;
}

.status-bar {
  min-height: 24px;
  padding: 3px 12px;
  color: #8f9bad;
  background: #12171e;
  border-top: 1px solid #252d39;
  font-size: 0.88em;
}

.timeline-toolbar {
  background: #171d26;
  border: 1px solid #2d3746;
  border-radius: 12px;
  padding: 8px;
}

.inspector-surface {
  background: #151a22;
  border-left: 1px solid #2d3746;
  padding: 14px;
}

.placeholder-card {
  background: #171d26;
  border: 1px solid #2d3746;
  border-radius: 16px;
  padding: 28px;
  box-shadow: 0 6px 20px rgba(0,0,0,0.24);
}

entry.proofreading-spelling-error {
  box-shadow: inset 0 -2px #e25562;
}

entry.proofreading-grammar-error {
  border-bottom-color: #d08a31;
}

entry.proofreading-spelling-error.proofreading-grammar-error {
  box-shadow: inset 0 -2px #e25562, inset 0 -4px #d08a31;
}
  )css");

  const auto display = Gdk::Display::get_default();
  if (display) {
    Gtk::StyleContext::add_provider_for_display(
        display, provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    installed = true;
  }
}

} // namespace inde::ui
