#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/editorial_structure_workspace.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/bounded_choices.hpp"
#include "inde/ui/work_setup.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <sstream>

namespace inde::ui {

EditorialStructureWorkspace::EditorialStructureWorkspace(
    application::ProjectService &service)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 12), service_(service),
      structural_editor_(service),production_(service) {
  set_vexpand(true);
  set_hexpand(true);
  set_margin(16);
  add_css_class("workspace-page");
  title_.add_css_class("title-1");
  title_.set_halign(Gtk::Align::START);
  hint_.set_halign(Gtk::Align::START);
  hint_.set_wrap(true);
  hint_.add_css_class("dim-label");
  header_.append(title_);
  header_.append(hint_);
  library_header_.append(header_);
  header_.set_hexpand(true);
  library_header_.append(work_combo_);
  set_accessible_label(work_combo_, "Obra do manuscrito");
  work_combo_.set_tooltip_text("Obra do manuscrito");
  // Títulos autorais longos não podem impor a largura mínima da janela.
  bound_choice_width(work_combo_);
  library_header_.append(back_to_library_);
  library_header_.append(back_to_manuscript_);
  library_header_.append(production_button_);
  append(library_header_);
  setup_work_button_.set_halign(Gtk::Align::START);
  setup_work_button_.add_css_class("suggested-action");
  append(setup_work_button_);
  setup_work_button_.signal_clicked().connect([this] {
    if (auto *owner=owner_window())
      show_work_setup(*owner,service_,[this](const std::string &id) { reveal_work(id); });
  });
  pages_.set_hexpand(true);
  pages_.set_vexpand(true);
  // A biblioteca e o editor têm composições distintas. Manter a pilha
  // homogênea faria a página oculta aumentar a altura mínima do workspace.
  pages_.set_hhomogeneous(false);
  pages_.set_vhomogeneous(false);
  pages_.set_transition_type(Gtk::StackTransitionType::SLIDE_LEFT_RIGHT);
  append(pages_);

  library_toolbar_.set_selection_mode(Gtk::SelectionMode::NONE);
  library_toolbar_.set_column_spacing(6);
  library_toolbar_.set_row_spacing(6);
  library_toolbar_.set_min_children_per_line(1);
  library_toolbar_.set_max_children_per_line(8);
  library_toolbar_.set_halign(Gtk::Align::FILL);
  library_toolbar_.append(create_structure_button_);
  library_toolbar_.append(instantiate_button_);
  library_toolbar_.append(open_button_);
  alternatives_actions_.set_label("Ações da alternativa");
  alternatives_options_.set_margin(10);
  for (auto *button : {&activate_button_, &duplicate_button_, &derive_button_,
                       &capture_button_, &delete_structure_button_})
    alternatives_options_.append(*button);
  delete_structure_button_.add_css_class("destructive-action");
  alternatives_popover_.set_child(alternatives_options_);
  alternatives_actions_.set_popover(alternatives_popover_);
  library_toolbar_.append(alternatives_actions_);
  library_page_.append(library_toolbar_);

  auto *models_heading = Gtk::make_managed<Gtk::Label>("Modelos reutilizáveis");
  models_heading->set_halign(Gtk::Align::START);
  models_heading->add_css_class("heading");
  models_panel_.append(*models_heading);
  models_panel_.append(delete_model_button_);
  models_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  models_list_.add_css_class("boxed-list");
  models_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  models_scroll_.set_child(models_list_);
  models_scroll_.set_vexpand(true);
  models_panel_.append(models_scroll_);

  auto *structures_heading =
      Gtk::make_managed<Gtk::Label>("Alternativas desta Obra");
  structures_heading->set_halign(Gtk::Align::START);
  structures_heading->add_css_class("heading");
  structures_panel_.append(*structures_heading);
  structures_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  structures_list_.add_css_class("boxed-list");
  structures_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                Gtk::PolicyType::AUTOMATIC);
  structures_scroll_.set_child(structures_list_);
  structures_scroll_.set_vexpand(true);
  structures_panel_.append(structures_scroll_);
  library_views_.set_vexpand(true);
  library_views_.set_hhomogeneous(false);
  library_views_.set_vhomogeneous(false);
  library_views_.add(structures_panel_, "alternatives", "Alternativas da obra");
  library_views_.add(models_panel_, "models", "Modelos reutilizáveis");
  library_switcher_.set_stack(library_views_);
  library_views_.property_visible_child_name().signal_changed().connect([this] {
    const bool alternatives = library_views_.get_visible_child_name() == "alternatives";
    alternatives_popover_.popdown();
    open_button_.set_visible(alternatives);
    alternatives_actions_.set_visible(alternatives);
  });
  library_switcher_.set_halign(Gtk::Align::START);
  library_page_.append(library_switcher_);
  library_page_.append(library_views_);
  pages_.add(library_page_, "library");

  auto *editor_page =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  editor_page->append(structural_editor_);
  pages_.add(*editor_page, "editor");
  pages_.add(production_,"production");
  production_button_.signal_clicked().connect(sigc::mem_fun(*this,&EditorialStructureWorkspace::show_production));
  structural_editor_.signal_status_message().connect(
      [this](const Glib::ustring &message) {
        signal_status_message_.emit(message);
      });
  structural_editor_.signal_open_document_requested().connect(
      [this](const std::string &id) {
        signal_open_document_requested_.emit(id);
      });
  structural_editor_.signal_create_document_requested().connect(
      [this](const std::string &id) {
        signal_create_document_requested_.emit(id);
      });
  structural_editor_.signal_filter_documents_requested().connect(
      [this](const std::string &id) {
        signal_filter_documents_requested_.emit(id);
      });

  work_combo_.signal_changed().connect([this] {
    if (refreshing_work_) return;
    const auto id = work_combo_.get_active_id().raw();
    if (selected_work_id_ == (id.empty() ? std::nullopt : std::optional{id})) return;
    selected_work_id_ = id.empty() ? std::nullopt : std::optional{id};
    selected_structure_id_.reset();
    if (pages_.get_visible_child_name() == "editor") show_editor();
    else if(pages_.get_visible_child_name()=="production")show_production();
    else refresh_library();
  });
  structures_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_structure_id_ =
        row && row->get_index() >= 0 &&
                static_cast<std::size_t>(row->get_index()) <
                    structure_row_ids_.size()
            ? std::optional{structure_row_ids_[row->get_index()]}
            : std::nullopt;
    const bool selected = selected_structure_id_.has_value();
    duplicate_button_.set_sensitive(selected);
    derive_button_.set_sensitive(selected);
    capture_button_.set_sensitive(selected);
    activate_button_.set_sensitive(selected);
    open_button_.set_sensitive(selected);
    delete_structure_button_.set_sensitive(selected);
  });
  models_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_model_id_ = row && row->get_index() >= 0 &&
                                 static_cast<std::size_t>(row->get_index()) <
                                     model_row_ids_.size()
                             ? std::optional{model_row_ids_[row->get_index()]}
                             : std::nullopt;
    if (!selected_model_id_) {
      delete_model_button_.set_sensitive(false);
      return;
    }
    const auto models =
        service_.structures().models(project::StructureLayer::Editorial);
    const auto found =
        std::find_if(models.begin(), models.end(), [&](const auto &value) {
          return value.id == *selected_model_id_;
        });
    delete_model_button_.set_sensitive(found != models.end() &&
                                       !found->is_builtin);
  });
  structures_list_.signal_row_activated().connect(
      [this](Gtk::ListBoxRow *) { open_structure(); });
  back_to_library_.signal_clicked().connect([this] { show_library(); });
  back_to_manuscript_.signal_clicked().connect([this] { show_editor(); });
  create_structure_button_.signal_clicked().connect(sigc::mem_fun(
      *this, &EditorialStructureWorkspace::create_blank_structure));
  instantiate_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &EditorialStructureWorkspace::instantiate_model));
  duplicate_button_.signal_clicked().connect(
      [this] { duplicate_structure(false); });
  derive_button_.signal_clicked().connect(
      [this] { duplicate_structure(true); });
  capture_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &EditorialStructureWorkspace::capture_model));
  activate_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &EditorialStructureWorkspace::activate_structure));
  open_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &EditorialStructureWorkspace::open_structure));
  delete_structure_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &EditorialStructureWorkspace::delete_structure));
  delete_model_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &EditorialStructureWorkspace::delete_model));
  show_editor();
}

