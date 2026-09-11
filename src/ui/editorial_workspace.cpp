#include "inde/ui/editorial_workspace.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <filesystem>
#include <gdkmm/pixbuf.h>
#include <gdkmm/texture.h>

namespace inde::ui {
namespace {

std::optional<std::filesystem::path>
resolve_cover_path(const std::filesystem::path &project_path,
                   const std::string &stored) {
  if (stored.empty())
    return std::nullopt;
  std::filesystem::path candidate{stored};
  if (candidate.is_absolute() && std::filesystem::is_regular_file(candidate))
    return candidate;
  candidate = project_path / stored;
  if (std::filesystem::is_regular_file(candidate))
    return candidate;
  candidate = project_path.parent_path() / stored;
  if (std::filesystem::is_regular_file(candidate))
    return candidate;
  return std::nullopt;
}

Gtk::Widget *make_cover(const std::filesystem::path &project_path,
                        const std::string &stored, int width, int height) {
  const auto resolved = resolve_cover_path(project_path, stored);
  if (resolved) {
    try {
      // A textura reduzida também reduz o tamanho natural reportado ao FlowBox.
      // Carregar a capa original diretamente fazia imagens grandes alargarem o
      // card, apesar do size request do widget.
      const auto pixbuf = Gdk::Pixbuf::create_from_file(resolved->string(),
                                                        width, height, false);
      auto *picture = Gtk::make_managed<Gtk::Picture>();
      picture->set_paintable(Gdk::Texture::create_for_pixbuf(pixbuf));
      picture->set_content_fit(Gtk::ContentFit::COVER);
      picture->set_can_shrink(true);
      picture->set_size_request(width, height);
      picture->add_css_class("catalog-cover");
      return picture;
    } catch (const Glib::Error &) {
      // Um caminho existente mas ilegível usa o mesmo fallback honesto de um
      // caminho ausente, sem impedir a abertura do Catálogo.
    }
  }
  auto *placeholder = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
  placeholder->set_size_request(width, height);
  placeholder->set_halign(Gtk::Align::CENTER);
  placeholder->set_valign(Gtk::Align::CENTER);
  placeholder->add_css_class("catalog-cover-placeholder");
  auto *icon = Gtk::make_managed<Gtk::Image>();
  icon->set_from_icon_name("image-x-generic-symbolic");
  icon->set_pixel_size(32);
  icon->set_tooltip_text(stored.empty()
                             ? "Sem capa definida"
                             : "Arquivo de capa não encontrado: " + stored);
  placeholder->append(*icon);
  return placeholder;
}

} // namespace

EditorialWorkspace::EditorialWorkspace(application::ProjectService &service)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 12), service_(service) {
  build_ui();
}

