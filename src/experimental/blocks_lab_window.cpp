#include "inde/experimental/blocks_lab_window.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/application_theme.hpp"
namespace inde::experimental {
namespace {
void text_area(Gtk::TextView &text, Gtk::ScrolledWindow &scroll, bool edit = false) {
  text.set_editable(edit); text.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
  text.add_css_class("writing-editor"); text.set_left_margin(10); text.set_right_margin(10); text.set_top_margin(10);
  scroll.set_child(text); scroll.set_vexpand(true); scroll.set_hexpand(true);
}
void bounded_selector(Gtk::ComboBoxText &combo) {
  for (auto *cell : combo.get_cells()) if (auto *text = dynamic_cast<Gtk::CellRendererText *>(cell)) {
    text->property_ellipsize() = Pango::EllipsizeMode::END; text->property_max_width_chars() = 30;
  }
}
}
BlocksLabWindow::BlocksLabWindow(const std::filesystem::path &copy) : source_(copy) {
  set_title("INDE — montagem de texto"); set_default_size(1280, 900); add_css_class("inde-window");
  add_css_class("text-workshop");
  for (auto *combo : {&documents_, &versions_, &alternatives_, &layers_}) bounded_selector(*combo);
  root_.set_margin(12); set_child(root_); heading_.add_css_class("page-title"); heading_.set_xalign(0); root_.append(heading_);
  notice_.set_text("Laboratório: a montagem e seus rascunhos ficam em memória. Somente ‘Criar Documento na cópia’, após confirmação, grava um novo texto. As origens não são alteradas.");
  notice_.set_wrap(true); notice_.set_xalign(0); root_.append(notice_);
  title_.set_text(state_.title); title_.set_placeholder_text("Nome da montagem / novo Documento"); title_.set_hexpand(true);
  top_.append(title_); top_.append(reset_); top_.append(undo_button_); top_.append(redo_button_); top_.append(prepare_);
  organization_toggle_.set_active(true); top_.append(organization_toggle_);
  organization_toggle_.signal_toggled().connect([this] { sidebar_box_.set_visible(organization_toggle_.get_active()); });
  theme_choices_.set_margin(8);
  for (auto *b : {&light_theme_, &dark_theme_, &system_theme_}) theme_choices_.append(*b);
  theme_popover_.set_child(theme_choices_); theme_menu_.set_popover(theme_popover_);
  theme_menu_.set_icon_name("weather-clear-symbolic"); theme_menu_.set_tooltip_text("Escolher tema"); top_.append(theme_menu_);
  light_theme_.signal_clicked().connect([this] { ui::set_application_theme(ui::ThemeMode::Light); theme_popover_.popdown(); });
  dark_theme_.signal_clicked().connect([this] { ui::set_application_theme(ui::ThemeMode::Dark); theme_popover_.popdown(); });
  system_theme_.signal_clicked().connect([this] { ui::set_application_theme(ui::ThemeMode::System); theme_popover_.popdown(); });
  root_.append(top_);
  confirmation_text_.set_wrap(true); confirmation_text_.set_xalign(0); confirmation_text_.set_hexpand(true);
  confirmation_.append(confirmation_text_); confirmation_buttons_.append(cancel_); confirmation_buttons_.append(confirm_);
  confirmation_.append(confirmation_buttons_); confirmation_.set_visible(false); root_.append(confirmation_);
  sidebar_box_.append(name_); focus_info_.set_xalign(0); focus_info_.set_wrap(true);
  root_.append(focus_info_); sidebar_box_.append(sidebar_); sidebar_.set_vexpand(true);
  horizontal_.set_start_child(sidebar_box_); horizontal_.set_end_child(preview_tabs_); horizontal_.set_position(330); horizontal_.set_vexpand(true);
  sidebar_.set_size_request(300, -1); sidebar_.set_scrollable(true);
  documents_.set_tooltip_text("Documento de origem: mudar fonte não descarta a montagem");
  source_box_.append(documents_); source_actions_.append(previous_); source_actions_.append(next_); source_box_.append(source_actions_);
  name_.set_placeholder_text("Nome para bloco, alternativa ou camada");
  name_.set_tooltip_text("Nome usado por Extrair, Nomear, Criar alternativa, Promover e Criar camada");
  source_box_.append(extract_); source_box_.append(whole_); text_area(source_text_, source_scroll_); source_text_.set_tooltip_text("Origem somente leitura: selecione a faixa que será um bloco");
  source_box_.append(source_scroll_); sidebar_.append_page(source_box_, "Origens");
  library_.set_selection_mode(Gtk::SelectionMode::SINGLE); library_scroll_.set_child(library_); library_scroll_.set_min_content_height(120); library_scroll_.set_max_content_height(180);
  library_box_.append(library_scroll_); versions_.set_tooltip_text("Versão do bloco escolhido na biblioteca"); library_box_.append(versions_);
  library_actions_.append(open_); library_actions_.append(reuse_); library_box_.append(library_actions_);
  library_actions2_.append(rename_); library_actions2_.append(rename_variant_); library_box_.append(library_actions2_);
  auto *version_options = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
  for (auto *b : {&split_, &cursor_split_, &pin_, &follow_, &fork_, &alternative_, &promote_}) version_options->append(*b);
  auto *versions_expander = Gtk::make_managed<Gtk::Expander>("Divisões, versões e alternativas");
  versions_expander->set_child(*version_options); library_box_.append(*versions_expander);
  auto *layer_options = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
  layer_options->append(layers_); layer_options->append(assign_layer_);
  layer_actions_.append(new_layer_); layer_actions_.append(rename_layer_);
  layer_options->append(layer_actions_); layer_options->append(layer_visibility_);
  auto *layers_expander = Gtk::make_managed<Gtk::Expander>("Camadas de leitura");
  layers_expander->set_child(*layer_options); library_box_.append(*layers_expander);
  library_page_scroll_.set_child(library_box_);
  sidebar_.append_page(library_page_scroll_, "Blocos");
  occurrences_.set_selection_mode(Gtk::SelectionMode::SINGLE); montage_scroll_.set_child(occurrences_); montage_scroll_.set_vexpand(true);
  montage_box_.append(montage_scroll_); montage_actions_.append(up_); montage_actions_.append(down_); montage_box_.append(montage_actions_);
  for (auto *b : {&repeat_, &hide_, &remove_, &ref_, &origin_}) montage_box_.append(*b);
  sidebar_.append_page(montage_box_, "Usos dos blocos"); text_area(impact_text_, impact_scroll_); sidebar_.append_page(impact_scroll_, "Usos/impacto");
  editor_box_.set_margin(12); editor_box_.append(edit_heading_);
  editor_selector_.set_tooltip_text("Rascunhos abertos: mostrar apenas uma edição por vez");
  bounded_selector(editor_selector_); editor_box_.append(editor_selector_);
  editor_selector_.signal_changed().connect([this] {
    if (!selecting_editor_ && editor_selector_.get_active_row_number() >= 0)
      show_editor(static_cast<std::size_t>(editor_selector_.get_active_row_number()));
  });
  editor_grid_.set_row_spacing(8); editor_grid_.set_column_spacing(8);
  editors_scroll_.set_child(editor_grid_); editors_scroll_.set_vexpand(true); editor_box_.append(editors_scroll_);
  preview_tabs_.append_page(editor_box_, "Editar trechos");
  alternatives_.set_tooltip_text("Alternativas de leitura: cada família preserva sua Base"); read_box_.append(alternatives_);
  read_info_.set_wrap(true); read_info_.set_xalign(0); read_info_.add_css_class("workshop-summary"); read_box_.append(read_info_);
  text_area(read_text_, read_scroll_); read_text_.set_tooltip_text("Leitura renderizada: somente versões em uso na montagem, sem rascunhos");
  read_box_.append(read_scroll_); preview_tabs_.append_page(read_box_, "Leitura");
  text_area(map_text_, map_scroll_); preview_tabs_.append_page(map_scroll_, "Posições e origens dos blocos");
  text_area(output_text_, output_scroll_); text_area(warnings_text_, warnings_scroll_); warnings_scroll_.set_min_content_height(90);
  acknowledge_.add_css_class("workshop-acknowledge");
  output_box_.append(output_scroll_); output_box_.append(warnings_scroll_); output_box_.append(acknowledge_); output_box_.append(publish_);
  publish_.set_sensitive(false); preview_tabs_.append_page(output_box_, "Documento a criar");
  preview_tabs_.set_scrollable(true); preview_tabs_.set_current_page(1);
  root_.append(horizontal_); status_.set_wrap(true); status_.set_xalign(0); root_.append(status_);
  for (int i = 0; i < 2; ++i) {
    project::Document d; d.id = project::new_uuid(); d.created_at = d.updated_at = project::utc_now();
    d.title = i ? "Exemplo — a carta" : "Exemplo — a chegada";
    d.content = i ? "A carta dizia: volte antes da chuva.\nEla reconheceu a letra da mãe.\n" :
        "Lia chegou à estação vazia.\nNa plataforma, encontrou uma chave.\nNinguém soube explicar quem a deixara ali.\n";
    d.formatting.push_back({project::DocumentTextStyle::Bold, 0, 3, {}}); examples_.push_back(d); sources_.emplace(d.id, d);
  }
  wire_commands(); source_page(); render(); install_keyboard();
}
BlocksLabWindow::~BlocksLabWindow() = default;
}