void EditorialStructureWorkspace::install_actions(
    Gtk::ApplicationWindow &window) {
  structural_editor_.install_actions(window);
}

void EditorialStructureWorkspace::refresh() {
  if (!service_.current()) {
    reset();
    return;
  }
  const auto &works = service_.catalog().works;
  setup_work_button_.set_visible(works.empty());
  work_combo_.set_visible(!works.empty());
  if (works.empty()) hint_.set_text("Para organizar o manuscrito ou produzir uma saída, comece uma Obra aqui. Seu universo será criado junto, sem textos ou capítulos de exemplo.");
  if (selected_work_id_ &&
      std::none_of(works.begin(), works.end(), [this](const auto &work) {
        return work.id == *selected_work_id_;
      }))
    selected_work_id_.reset();
  const auto wanted = selected_work_id_.value_or("");
  refreshing_work_ = true;
  work_combo_.remove_all();
  for (const auto &work : works)
    work_combo_.append(work.id, work.title);
  if (!wanted.empty())
    work_combo_.set_active_id(wanted);
  if (work_combo_.get_active_row_number() < 0 && !works.empty())
    work_combo_.set_active(0);
  const auto active = work_combo_.get_active_id().raw();
  selected_work_id_ = active.empty() ? std::nullopt : std::optional{active};
  refreshing_work_ = false;
  create_structure_button_.set_sensitive(selected_work_id_.has_value());
  instantiate_button_.set_sensitive(selected_work_id_.has_value());
  if (pages_.get_visible_child_name() == "editor")
    structural_editor_.set_work(selected_work_id_);
  else if(pages_.get_visible_child_name()=="production")production_.set_work(selected_work_id_);
  else
    refresh_library();
}