void EditorialWorkspace::build_ui() {
  set_vexpand(true);
  set_hexpand(true);
  set_margin(16);
  add_css_class("workspace-page");
  catalog_page_.set_vexpand(true);
  catalog_page_.set_hexpand(true);
  append(catalog_page_);
  catalog_scroll_.set_child(catalog_content_);
  catalog_scroll_.set_policy(Gtk::PolicyType::NEVER,
                             Gtk::PolicyType::AUTOMATIC);
  catalog_scroll_.set_propagate_natural_width(false);
  catalog_scroll_.set_vexpand(true);
  catalog_content_.set_margin(4);
  catalog_content_.set_hexpand(true);
  catalog_title_.add_css_class("title-1");
  catalog_title_.add_css_class("page-title");
  catalog_title_.set_hexpand(true);
  catalog_title_.set_halign(Gtk::Align::START);
  catalog_header_.append(catalog_title_);
  catalog_header_.append(filters_button_);
  catalog_content_.append(catalog_header_);
  filters_panel_.set_margin(14);
  catalog_search_.set_placeholder_text("Pesquisar IPs e Obras");
  set_accessible_label(catalog_search_, "Pesquisar IPs e Obras");
  set_accessible_description(
      catalog_search_,
      "Filtra propriedades intelectuais e obras exibidas neste catálogo.");
  filters_panel_.append(catalog_search_);
  auto *filter_actions =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
  apply_filters_button_.add_css_class("suggested-action");
  filter_actions->append(clear_filters_button_);
  filter_actions->append(apply_filters_button_);
  filters_panel_.append(*filter_actions);
  filters_surface_.set_child(filters_panel_);
  filters_surface_.add_css_class("filter-sheet");
  filters_revealer_.set_child(filters_surface_);
  filters_revealer_.set_transition_type(
      Gtk::RevealerTransitionType::SLIDE_DOWN);
  set_overlay_revealer_open(filters_revealer_, false);
  catalog_content_.append(filters_revealer_);
  catalog_content_.append(navigation_panel_);
  catalog_page_.append(catalog_scroll_);
  navigation_panel_.set_hexpand(true);
  project_name_.add_css_class("title-1");
  project_name_.add_css_class("page-title");
  project_details_.add_css_class("dim-label");
  project_details_.add_css_class("page-subtitle");
  ip_title_.add_css_class("title-2");
  work_title_.add_css_class("title-2");
  ip_title_.add_css_class("section-title");
  work_title_.add_css_class("section-title");
  ip_title_.set_halign(Gtk::Align::START);
  work_title_.set_halign(Gtk::Align::START);
  ip_toolbar_.add_css_class("toolbar-surface");
  work_toolbar_.add_css_class("toolbar-surface");
  navigation_panel_.append(project_name_);
  navigation_panel_.append(project_details_);
  navigation_panel_.append(ip_panel_);
  navigation_panel_.append(work_panel_);
  ip_panel_.set_hexpand(true);
  work_panel_.set_hexpand(true);
  ip_panel_.append(ip_title_);
  ip_panel_.append(ip_toolbar_);
  ip_toolbar_.append(add_ip_button_);
  ip_toolbar_.append(ip_actions_button_);
  work_panel_.append(work_title_);
  work_panel_.append(work_toolbar_);
  work_toolbar_.append(add_work_button_);
  work_toolbar_.append(work_actions_button_);
  configure_overflow_button(ip_actions_button_,
                            {{"Editar propriedade intelectual",
                              [this] {
                                if (selected_ip_id_)
                                  edit_intellectual_property(*selected_ip_id_);
                              },
                              "document-edit-symbolic"},
                             {"Remover propriedade intelectual",
                              [this] {
                                if (selected_ip_id_)
                                  delete_intellectual_property(
                                      *selected_ip_id_);
                              },
                              "user-trash-symbolic", true, true, true}},
                            "Ações da propriedade intelectual selecionada");
  configure_overflow_button(work_actions_button_,
                            {{"Abrir estrutura editorial",
                              [this] {
                                if (selected_work_id_)
                                  select_work_structure(*selected_work_id_);
                              },
                              "go-next-symbolic"},
                             {"Editar obra",
                              [this] {
                                if (selected_work_id_)
                                  edit_work(*selected_work_id_);
                              },
                              "document-edit-symbolic"},
                             {"Remover obra",
                              [this] {
                                if (selected_work_id_)
                                  delete_work(*selected_work_id_);
                              },
                              "user-trash-symbolic", true, true, true}},
                            "Ações da obra selecionada");
  for (auto *empty : {&ip_empty_, &work_empty_}) {
    empty->set_halign(Gtk::Align::START);
    empty->set_xalign(0.0F);
    empty->set_margin_start(12);
    empty->set_margin_top(8);
    empty->set_margin_bottom(8);
    empty->add_css_class("page-subtitle");
  }
  ip_panel_.append(ip_empty_);
  work_panel_.append(work_empty_);
  ip_panel_.append(ip_list_);
  work_panel_.append(work_list_);
  for (auto *cards : {&ip_list_, &work_list_}) {
    cards->set_selection_mode(Gtk::SelectionMode::NONE);
    cards->set_min_children_per_line(1);
    cards->set_max_children_per_line(3);
    cards->set_column_spacing(12);
    cards->set_row_spacing(12);
    cards->set_homogeneous(true);
    cards->set_halign(Gtk::Align::START);
    cards->set_valign(Gtk::Align::START);
    cards->set_hexpand(false);
    cards->set_vexpand(false);
  }
  remove_ip_button_.add_css_class("destructive-action");
  remove_work_button_.add_css_class("destructive-action");
  filters_button_.signal_clicked().connect([this] {
    filters_visible_ = !filters_visible_;
    set_overlay_revealer_open(filters_revealer_, filters_visible_);
    filters_button_.set_label(filters_visible_ ? "Ocultar filtros" : "Filtros");
    if (filters_visible_)
      catalog_search_.grab_focus();
  });
  catalog_search_.signal_search_changed().connect(
      [this] { refresh_catalog(); });
  catalog_search_.signal_activate().connect([this] {
    filters_visible_ = false;
    set_overlay_revealer_open(filters_revealer_, false);
    filters_button_.set_label("Filtros");
    filters_button_.grab_focus();
  });
  clear_filters_button_.signal_clicked().connect([this] {
    catalog_search_.set_text("");
    refresh_catalog();
  });
  apply_filters_button_.signal_clicked().connect([this] {
    filters_visible_ = false;
    set_overlay_revealer_open(filters_revealer_, false);
    filters_button_.set_label("Filtros");
    filters_button_.grab_focus();
  });
}

