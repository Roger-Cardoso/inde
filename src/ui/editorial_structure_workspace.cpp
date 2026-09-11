#include "inde/ui/editorial_structure_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <sstream>

namespace inde::ui {

EditorialStructureWorkspace::EditorialStructureWorkspace(
    application::ProjectService &service)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 12), service_(service),
      structural_editor_(service) {
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
  append(library_header_);
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
  library_toolbar_.append(duplicate_button_);
  library_toolbar_.append(derive_button_);
  library_toolbar_.append(capture_button_);
  library_toolbar_.append(activate_button_);
  library_toolbar_.append(open_button_);
  library_toolbar_.append(delete_structure_button_);
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
  library_paned_.set_start_child(models_panel_);
  library_paned_.set_end_child(structures_panel_);
  library_paned_.set_position(360);
  library_paned_.set_vexpand(true);
  library_page_.append(library_paned_);
  pages_.add(library_page_, "library");

  auto *editor_page =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  back_to_library_.set_halign(Gtk::Align::START);
  editor_page->append(back_to_library_);
  editor_page->append(structural_editor_);
  pages_.add(*editor_page, "editor");
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
    const auto id = work_combo_.get_active_id().raw();
    selected_work_id_ = id.empty() ? std::nullopt : std::optional{id};
    selected_structure_id_.reset();
    refresh_library();
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
  show_library();
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
  if (selected_work_id_ &&
      std::none_of(works.begin(), works.end(), [this](const auto &work) {
        return work.id == *selected_work_id_;
      }))
    selected_work_id_.reset();
  const auto wanted = selected_work_id_.value_or("");
  work_combo_.remove_all();
  for (const auto &work : works)
    work_combo_.append(work.id, work.title);
  if (!wanted.empty())
    work_combo_.set_active_id(wanted);
  if (work_combo_.get_active_row_number() < 0 && !works.empty())
    work_combo_.set_active(0);
  const auto active = work_combo_.get_active_id().raw();
  selected_work_id_ = active.empty() ? std::nullopt : std::optional{active};
  create_structure_button_.set_sensitive(selected_work_id_.has_value());
  instantiate_button_.set_sensitive(selected_work_id_.has_value());
  if (pages_.get_visible_child_name() == "editor")
    structural_editor_.set_work(selected_work_id_);
  else
    refresh_library();
}

void EditorialStructureWorkspace::reset() {
  selected_work_id_.reset();
  selected_structure_id_.reset();
  selected_model_id_.reset();
  work_combo_.remove_all();
  create_structure_button_.set_sensitive(false);
  instantiate_button_.set_sensitive(false);
  while (auto *child = models_list_.get_first_child())
    models_list_.remove(*child);
  while (auto *child = structures_list_.get_first_child())
    structures_list_.remove(*child);
  structural_editor_.reset();
  show_library();
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
    structural_editor_.set_work(selected_work_id_);
    structural_editor_.reveal_node(node_id);
    return;
  }
}

void EditorialStructureWorkspace::toggle_inspector() {
  structural_editor_.toggle_inspector();
}

void EditorialStructureWorkspace::show_library() {
  pages_.set_visible_child("library");
  title_.set_text("Modelos de estrutura editorial");
  hint_.set_text("Compare alternativas por Obra. Modelo reutilizável, "
                 "instância aplicada e estrutura ativa permanecem distintos.");
  refresh_library();
}

void EditorialStructureWorkspace::show_editor() {
  pages_.set_visible_child("editor");
  title_.set_text("Estrutura editorial ativa");
  hint_.set_text("Organização de produção e publicação da Obra selecionada.");
  structural_editor_.set_work(selected_work_id_);
  structural_editor_.show_structure();
}

void EditorialStructureWorkspace::refresh_library() {
  while (auto *child = models_list_.get_first_child())
    models_list_.remove(*child);
  while (auto *child = structures_list_.get_first_child())
    structures_list_.remove(*child);
  model_row_ids_.clear();
  structure_row_ids_.clear();
  selected_model_id_.reset();
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
    if (!selected_work_id_)
      return;
    for (const auto &value :
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
  for (auto *button :
       {&duplicate_button_, &derive_button_, &capture_button_,
        &activate_button_, &open_button_, &delete_structure_button_})
    button->set_sensitive(false);
}

void EditorialStructureWorkspace::delete_model() {
  if (!selected_model_id_ || !owner_window())
    return;
  auto *dialog = new OverlayDialog(
      *owner_window(),
      "Remover este modelo editorial do usuário? Instâncias que ainda o "
      "referenciam impedem a remoção.",
      false, Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);
  const auto id = *selected_model_id_;
  dialog->signal_response().connect([this, id](int response) {
    if (response != Gtk::ResponseType::YES)
      return;
    try {
      service_.structures().delete_model(id);
      selected_model_id_.reset();
      refresh_library();
      signal_status_message_.emit("Modelo editorial do usuário removido.");
    } catch (const std::exception &error) {
      show_error(error);
    }
  });
  dialog->present();
}

Gtk::Window *EditorialStructureWorkspace::owner_window() {
  return dynamic_cast<Gtk::Window *>(get_root());
}

void EditorialStructureWorkspace::show_error(const std::exception &error) {
  if (auto *owner = owner_window()) {
    auto *dialog =
        new OverlayDialog(*owner, error.what(), false, Gtk::MessageType::ERROR,
                          Gtk::ButtonsType::CLOSE, true);
    dialog->present();
  }
}

void EditorialStructureWorkspace::create_blank_structure() {
  if (!selected_work_id_ || !owner_window())
    return;
  auto *dialog =
      new OverlayDialog("Nova estrutura editorial", *owner_window(), true);
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
              "Estrutura editorial vazia criada sem alterar a ativa.");
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
  name->set_placeholder_text("Nome da nova instância");
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
      signal_status_message_.emit("Modelo aplicado como instância "
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
  auto *dialog = new OverlayDialog(derived ? "Derivar estrutura editorial"
                                           : "Duplicar estrutura editorial",
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
          derived ? "Estrutura editorial derivada com proveniência."
                  : "Estrutura editorial duplicada com novas identidades.");
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
              "Modelo reutilizável criado sem alterar a instância.");
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
      signal_status_message_.emit("Esta estrutura editorial já está ativa.");
      return;
    }
    auto *dialog =
        new OverlayDialog("Ativar estrutura editorial", *owner_window(), true);
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
        signal_status_message_.emit("Estrutura editorial ativa atualizada; "
                                    "nenhuma colocação foi convertida.");
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
  auto *dialog = new OverlayDialog(*owner_window(),
                                   "Remover esta estrutura editorial e seus "
                                   "elementos? A estrutura ativa é protegida.",
                                   false, Gtk::MessageType::QUESTION,
                                   Gtk::ButtonsType::YES_NO, true);
  const auto id = *selected_structure_id_;
  dialog->signal_response().connect([this, id](int response) {
    if (response != Gtk::ResponseType::YES)
      return;
    try {
      service_.structures().delete_editorial(id);
      selected_structure_id_.reset();
      refresh_library();
      signal_status_message_.emit("Estrutura editorial alternativa removida.");
    } catch (const std::exception &error) {
      show_error(error);
    }
  });
  dialog->present();
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