void EditorialStructureWorkspace::reset() {
  setup_work_button_.set_visible(false);
  production_.reset();
  selected_work_id_.reset();
  selected_structure_id_.reset();
  selected_model_id_.reset();
  refreshing_work_ = true;
  work_combo_.remove_all();
  refreshing_work_ = false;
  create_structure_button_.set_sensitive(false);
  instantiate_button_.set_sensitive(false);
  while (auto *child = models_list_.get_first_child())
    models_list_.remove(*child);
  while (auto *child = structures_list_.get_first_child())
    structures_list_.remove(*child);
  structural_editor_.reset();
  show_editor();
}
void EditorialStructureWorkspace::reveal_removal_use(const project::RemovalUse &use) {
  if (!use.work_id.empty()) reveal_work(use.work_id);
  show_library(); refresh_library();
  auto select = [](Gtk::ListBox &list, const auto &ids, const std::string &id) {
    for (std::size_t i = 0; i < ids.size(); ++i)
      if (ids[i] == id) { list.select_row(*list.get_row_at_index(static_cast<int>(i))); return; }
    throw std::runtime_error("Item de origem não encontrado na biblioteca");
  };
  if (use.location_table == "structure_models") {
    library_views_.set_visible_child("models"); select(models_list_, model_row_ids_, use.location_id);
  } else {
    library_views_.set_visible_child("alternatives"); select(structures_list_, structure_row_ids_, use.location_id);
  }
}

void EditorialStructureWorkspace::reveal_work(const std::string &work_id) {
  selected_work_id_ = work_id;
  refresh();
  show_editor();
}

void EditorialStructureWorkspace::reveal_node(const std::string &node_id) {
  if (!service_.current())
    return;
  for (const auto &work : service_.catalog().works) {
    const auto nodes = service_.structural_nodes_for_work(work.id);
    const auto found =
        std::find_if(nodes.begin(), nodes.end(),
                     [&](const auto &node) { return node.id == node_id; });
    if (found == nodes.end())
      continue;
    selected_work_id_ = work.id;
    refresh();
    show_editor();
    structural_editor_.reveal_node(node_id);
    return;
  }
}

void EditorialStructureWorkspace::toggle_inspector() {
  structural_editor_.toggle_inspector();
}