void EditorialWorkspace::install_actions(Gtk::ApplicationWindow &window) {
  window.add_action(
      "create-ip",
      sigc::mem_fun(*this, &EditorialWorkspace::create_intellectual_property));
  window.add_action("create-work",
                    sigc::mem_fun(*this, &EditorialWorkspace::create_work));
  window.add_action("edit-ip", [this] {
    if (selected_ip_id_)
      edit_intellectual_property(*selected_ip_id_);
  });
  window.add_action("remove-ip", [this] {
    if (selected_ip_id_)
      delete_intellectual_property(*selected_ip_id_);
  });
  window.add_action("edit-work", [this] {
    if (selected_work_id_)
      edit_work(*selected_work_id_);
  });
  window.add_action("remove-work", [this] {
    if (selected_work_id_)
      delete_work(*selected_work_id_);
  });
  add_ip_button_.set_action_name("win.create-ip");
  add_work_button_.set_action_name("win.create-work");
  edit_ip_button_.set_action_name("win.edit-ip");
  remove_ip_button_.set_action_name("win.remove-ip");
  edit_work_button_.set_action_name("win.edit-work");
  remove_work_button_.set_action_name("win.remove-work");
}

void EditorialWorkspace::refresh() {
  const auto *current = service_.current();
  if (!current) {
    reset();
    return;
  }
  project_name_.set_text(current->manifest().name);
  const auto &catalog = service_.catalog();
  project_details_.set_text(
      std::to_string(catalog.intellectual_properties.size()) +
      (catalog.intellectual_properties.size() == 1
           ? " propriedade intelectual"
           : " propriedades intelectuais") +
      "  •  " + std::to_string(catalog.works.size()) +
      (catalog.works.size() == 1 ? " obra" : " obras") + "  •  Projeto INDE v" +
      std::to_string(current->manifest().format_version));
  project_details_.set_tooltip_text(current->path().string());
  project_details_.set_justify(Gtk::Justification::CENTER);
  refresh_catalog();
}

void EditorialWorkspace::reset() {
  selected_ip_id_.reset();
  selected_work_id_.reset();
  while (auto *child = ip_list_.get_first_child())
    ip_list_.remove(*child);
  while (auto *child = work_list_.get_first_child())
    work_list_.remove(*child);
  project_name_.set_text("");
  project_details_.set_text("");
}

void EditorialWorkspace::toggle_navigation() {
  filters_visible_ = !filters_visible_;
  set_overlay_revealer_open(filters_revealer_, filters_visible_);
  filters_button_.set_label(filters_visible_ ? "Ocultar filtros" : "Filtros");
  if (filters_visible_)
    catalog_search_.grab_focus();
}

void EditorialWorkspace::toggle_inspector() {
  // O catálogo não possui inspetor; a visualização detalhada está no Editorial.
}