void EditorialStructureWorkspace::show_library() {
  production_button_.set_visible(true);
  pages_.set_visible_child("library");
  back_to_library_.set_visible(false);
  back_to_manuscript_.set_visible(true);
  title_.set_text("Modelos de organização do manuscrito");
  hint_.set_text("Compare alternativas por Obra. Modelo reutilizável, "
                 "organização criada com o modelo e organização em uso permanecem distintas.");
  if (service_.current() && service_.catalog().works.empty())
    hint_.set_text("Comece uma Obra aqui para aplicar e comparar estruturas. Seu universo será criado junto, sem textos ou capítulos de exemplo.");
  refresh_library();
}

void EditorialStructureWorkspace::show_editor() {
  production_button_.set_visible(true);
  pages_.set_visible_child("editor");
  back_to_library_.set_visible(true);
  back_to_manuscript_.set_visible(false);
  title_.set_text("Manuscrito");
  hint_.set_text("Organize as unidades da obra e abra seus documentos para escrever.");
  if (service_.current() && service_.catalog().works.empty())
    hint_.set_text("Comece uma Obra aqui para organizar o manuscrito. Seu universo será criado junto, sem textos ou capítulos de exemplo.");
  structural_editor_.set_work(selected_work_id_);
  structural_editor_.show_structure();
}

void EditorialStructureWorkspace::show_production() {
  pages_.set_visible_child("production");back_to_library_.set_visible(false);back_to_manuscript_.set_visible(true);production_button_.set_visible(false);
  title_.set_text("Produção da Obra");hint_.set_text("Prepare uma saída independente. Seleção de textos, organização do Projeto e aprovação editorial continuam distintas.");
  if (service_.current() && service_.catalog().works.empty())
    hint_.set_text("Comece uma Obra aqui para preparar uma saída. Seu universo será criado junto, sem textos ou capítulos de exemplo.");
  production_.set_work(selected_work_id_);
}

void EditorialStructureWorkspace::refresh_library() {
  const auto wanted_structure = selected_structure_id_;
  const auto wanted_model = selected_model_id_;
  while (auto *child = models_list_.get_first_child())
    models_list_.remove(*child);
  while (auto *child = structures_list_.get_first_child())
    structures_list_.remove(*child);
  model_row_ids_.clear();
  structure_row_ids_.clear();
  selected_model_id_.reset();
  selected_structure_id_.reset();
  for (auto *button : {&duplicate_button_, &derive_button_, &capture_button_,
                       &activate_button_, &open_button_, &delete_structure_button_})
    button->set_sensitive(false);
  delete_model_button_.set_sensitive(false);
  create_structure_button_.set_sensitive(selected_work_id_.has_value());
  instantiate_button_.set_sensitive(selected_work_id_.has_value());
  if (!service_.current())
    return;
  try {
    for (const auto &value :
         service_.structures().models(project::StructureLayer::Editorial)) {
      auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
      box->set_margin(8);
      auto *name = Gtk::make_managed<Gtk::Label>(value.name);
      name->set_halign(Gtk::Align::START);
      name->add_css_class("heading");
      auto *detail = Gtk::make_managed<Gtk::Label>(
          std::string(value.is_builtin ? "Sistema" : "Usuário") + " · " +
          value.description);
      detail->set_halign(Gtk::Align::START);
      detail->set_xalign(0.0F);
      detail->set_wrap(true);
      detail->add_css_class("dim-label");
      box->append(*name);
      box->append(*detail);
      models_list_.append(*box);
      model_row_ids_.push_back(value.id);
    }
    if (selected_work_id_) for (const auto &value :
         service_.structures().editorial_structures(*selected_work_id_)) {
      const auto nodes = service_.structures().editorial_node_count(value.id);
      auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
      box->set_margin(8);
      auto *name = Gtk::make_managed<Gtk::Label>(
          value.name + (value.is_active ? " · ATIVA" : ""));
      name->set_halign(Gtk::Align::START);
      name->add_css_class("heading");
      std::string origin = project::to_string(value.creation_kind);
      auto *detail =
          Gtk::make_managed<Gtk::Label>(origin + " · " + std::to_string(nodes) +
                                        " elementos\n" + value.description);
      detail->set_halign(Gtk::Align::START);
      detail->set_xalign(0.0F);
      detail->set_wrap(true);
      detail->add_css_class("dim-label");
      box->append(*name);
      box->append(*detail);
      structures_list_.append(*box);
      structure_row_ids_.push_back(value.id);
    }
  } catch (const std::exception &error) {
    show_error(error);
  }
  for (std::size_t i = 0; i < structure_row_ids_.size(); ++i)
    if (wanted_structure == structure_row_ids_[i])
      structures_list_.select_row(*structures_list_.get_row_at_index(static_cast<int>(i)));
  for (std::size_t i = 0; i < model_row_ids_.size(); ++i)
    if (wanted_model == model_row_ids_[i])
      models_list_.select_row(*models_list_.get_row_at_index(static_cast<int>(i)));
}

void EditorialStructureWorkspace::delete_model() {
  if (!selected_model_id_ || !owner_window())
    return;
  const auto id = *selected_model_id_;
  confirm_project_removal(*owner_window(), service_, project::RemovalKind::StructureModel, id, [this] {
      selected_model_id_.reset();
      refresh_library();
      signal_status_message_.emit("Modelo editorial do usuário removido.");
  });
}

Gtk::Window *EditorialStructureWorkspace::owner_window() {
  return dynamic_cast<Gtk::Window *>(get_root());
}

void EditorialStructureWorkspace::show_error(const std::exception &error) {
  if (auto *owner = owner_window()) {
    auto *dialog =
        new OverlayDialog(*owner, author::diagnostic(error.what()), false, Gtk::MessageType::ERROR,
                          Gtk::ButtonsType::CLOSE, true);
    dialog->present();
  }
}

void EditorialStructureWorkspace::create_blank_structure() {
  if (!selected_work_id_ || !owner_window())
    return;
  auto *dialog =
      new OverlayDialog("Nova organização do manuscrito", *owner_window(), true);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome da alternativa");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição opcional");
  dialog->get_content_area()->append(*name);
  dialog->get_content_area()->append(*description);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  const auto work = *selected_work_id_;
  dialog->signal_response().connect(
      [this, work, name, description](int response) {
        if (response != Gtk::ResponseType::ACCEPT)
          return;
        try {
          static_cast<void>(service_.structures().create_editorial(
              work, name->get_text(), description->get_text()));
          refresh_library();
          signal_status_message_.emit(
              "Organização do manuscrito vazia criada sem alterar a ativa.");
        } catch (const std::exception &error) {
          show_error(error);
        }
      });
  dialog->present();
}

void EditorialStructureWorkspace::instantiate_model() {
  if (!selected_work_id_ || !owner_window())
    return;
  const auto models =
      service_.structures().models(project::StructureLayer::Editorial);
  auto *dialog =
      new OverlayDialog("Aplicar modelo editorial", *owner_window(), true);
  auto *model = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : models)
    model->append(value.id, value.name + (value.is_builtin ? " · Sistema"
                                                           : " · Usuário"));
  if (!models.empty())
    model->set_active(0);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome do novo plano");
  dialog->get_content_area()->append(*model);
  dialog->get_content_area()->append(*name);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Instanciar", Gtk::ResponseType::ACCEPT);
  const auto work = *selected_work_id_;
  dialog->signal_response().connect([this, work, model, name](int response) {
    if (response != Gtk::ResponseType::ACCEPT)
      return;
    try {
      static_cast<void>(service_.structures().instantiate_editorial(
          work, model->get_active_id().raw(), name->get_text()));
      refresh_library();
      signal_status_message_.emit("Modelo aplicado como organização independente "
                                  "independente; a ativa foi preservada.");
    } catch (const std::exception &error) {
      show_error(error);
    }
  });
  dialog->present();
}