Gtk::Window *EditorialWorkspace::owner_window() {
  return dynamic_cast<Gtk::Window *>(get_root());
}

void EditorialWorkspace::show_error(const Glib::ustring &title,
                                    const std::exception &error) {
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog(*owner, error.what(), false, Gtk::MessageType::ERROR,
                        Gtk::ButtonsType::CLOSE, true);
  dialog->set_title(title);
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->show();
}

void EditorialWorkspace::refresh_catalog() {
  while (auto *child = ip_list_.get_first_child())
    ip_list_.remove(*child);
  while (auto *child = work_list_.get_first_child())
    work_list_.remove(*child);
  const auto &catalog = service_.catalog();
  const auto project_path = service_.current()->path();
  if (selected_ip_id_ &&
      !std::any_of(catalog.intellectual_properties.begin(),
                   catalog.intellectual_properties.end(),
                   [&](const auto &ip) { return ip.id == *selected_ip_id_; }))
    selected_ip_id_.reset();
  if (selected_work_id_ &&
      !std::any_of(
          catalog.works.begin(), catalog.works.end(),
          [&](const auto &work) { return work.id == *selected_work_id_; }))
    selected_work_id_.reset();
  if (!selected_ip_id_ && !catalog.intellectual_properties.empty())
    selected_ip_id_ = catalog.intellectual_properties.front().id;
  const auto selected_ip =
      std::find_if(catalog.intellectual_properties.begin(),
                   catalog.intellectual_properties.end(), [&](const auto &ip) {
                     return selected_ip_id_ && ip.id == *selected_ip_id_;
                   });
  work_title_.set_text(selected_ip == catalog.intellectual_properties.end()
                           ? "Obras"
                           : "Obras de " + selected_ip->title);
  const auto set_available = [](Gtk::Button &button, bool available) {
    button.set_sensitive(available);
    button.remove_css_class(available ? "unavailable-action"
                                      : "available-action");
    button.add_css_class(available ? "available-action" : "unavailable-action");
  };
  set_available(add_work_button_, !catalog.intellectual_properties.empty());
  const auto search = catalog_search_.get_text().lowercase().raw();
  const auto matches_search = [&search](const std::string &text) {
    return search.empty() ||
           Glib::ustring(text).lowercase().find(search) != Glib::ustring::npos;
  };
  int ip_card_index = 0;
  for (const auto &ip : catalog.intellectual_properties) {
    if (!matches_search(ip.title) && !matches_search(ip.subtitle) &&
        !matches_search(ip.description))
      continue;
    auto *button = Gtk::make_managed<Gtk::Button>();
    auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 0);
    auto *labels = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    labels->set_margin(12);
    auto *title = Gtk::make_managed<Gtk::Label>(ip.title);
    auto *details = Gtk::make_managed<Gtk::Label>(
        ip.subtitle.empty() ? ip.description : ip.subtitle);
    title->add_css_class("heading");
    title->set_xalign(0.0F);
    title->set_ellipsize(Pango::EllipsizeMode::END);
    details->set_xalign(0.0F);
    details->set_wrap(true);
    details->set_lines(3);
    details->set_ellipsize(Pango::EllipsizeMode::END);
    details->add_css_class("dim-label");
    labels->append(*title);
    labels->append(*details);
    card->append(*make_cover(project_path, ip.cover_path, 92, 118));
    card->append(*labels);
    button->set_child(*card);
    button->set_tooltip_text(
        ip.title + "\n" +
        (ip.description.empty() ? "Propriedade intelectual" : ip.description));
    button->set_halign(Gtk::Align::START);
    button->set_valign(Gtk::Align::START);
    button->set_hexpand(false);
    button->set_vexpand(false);
    button->set_size_request(360, 118);
    button->add_css_class("card");
    button->add_css_class("catalog-card");
    if (selected_ip_id_ == ip.id)
      button->add_css_class("selected-card");
    button->signal_clicked().connect([this, id = ip.id] {
      selected_ip_id_ = id;
      selected_work_id_.reset();
      refresh_catalog();
    });
    attach_context_menu(
        *button,
        {{"Selecionar",
          [this, id = ip.id] {
            selected_ip_id_ = id;
            selected_work_id_.reset();
            refresh_catalog();
          },
          "object-select-symbolic"},
         {"Editar", [this, id = ip.id] { edit_intellectual_property(id); },
          "document-edit-symbolic"},
         {"Remover", [this, id = ip.id] { delete_intellectual_property(id); },
          "user-trash-symbolic", true, true, true}});
    ip_list_.append(*button);
    if (auto *card = ip_list_.get_child_at_index(ip_card_index++)) {
      card->set_halign(Gtk::Align::START);
      card->set_valign(Gtk::Align::START);
      card->set_size_request(360, 118);
    }
  }
  int work_card_index = 0;
  for (const auto &work : catalog.works) {
    if (selected_ip_id_ && work.intellectual_property_id != *selected_ip_id_)
      continue;
    if (!matches_search(work.title) && !matches_search(work.subtitle) &&
        !matches_search(work.synopsis))
      continue;
    auto *button = Gtk::make_managed<Gtk::Button>();
    auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 0);
    auto *labels = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    labels->set_margin(12);
    auto *title = Gtk::make_managed<Gtk::Label>(work.title);
    auto *details = Gtk::make_managed<Gtk::Label>(
        work.status + (work.subtitle.empty() ? "" : " — " + work.subtitle));
    title->add_css_class("heading");
    title->set_xalign(0.0F);
    title->set_ellipsize(Pango::EllipsizeMode::END);
    details->set_xalign(0.0F);
    details->set_wrap(true);
    details->set_lines(3);
    details->set_ellipsize(Pango::EllipsizeMode::END);
    details->add_css_class("dim-label");
    labels->append(*title);
    labels->append(*details);
    const auto parent_ip = std::find_if(
        catalog.intellectual_properties.begin(),
        catalog.intellectual_properties.end(), [&](const auto &value) {
          return value.id == work.intellectual_property_id;
        });
    card->append(*make_cover(project_path,
                             parent_ip == catalog.intellectual_properties.end()
                                 ? std::string{}
                                 : parent_ip->cover_path,
                             92, 118));
    card->append(*labels);
    button->set_child(*card);
    button->set_tooltip_text(
        work.title + "\n" +
        (work.synopsis.empty() ? "Abrir estrutura editorial" : work.synopsis));
    button->set_halign(Gtk::Align::START);
    button->set_valign(Gtk::Align::START);
    button->set_hexpand(false);
    button->set_vexpand(false);
    button->set_size_request(380, 118);
    button->add_css_class("card");
    button->add_css_class("catalog-card");
    if (selected_work_id_ == work.id)
      button->add_css_class("suggested-action");
    button->signal_clicked().connect(
        [this, id = work.id] { select_work_structure(id); });
    attach_context_menu(*button,
                        {{"Abrir estrutura editorial",
                          [this, id = work.id] { select_work_structure(id); },
                          "go-next-symbolic"},
                         {"Editar", [this, id = work.id] { edit_work(id); },
                          "document-edit-symbolic"},
                         {"Remover", [this, id = work.id] { delete_work(id); },
                          "user-trash-symbolic", true, true, true}});
    work_list_.append(*button);
    if (auto *card = work_list_.get_child_at_index(work_card_index++)) {
      card->set_halign(Gtk::Align::START);
      card->set_valign(Gtk::Align::START);
      card->set_size_request(380, 118);
    }
  }
  ip_empty_.set_text(
      search.empty()
          ? "Nenhuma propriedade intelectual ainda. Crie a primeira para "
            "organizar suas obras."
          : "Nenhuma propriedade intelectual corresponde à pesquisa.");
  ip_empty_.set_visible(ip_card_index == 0);
  work_empty_.set_text(
      catalog.intellectual_properties.empty()
          ? "Crie uma propriedade intelectual antes de adicionar uma obra."
          : (search.empty()
                 ? "Nenhuma obra cadastrada nesta propriedade intelectual."
                 : "Nenhuma obra corresponde à pesquisa neste contexto."));
  work_empty_.set_visible(work_card_index == 0);
  set_available(edit_ip_button_, selected_ip_id_.has_value());
  set_available(remove_ip_button_, selected_ip_id_.has_value());
  set_available(edit_work_button_, selected_work_id_.has_value());
  set_available(remove_work_button_, selected_work_id_.has_value());
  ip_actions_button_.set_sensitive(selected_ip_id_.has_value());
  work_actions_button_.set_sensitive(selected_work_id_.has_value());
}