void EditorialStructureWorkspace::duplicate_structure(bool derived) {
  if (!selected_structure_id_ || !owner_window())
    return;
  auto *dialog = new OverlayDialog(derived ? "Derivar organização do manuscrito"
                                           : "Duplicar organização do manuscrito",
                                   *owner_window(), true);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome da nova alternativa");
  dialog->get_content_area()->append(*name);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button(derived ? "Derivar" : "Duplicar",
                     Gtk::ResponseType::ACCEPT);
  const auto id = *selected_structure_id_;
  dialog->signal_response().connect([this, id, name, derived](int response) {
    if (response != Gtk::ResponseType::ACCEPT)
      return;
    try {
      static_cast<void>(service_.structures().duplicate_editorial(
          id, name->get_text(), derived));
      refresh_library();
      signal_status_message_.emit(
          derived ? "Organização do manuscrito derivada com origem e histórico."
                  : "Organização do manuscrito duplicada com novas identidades.");
    } catch (const std::exception &error) {
      show_error(error);
    }
  });
  dialog->present();
}

void EditorialStructureWorkspace::capture_model() {
  if (!selected_structure_id_ || !owner_window())
    return;
  auto *dialog =
      new OverlayDialog("Salvar estrutura como modelo", *owner_window(), true);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome do modelo reutilizável");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição");
  dialog->get_content_area()->append(*name);
  dialog->get_content_area()->append(*description);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar modelo", Gtk::ResponseType::ACCEPT);
  const auto id = *selected_structure_id_;
  dialog->signal_response().connect(
      [this, id, name, description](int response) {
        if (response != Gtk::ResponseType::ACCEPT)
          return;
        try {
          static_cast<void>(service_.structures().capture_editorial_model(
              id, name->get_text(), description->get_text()));
          refresh_library();
          signal_status_message_.emit(
              "Modelo reutilizável criado sem alterar a organização atual.");
        } catch (const std::exception &error) {
          show_error(error);
        }
      });
  dialog->present();
}

void EditorialStructureWorkspace::activate_structure() {
  if (!selected_structure_id_ || !owner_window())
    return;
  try {
    const auto impact = service_.structures().editorial_activation_impact(
        *selected_structure_id_);
    if (impact.current_structure_id == impact.target_structure_id) {
      signal_status_message_.emit("Esta organização do manuscrito já está ativa.");
      return;
    }
    auto *dialog =
        new OverlayDialog("Ativar organização do manuscrito", *owner_window(), true);
    dialog->set_secondary_text(
        "A troca não move nem apaga Documentos. " +
        std::to_string(impact.affected_documents) +
        " Documentos permanecem colocados na estrutura anterior e podem ficar "
        "fora da projeção ativa; o alvo possui " +
        std::to_string(impact.target_units) + " elementos.");
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    dialog->add_button("Ativar", Gtk::ResponseType::ACCEPT);
    const auto id = *selected_structure_id_;
    dialog->signal_response().connect([this, id](int response) {
      if (response != Gtk::ResponseType::ACCEPT)
        return;
      try {
        service_.structures().activate_editorial(id);
        refresh_library();
        signal_status_message_.emit("Organização do manuscrito ativa atualizada; nenhuma colocação foi convertida.");
      } catch (const std::exception &error) {
        show_error(error);
      }
    });
    dialog->present();
  } catch (const std::exception &error) {
    show_error(error);
  }
}

void EditorialStructureWorkspace::delete_structure() {
  if (!selected_structure_id_ || !owner_window())
    return;
  const auto id = *selected_structure_id_;
  confirm_project_removal(*owner_window(), service_, project::RemovalKind::EditorialStructure, id, [this] {
      selected_structure_id_.reset();
      refresh_library();
      signal_status_message_.emit("Organização do manuscrito alternativa removida.");
  });
}

void EditorialStructureWorkspace::open_structure() {
  if (!selected_structure_id_ || !selected_work_id_)
    return;
  try {
    const auto structures =
        service_.structures().editorial_structures(*selected_work_id_);
    const auto found = std::find_if(
        structures.begin(), structures.end(),
        [&](const auto &value) { return value.id == *selected_structure_id_; });
    if (found == structures.end())
      return;
    if (!found->is_active) {
      signal_status_message_.emit(
          "Ative a alternativa antes de editá-la; a abertura nunca troca a "
          "ativa silenciosamente.");
      return;
    }
    show_editor();
  } catch (const std::exception &error) {
    show_error(error);
  }
}

} // namespace inde::ui