void EditorialWorkspace::create_intellectual_property() {
  auto *dialog =
      new OverlayDialog("Nova propriedade intelectual", *owner_window(), true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *title = Gtk::make_managed<Gtk::Entry>();
  title->set_placeholder_text("Título (obrigatório)");
  auto *subtitle = Gtk::make_managed<Gtk::Entry>();
  subtitle->set_placeholder_text("Subtítulo");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição");
  auto *cover = Gtk::make_managed<Gtk::Entry>();
  cover->set_placeholder_text("Caminho da capa (opcional)");
  form->set_margin(16);
  append_labeled_form_field(*form, "Título da propriedade", *title,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Subtítulo", *subtitle);
  append_labeled_form_field(*form, "Descrição", *description);
  append_labeled_form_field(
      *form, "Caminho da capa", *cover,
      "Aceita um caminho absoluto ou relativo ao arquivo do projeto.");
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, title, subtitle, description,
                                     cover](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        service_.create_intellectual_property(
            title->get_text(), subtitle->get_text(), description->get_text(),
            cover->get_text());
        refresh_catalog();
      } catch (const std::exception &error) {
        show_error("Não foi possível criar a propriedade intelectual", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void EditorialWorkspace::create_work() {
  const auto &catalog = service_.catalog();
  if (catalog.intellectual_properties.empty())
    return;
  auto *dialog = new OverlayDialog("Nova obra", *owner_window(), true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *ip = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : catalog.intellectual_properties)
    ip->append(value.id, value.title);
  ip->set_active(0);
  auto *title = Gtk::make_managed<Gtk::Entry>();
  title->set_placeholder_text("Título (obrigatório)");
  auto *subtitle = Gtk::make_managed<Gtk::Entry>();
  subtitle->set_placeholder_text("Subtítulo");
  auto *synopsis = Gtk::make_managed<Gtk::Entry>();
  synopsis->set_placeholder_text("Sinopse");
  auto *language = Gtk::make_managed<Gtk::Entry>();
  language->set_text("pt-BR");
  auto *status = Gtk::make_managed<Gtk::ComboBoxText>();
  status->append("Planejamento");
  status->append("Em escrita");
  status->append("Revisão");
  status->append("Concluída");
  status->set_active(0);
  form->set_margin(16);
  append_labeled_form_field(*form, "Propriedade intelectual", *ip,
                            "Define a IP à qual a obra pertence.");
  append_labeled_form_field(*form, "Título da obra", *title,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Subtítulo", *subtitle);
  append_labeled_form_field(*form, "Sinopse", *synopsis);
  append_labeled_form_field(*form, "Idioma", *language,
                            "Código de idioma da obra.");
  append_labeled_form_field(*form, "Situação editorial", *status);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, ip, title, subtitle,
                                     synopsis, language, status](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        service_.create_work(ip->get_active_id(), title->get_text(),
                             subtitle->get_text(), synopsis->get_text(),
                             language->get_text(), status->get_active_text());
        refresh_catalog();
      } catch (const std::exception &error) {
        show_error("Não foi possível criar a obra", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void EditorialWorkspace::edit_intellectual_property(std::string id) {
  const auto &values = service_.catalog().intellectual_properties;
  const auto found =
      std::find_if(values.begin(), values.end(),
                   [&](const auto &value) { return value.id == id; });
  if (found == values.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar propriedade intelectual",
                                   *owner_window(), true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *title = Gtk::make_managed<Gtk::Entry>();
  title->set_text(original.title);
  auto *subtitle = Gtk::make_managed<Gtk::Entry>();
  subtitle->set_text(original.subtitle);
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(original.description);
  auto *cover = Gtk::make_managed<Gtk::Entry>();
  cover->set_text(original.cover_path);
  cover->set_placeholder_text("Caminho da capa (opcional)");
  form->set_margin(16);
  append_labeled_form_field(*form, "Título da propriedade", *title,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Subtítulo", *subtitle);
  append_labeled_form_field(*form, "Descrição", *description);
  append_labeled_form_field(*form, "Caminho da capa", *cover, "Opcional.");
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, title, subtitle,
                                     description, cover](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        original.title = title->get_text().raw();
        original.subtitle = subtitle->get_text().raw();
        original.description = description->get_text().raw();
        original.cover_path = cover->get_text().raw();
        service_.update_intellectual_property(original);
        refresh_catalog();
      } catch (const std::exception &error) {
        show_error("Não foi possível editar a propriedade intelectual", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void EditorialWorkspace::edit_work(std::string id) {
  const auto &values = service_.catalog().works;
  const auto found =
      std::find_if(values.begin(), values.end(),
                   [&](const auto &value) { return value.id == id; });
  if (found == values.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar obra", *owner_window(), true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *title = Gtk::make_managed<Gtk::Entry>();
  title->set_text(original.title);
  auto *subtitle = Gtk::make_managed<Gtk::Entry>();
  subtitle->set_text(original.subtitle);
  auto *synopsis = Gtk::make_managed<Gtk::Entry>();
  synopsis->set_text(original.synopsis);
  auto *language = Gtk::make_managed<Gtk::Entry>();
  language->set_text(original.language);
  auto *status = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto *value :
       {"Planejamento", "Em escrita", "Revisão", "Concluída"})
    status->append(value);
  status->set_active_text(original.status);
  form->set_margin(16);
  append_labeled_form_field(*form, "Título da obra", *title,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Subtítulo", *subtitle);
  append_labeled_form_field(*form, "Sinopse", *synopsis);
  append_labeled_form_field(*form, "Idioma", *language,
                            "Código de idioma da obra.");
  append_labeled_form_field(*form, "Situação editorial", *status);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, title, subtitle,
                                     synopsis, language,
                                     status](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        original.title = title->get_text().raw();
        original.subtitle = subtitle->get_text().raw();
        original.synopsis = synopsis->get_text().raw();
        original.language = language->get_text().raw();
        original.status = status->get_active_text().raw();
        service_.update_work(original);
        refresh_catalog();
      } catch (const std::exception &error) {
        show_error("Não foi possível editar a obra", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void EditorialWorkspace::delete_intellectual_property(std::string id) {
  auto *dialog = new OverlayDialog(
      *owner_window(), "Remover esta propriedade intelectual?", false,
      Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);
  dialog->set_secondary_text("A operação não pode ser desfeita. IPs com obras "
                             "vinculadas são protegidas.");
  dialog->signal_response().connect([this, dialog,
                                     id = std::move(id)](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.delete_intellectual_property(id);
        refresh_catalog();
      } catch (const std::exception &error) {
        show_error("Não foi possível remover a propriedade intelectual", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void EditorialWorkspace::delete_work(std::string id) {
  auto *dialog = new OverlayDialog(*owner_window(), "Remover esta obra?", false,
                                   Gtk::MessageType::QUESTION,
                                   Gtk::ButtonsType::YES_NO, true);
  dialog->set_secondary_text("A operação não pode ser desfeita.");
  dialog->signal_response().connect(
      [this, dialog, id = std::move(id)](int response) {
        if (response == Gtk::ResponseType::YES) {
          try {
            service_.delete_work(id);
            refresh_catalog();
          } catch (const std::exception &error) {
            show_error("Não foi possível remover a obra", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void EditorialWorkspace::select_work_structure(std::string work_id) {
  selected_work_id_ = std::move(work_id);
  refresh_catalog();
  signal_open_work_requested_.emit(*selected_work_id_);
}

} // namespace inde::ui
