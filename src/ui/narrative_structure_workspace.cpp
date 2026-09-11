#include "inde/ui/narrative_structure_workspace.hpp"

#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>

namespace inde::ui {
namespace {

void clear(Gtk::ListBox &list) {
  while (auto *child = list.get_first_child())
    list.remove(*child);
}

Gtk::Box *row_content(const Glib::ustring &title, const Glib::ustring &detail) {
  auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
  box->set_margin(8);
  auto *heading = Gtk::make_managed<Gtk::Label>(title);
  heading->set_halign(Gtk::Align::START);
  heading->set_xalign(0.0F);
  heading->add_css_class("heading");
  auto *secondary = Gtk::make_managed<Gtk::Label>(detail);
  secondary->set_halign(Gtk::Align::START);
  secondary->set_xalign(0.0F);
  secondary->set_wrap(true);
  secondary->add_css_class("dim-label");
  box->append(*heading);
  box->append(*secondary);
  return box;
}

std::string origin_label(project::StructureCreationKind value) {
  switch (value) {
  case project::StructureCreationKind::Migrated:
    return "Migrada";
  case project::StructureCreationKind::Blank:
    return "Vazia";
  case project::StructureCreationKind::Instantiated:
    return "Instanciada de modelo";
  case project::StructureCreationKind::Duplicated:
    return "Duplicada";
  case project::StructureCreationKind::Derived:
    return "Derivada";
  }
  return {};
}
std::string link_label(project::NarrativeLinkKind value) {
  switch (value) {
  case project::NarrativeLinkKind::Precedes:
    return "precede";
  case project::NarrativeLinkKind::Causes:
    return "causa";
  case project::NarrativeLinkKind::Alternative:
    return "é alternativa a";
  case project::NarrativeLinkKind::Depends:
    return "depende de";
  }
  return {};
}

} // namespace

NarrativeStructureWorkspace::NarrativeStructureWorkspace(
    application::ProjectService &service)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 10), service_(service) {
  build_ui();
}

void NarrativeStructureWorkspace::build_ui() {
  set_hexpand(true);
  set_vexpand(true);
  title_.add_css_class("title-1");
  title_.set_halign(Gtk::Align::START);
  hint_.add_css_class("dim-label");
  hint_.set_halign(Gtk::Align::START);
  hint_.set_wrap(true);
  heading_.set_hexpand(true);
  heading_.append(title_);
  heading_.append(hint_);
  header_.append(heading_);
  header_.append(work_combo_);
  header_.append(library_button_);
  header_.append(roles_button_);
  append(header_);
  stack_.set_hexpand(true);
  stack_.set_vexpand(true);
  stack_.set_transition_type(Gtk::StackTransitionType::SLIDE_LEFT_RIGHT);
  append(stack_);

  library_toolbar_.set_selection_mode(Gtk::SelectionMode::NONE);
  library_toolbar_.set_column_spacing(6);
  library_toolbar_.set_row_spacing(6);
  library_toolbar_.set_min_children_per_line(1);
  library_toolbar_.set_max_children_per_line(7);
  library_toolbar_.set_halign(Gtk::Align::FILL);
  library_toolbar_.append(create_button_);
  library_toolbar_.append(instantiate_button_);
  library_toolbar_.append(duplicate_button_);
  library_toolbar_.append(derive_button_);
  library_toolbar_.append(capture_button_);
  library_toolbar_.append(activate_button_);
  library_toolbar_.append(delete_button_);
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
  stack_.add(library_page_, "library");

  detail_title_.set_hexpand(true);
  detail_title_.set_halign(Gtk::Align::START);
  detail_title_.add_css_class("title-2");
  detail_header_.append(detail_back_);
  detail_header_.append(detail_title_);
  detail_page_.append(detail_header_);
  lines_toolbar_.append(add_line_button_);
  lines_toolbar_.append(remove_line_button_);
  lines_panel_.append(lines_toolbar_);
  auto *line_heading = Gtk::make_managed<Gtk::Label>("Linhas narrativas");
  line_heading->set_halign(Gtk::Align::START);
  line_heading->add_css_class("heading");
  lines_panel_.prepend(*line_heading);
  lines_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  lines_list_.add_css_class("boxed-list");
  lines_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  lines_scroll_.set_child(lines_list_);
  lines_scroll_.set_vexpand(true);
  lines_panel_.append(lines_scroll_);
  units_toolbar_.append(add_unit_button_);
  units_toolbar_.append(remove_unit_button_);
  units_toolbar_.append(membership_button_);
  units_toolbar_.append(reality_button_);
  units_toolbar_.append(link_button_);
  units_toolbar_.append(remove_link_button_);
  units_panel_.append(units_toolbar_);
  auto *unit_heading = Gtk::make_managed<Gtk::Label>("Unidades e vínculos");
  unit_heading->set_halign(Gtk::Align::START);
  unit_heading->add_css_class("heading");
  units_panel_.prepend(*unit_heading);
  units_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  units_list_.add_css_class("boxed-list");
  links_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  links_list_.add_css_class("boxed-list");
  units_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  units_scroll_.set_child(units_list_);
  units_scroll_.set_vexpand(true);
  units_panel_.append(units_scroll_);
  auto *links_heading =
      Gtk::make_managed<Gtk::Label>("Precedência, causalidade e alternativas");
  links_heading->set_halign(Gtk::Align::START);
  links_heading->add_css_class("heading");
  units_panel_.append(*links_heading);
  links_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  links_scroll_.set_child(links_list_);
  links_scroll_.set_vexpand(true);
  units_panel_.append(links_scroll_);
  detail_paned_.set_start_child(lines_panel_);
  detail_paned_.set_end_child(units_panel_);
  detail_paned_.set_position(330);
  detail_paned_.set_vexpand(true);
  detail_page_.append(detail_paned_);
  stack_.add(detail_page_, "detail");

  roles_toolbar_.append(create_role_button_);
  roles_toolbar_.append(delete_role_button_);
  roles_toolbar_.append(assign_role_button_);
  roles_toolbar_.append(delete_assignment_button_);
  roles_page_.append(roles_toolbar_);
  roles_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  roles_list_.add_css_class("boxed-list");
  assignments_list_.set_selection_mode(Gtk::SelectionMode::SINGLE);
  assignments_list_.add_css_class("boxed-list");
  roles_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  roles_scroll_.set_child(roles_list_);
  assignments_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                 Gtk::PolicyType::AUTOMATIC);
  assignments_scroll_.set_child(assignments_list_);
  roles_paned_.set_start_child(roles_scroll_);
  roles_paned_.set_end_child(assignments_scroll_);
  roles_paned_.set_position(360);
  roles_paned_.set_vexpand(true);
  roles_page_.append(roles_paned_);
  stack_.add(roles_page_, "roles");

  work_combo_.signal_changed().connect([this] {
    selected_structure_id_.reset();
    show_library();
    refresh_library();
    refresh_roles();
  });
  library_button_.signal_clicked().connect([this] { show_library(); });
  roles_button_.signal_clicked().connect([this] { show_roles(); });
  detail_back_.signal_clicked().connect([this] { show_library(); });
  structures_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_structure_id_ =
        row && row->get_index() >= 0 &&
                static_cast<std::size_t>(row->get_index()) <
                    structure_row_ids_.size()
            ? std::optional{structure_row_ids_[row->get_index()]}
            : std::nullopt;
    const bool has = selected_structure_id_.has_value();
    duplicate_button_.set_sensitive(has);
    derive_button_.set_sensitive(has);
    capture_button_.set_sensitive(has);
    activate_button_.set_sensitive(has);
    delete_button_.set_sensitive(has);
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
        service_.structures().models(project::StructureLayer::Narrative);
    const auto found =
        std::find_if(models.begin(), models.end(), [&](const auto &value) {
          return value.id == *selected_model_id_;
        });
    delete_model_button_.set_sensitive(found != models.end() &&
                                       !found->is_builtin);
  });
  structures_list_.signal_row_activated().connect([this](Gtk::ListBoxRow *row) {
    if (row && row->get_index() >= 0 &&
        static_cast<std::size_t>(row->get_index()) < structure_row_ids_.size())
      show_detail(structure_row_ids_[row->get_index()]);
  });
  lines_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_line_id_ = row && row->get_index() >= 0 &&
                                static_cast<std::size_t>(row->get_index()) <
                                    line_row_ids_.size()
                            ? std::optional{line_row_ids_[row->get_index()]}
                            : std::nullopt;
    remove_line_button_.set_sensitive(selected_line_id_.has_value());
  });
  units_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_unit_id_ = row && row->get_index() >= 0 &&
                                static_cast<std::size_t>(row->get_index()) <
                                    unit_row_ids_.size()
                            ? std::optional{unit_row_ids_[row->get_index()]}
                            : std::nullopt;
    const bool has = selected_unit_id_.has_value();
    remove_unit_button_.set_sensitive(has);
    membership_button_.set_sensitive(has);
    reality_button_.set_sensitive(has);
    link_button_.set_sensitive(has);
  });
  links_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_link_id_ = row && row->get_index() >= 0 &&
                                static_cast<std::size_t>(row->get_index()) <
                                    link_row_ids_.size()
                            ? std::optional{link_row_ids_[row->get_index()]}
                            : std::nullopt;
    remove_link_button_.set_sensitive(selected_link_id_.has_value());
  });
  roles_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_role_id_ = row && row->get_index() >= 0 &&
                                static_cast<std::size_t>(row->get_index()) <
                                    role_row_ids_.size()
                            ? std::optional{role_row_ids_[row->get_index()]}
                            : std::nullopt;
    assign_role_button_.set_sensitive(selected_role_id_.has_value());
  });
  assignments_list_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    selected_assignment_id_ =
        row && row->get_index() >= 0 &&
                static_cast<std::size_t>(row->get_index()) <
                    assignment_row_ids_.size()
            ? std::optional{assignment_row_ids_[row->get_index()]}
            : std::nullopt;
    delete_assignment_button_.set_sensitive(
        selected_assignment_id_.has_value());
  });
  create_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::create_blank));
  instantiate_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::instantiate_model));
  duplicate_button_.signal_clicked().connect(
      [this] { duplicate_selected(false); });
  derive_button_.signal_clicked().connect([this] { duplicate_selected(true); });
  capture_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::capture_model));
  activate_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::activate_selected));
  delete_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::delete_selected));
  delete_model_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::delete_model));
  add_line_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::add_line));
  add_unit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::add_unit));
  remove_line_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::delete_line));
  remove_unit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::delete_unit));
  membership_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::connect_unit_line));
  reality_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::connect_unit_entity));
  link_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::add_link));
  remove_link_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::delete_link));
  create_role_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::create_role));
  delete_role_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::delete_role));
  assign_role_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::assign_role));
  delete_assignment_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &NarrativeStructureWorkspace::delete_assignment));
  reset();
}

Gtk::Window *NarrativeStructureWorkspace::owner_window() {
  return dynamic_cast<Gtk::Window *>(get_root());
}
std::string NarrativeStructureWorkspace::selected_work_id() const {
  return work_combo_.get_active_id().raw();
}
void NarrativeStructureWorkspace::refresh_works() {
  const auto wanted = selected_work_id();
  work_combo_.remove_all();
  for (const auto &work : service_.catalog().works)
    work_combo_.append(work.id, work.title);
  if (!wanted.empty())
    work_combo_.set_active_id(wanted);
  if (work_combo_.get_active_row_number() < 0 &&
      !service_.catalog().works.empty())
    work_combo_.set_active(0);
  const bool has_work = !selected_work_id().empty();
  create_button_.set_sensitive(has_work);
  instantiate_button_.set_sensitive(has_work);
}
void NarrativeStructureWorkspace::refresh() {
  if (!service_.current()) {
    reset();
    return;
  }
  refresh_works();
  refresh_library();
  refresh_roles();
  if (stack_.get_visible_child_name() == "detail" && selected_structure_id_)
    refresh_detail();
}
void NarrativeStructureWorkspace::reset() {
  work_combo_.remove_all();
  create_button_.set_sensitive(false);
  instantiate_button_.set_sensitive(false);
  clear(models_list_);
  clear(structures_list_);
  clear(lines_list_);
  clear(units_list_);
  clear(links_list_);
  clear(roles_list_);
  clear(assignments_list_);
  selected_structure_id_.reset();
  selected_model_id_.reset();
  selected_line_id_.reset();
  selected_unit_id_.reset();
  selected_link_id_.reset();
  selected_role_id_.reset();
  selected_assignment_id_.reset();
  show_library();
}
void NarrativeStructureWorkspace::show_library() {
  stack_.set_visible_child("library");
  library_button_.add_css_class("suggested-action");
  roles_button_.remove_css_class("suggested-action");
}
void NarrativeStructureWorkspace::show_roles() {
  stack_.set_visible_child("roles");
  roles_button_.add_css_class("suggested-action");
  library_button_.remove_css_class("suggested-action");
  refresh_roles();
}
void NarrativeStructureWorkspace::show_detail(std::string id) {
  selected_structure_id_ = std::move(id);
  stack_.set_visible_child("detail");
  library_button_.remove_css_class("suggested-action");
  roles_button_.remove_css_class("suggested-action");
  refresh_detail();
}

void NarrativeStructureWorkspace::refresh_library() {
  clear(models_list_);
  clear(structures_list_);
  model_row_ids_.clear();
  structure_row_ids_.clear();
  selected_model_id_.reset();
  delete_model_button_.set_sensitive(false);
  if (!service_.current())
    return;
  try {
    for (const auto &value :
         service_.structures().models(project::StructureLayer::Narrative)) {
      models_list_.append(*row_content(
          value.name, std::string(value.is_builtin ? "Sistema" : "Usuário") +
                          " · " + value.description));
      model_row_ids_.push_back(value.id);
    }
  } catch (const std::exception &e) {
    show_error(e);
    return;
  }
  const auto work = selected_work_id();
  create_button_.set_sensitive(!work.empty());
  instantiate_button_.set_sensitive(!work.empty());
  if (work.empty())
    return;
  try {
    for (const auto &value : service_.structures().narrative_structures(work)) {
      std::ostringstream detail;
      detail << (value.is_active ? "ATIVA · " : "")
             << origin_label(value.creation_kind);
      const auto unit_count = service_.structures().units(value.id).size();
      const auto line_count = service_.structures().lines(value.id).size();
      detail << " · " << unit_count << " unidades · " << line_count
             << " linhas";
      if (!value.description.empty())
        detail << "\n" << value.description;
      structures_list_.append(*row_content(value.name, detail.str()));
      structure_row_ids_.push_back(value.id);
    }
  } catch (const std::exception &e) {
    show_error(e);
  }
  duplicate_button_.set_sensitive(false);
  derive_button_.set_sensitive(false);
  capture_button_.set_sensitive(false);
  activate_button_.set_sensitive(false);
  delete_button_.set_sensitive(false);
}

void NarrativeStructureWorkspace::delete_model() {
  if (!selected_model_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog(
      *owner,
      "Remover este modelo narrativo do usuário? Instâncias que ainda o "
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
      signal_status_message_.emit("Modelo narrativo do usuário removido.");
    } catch (const std::exception &e) {
      show_error(e);
    }
  });
  dialog->present();
}
void NarrativeStructureWorkspace::refresh_detail() {
  if (!selected_structure_id_)
    return;
  const auto work = selected_work_id();
  const auto structures = service_.structures().narrative_structures(work);
  const auto found =
      std::find_if(structures.begin(), structures.end(), [&](const auto &v) {
        return v.id == *selected_structure_id_;
      });
  if (found == structures.end()) {
    show_library();
    return;
  }
  detail_title_.set_text(found->name + (found->is_active ? " · ativa" : ""));
  const auto lines = service_.structures().lines(found->id);
  const auto units = service_.structures().units(found->id);
  const auto memberships = service_.structures().unit_lines(found->id);
  const auto entities = service_.structures().unit_entities(found->id);
  const auto links = service_.structures().links(found->id);
  std::unordered_map<std::string, std::string> line_names, unit_names;
  for (const auto &v : lines)
    line_names.emplace(v.id, v.name);
  for (const auto &v : units)
    unit_names.emplace(v.id, v.title);
  clear(lines_list_);
  line_row_ids_.clear();
  for (const auto &v : lines) {
    const auto count =
        std::count_if(memberships.begin(), memberships.end(),
                      [&](const auto &m) { return m.line_id == v.id; });
    lines_list_.append(*row_content(
        v.name, std::to_string(count) + " unidades · " + v.description));
    line_row_ids_.push_back(v.id);
  }
  clear(units_list_);
  unit_row_ids_.clear();
  for (const auto &v : units) {
    std::ostringstream detail;
    detail << (v.designator.empty() ? "Sem designador" : v.designator);
    if (!v.purpose.empty())
      detail << " · " << v.purpose;
    if (!v.perspective.empty())
      detail << " · perspectiva: " << v.perspective;
    std::vector<std::string> memberships_for_unit;
    for (const auto &m : memberships)
      if (m.unit_id == v.id && line_names.contains(m.line_id))
        memberships_for_unit.push_back(line_names.at(m.line_id));
    detail << " · " << memberships_for_unit.size() << " linhas";
    const auto reality =
        std::count_if(entities.begin(), entities.end(),
                      [&](const auto &e) { return e.unit_id == v.id; });
    detail << " · " << reality << " vínculos com a realidade";
    units_list_.append(*row_content(v.title, detail.str()));
    unit_row_ids_.push_back(v.id);
  }
  clear(links_list_);
  link_row_ids_.clear();
  for (const auto &v : links) {
    const auto from = unit_names.contains(v.source_unit_id)
                          ? unit_names.at(v.source_unit_id)
                          : "Unidade ausente";
    const auto to = unit_names.contains(v.target_unit_id)
                        ? unit_names.at(v.target_unit_id)
                        : "Unidade ausente";
    links_list_.append(
        *row_content(from + " " + link_label(v.kind) + " " + to, v.label));
    link_row_ids_.push_back(v.id);
  }
  selected_line_id_.reset();
  selected_unit_id_.reset();
  selected_link_id_.reset();
  remove_line_button_.set_sensitive(false);
  remove_unit_button_.set_sensitive(false);
  membership_button_.set_sensitive(false);
  reality_button_.set_sensitive(false);
  link_button_.set_sensitive(false);
  remove_link_button_.set_sensitive(false);
}

void NarrativeStructureWorkspace::refresh_roles() {
  clear(roles_list_);
  clear(assignments_list_);
  role_row_ids_.clear();
  assignment_row_ids_.clear();
  if (!service_.current())
    return;
  try {
    const auto roles = service_.structures().roles();
    std::unordered_map<std::string, std::string> role_names;
    for (const auto &v : roles) {
      roles_list_.append(
          *row_content(v.name, (v.is_builtin ? "Sistema · " : "Usuário · ") +
                                   v.description));
      role_row_ids_.push_back(v.id);
      role_names.emplace(v.id, v.name);
    }
    const auto work = selected_work_id();
    for (const auto &v : service_.structures().role_assignments(
             work.empty() ? std::nullopt : std::optional{work})) {
      const auto role = role_names.contains(v.role_id)
                            ? role_names.at(v.role_id)
                            : "Papel ausente";
      const auto entity_value = service_.narrative().entity(v.entity_id);
      const auto entity =
          entity_value ? entity_value->name : "Entidade ausente";
      std::string scope =
          v.unit_id ? "Unidade narrativa"
                    : (v.structure_id ? "Estrutura narrativa" : "Obra");
      assignments_list_.append(
          *row_content(entity + " · " + role,
                       scope + (v.notes.empty() ? "" : " · " + v.notes)));
      assignment_row_ids_.push_back(v.id);
    }
  } catch (const std::exception &e) {
    show_error(e);
  }
  assign_role_button_.set_sensitive(false);
  delete_assignment_button_.set_sensitive(false);
}

void NarrativeStructureWorkspace::create_blank() {
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Nova estrutura narrativa", *owner, true);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome da alternativa");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição opcional");
  dialog->get_content_area()->append(*name);
  dialog->get_content_area()->append(*description);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  dialog->signal_response().connect([this, name, description](int response) {
    if (response != Gtk::ResponseType::ACCEPT)
      return;
    try {
      static_cast<void>(service_.structures().create_narrative(
          selected_work_id(), name->get_text(), description->get_text()));
      refresh_library();
      signal_status_message_.emit("Estrutura narrativa vazia criada.");
    } catch (const std::exception &e) {
      show_error(e);
    }
  });
  dialog->present();
}
void NarrativeStructureWorkspace::instantiate_model() {
  auto *owner = owner_window();
  if (!owner)
    return;
  auto models =
      service_.structures().models(project::StructureLayer::Narrative);
  auto *dialog = new OverlayDialog("Aplicar modelo narrativo", *owner, true);
  auto *combo = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &v : models)
    combo->append(v.id, v.name + (v.is_builtin ? " · Sistema" : " · Usuário"));
  if (!models.empty())
    combo->set_active(0);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome da nova instância");
  dialog->get_content_area()->append(*combo);
  dialog->get_content_area()->append(*name);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Instanciar", Gtk::ResponseType::ACCEPT);
  dialog->signal_response().connect([this, combo, name](int response) {
    if (response != Gtk::ResponseType::ACCEPT)
      return;
    try {
      static_cast<void>(service_.structures().instantiate_narrative(
          selected_work_id(), combo->get_active_id().raw(), name->get_text()));
      refresh_library();
      signal_status_message_.emit(
          "Modelo aplicado como nova instância; a ativa não foi alterada.");
    } catch (const std::exception &e) {
      show_error(e);
    }
  });
  dialog->present();
}
void NarrativeStructureWorkspace::duplicate_selected(bool derived) {
  if (!selected_structure_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog(
      derived ? "Derivar estrutura" : "Duplicar estrutura", *owner, true);
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
      static_cast<void>(service_.structures().duplicate_narrative(
          id, name->get_text(), derived));
      refresh_library();
      signal_status_message_.emit(
          derived ? "Estrutura derivada com proveniência preservada."
                  : "Estrutura duplicada com novas identidades.");
    } catch (const std::exception &e) {
      show_error(e);
    }
  });
  dialog->present();
}
void NarrativeStructureWorkspace::capture_model() {
  if (!selected_structure_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog("Salvar narrativa como modelo", *owner, true);
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
          static_cast<void>(service_.structures().capture_narrative_model(
              id, name->get_text(), description->get_text()));
          refresh_library();
          signal_status_message_.emit("Modelo narrativo reutilizável criado "
                                      "com linhas, pertencimentos e vínculos.");
        } catch (const std::exception &e) {
          show_error(e);
        }
      });
  dialog->present();
}
void NarrativeStructureWorkspace::activate_selected() {
  if (!selected_structure_id_)
    return;
  try {
    const auto impact = service_.structures().narrative_activation_impact(
        *selected_structure_id_);
    if (impact.current_structure_id == impact.target_structure_id) {
      signal_status_message_.emit("Esta estrutura narrativa já está ativa.");
      return;
    }
    auto *owner = owner_window();
    if (!owner)
      return;
    auto *dialog =
        new OverlayDialog("Ativar estrutura narrativa", *owner, true);
    dialog->set_secondary_text(
        "A seleção ativa da Obra será trocada. Nenhuma alternativa será "
        "apagada; a estrutura alvo possui " +
        std::to_string(impact.target_units) + " unidades.");
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    dialog->add_button("Ativar", Gtk::ResponseType::ACCEPT);
    const auto id = *selected_structure_id_;
    dialog->signal_response().connect([this, id](int response) {
      if (response != Gtk::ResponseType::ACCEPT)
        return;
      try {
        service_.structures().activate_narrative(id);
        refresh_library();
        signal_status_message_.emit("Estrutura narrativa ativa atualizada.");
      } catch (const std::exception &e) {
        show_error(e);
      }
    });
    dialog->present();
  } catch (const std::exception &e) {
    show_error(e);
  }
}
void NarrativeStructureWorkspace::delete_selected() {
  if (!selected_structure_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog(*owner,
                                   "Remover esta estrutura narrativa e suas "
                                   "unidades? A estrutura ativa é protegida.",
                                   false, Gtk::MessageType::QUESTION,
                                   Gtk::ButtonsType::YES_NO, true);
  const auto id = *selected_structure_id_;
  dialog->signal_response().connect([this, id](int response) {
    if (response != Gtk::ResponseType::YES)
      return;
    try {
      service_.structures().delete_narrative(id);
      selected_structure_id_.reset();
      refresh_library();
      signal_status_message_.emit("Estrutura narrativa removida.");
    } catch (const std::exception &e) {
      show_error(e);
    }
  });
  dialog->present();
}

void NarrativeStructureWorkspace::add_line() {
  if (!selected_structure_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Nova linha narrativa", *owner, true);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição opcional");
  dialog->get_content_area()->append(*name);
  dialog->get_content_area()->append(*description);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  const auto id = *selected_structure_id_;
  dialog->signal_response().connect([this, id, name, description](int r) {
    if (r != Gtk::ResponseType::ACCEPT)
      return;
    try {
      static_cast<void>(service_.structures().create_line(
          id, name->get_text(), description->get_text()));
      refresh_detail();
    } catch (const std::exception &e) {
      show_error(e);
    }
  });
  dialog->present();
}
void NarrativeStructureWorkspace::add_unit() {
  if (!selected_structure_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Nova unidade narrativa", *owner, true);
  auto *designator = Gtk::make_managed<Gtk::Entry>();
  designator->set_placeholder_text("Número/designador opcional");
  auto *title = Gtk::make_managed<Gtk::Entry>();
  title->set_placeholder_text("Título");
  auto *summary = Gtk::make_managed<Gtk::Entry>();
  summary->set_placeholder_text("Resumo");
  auto *purpose = Gtk::make_managed<Gtk::Entry>();
  purpose->set_placeholder_text("Propósito narrativo");
  auto *perspective = Gtk::make_managed<Gtk::Entry>();
  perspective->set_placeholder_text("Perspectiva ou PoV");
  for (auto *w : {designator, title, summary, purpose, perspective})
    dialog->get_content_area()->append(*w);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  const auto id = *selected_structure_id_;
  dialog->signal_response().connect([this, id, designator, title, summary,
                                     purpose, perspective](int r) {
    if (r != Gtk::ResponseType::ACCEPT)
      return;
    try {
      static_cast<void>(service_.structures().create_unit(
          id, designator->get_text(), title->get_text(), summary->get_text(),
          purpose->get_text(), perspective->get_text()));
      refresh_detail();
    } catch (const std::exception &e) {
      show_error(e);
    }
  });
  dialog->present();
}
void NarrativeStructureWorkspace::delete_line() {
  if (!selected_line_id_)
    return;
  try {
    service_.structures().delete_line(*selected_line_id_);
    refresh_detail();
  } catch (const std::exception &e) {
    show_error(e);
  }
}
void NarrativeStructureWorkspace::delete_unit() {
  if (!selected_unit_id_)
    return;
  try {
    service_.structures().delete_unit(*selected_unit_id_);
    refresh_detail();
  } catch (const std::exception &e) {
    show_error(e);
  }
}
void NarrativeStructureWorkspace::connect_unit_line() {
  if (!selected_structure_id_ || !selected_unit_id_)
    return;
  auto lines = service_.structures().lines(*selected_structure_id_);
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog("Vincular unidade a uma linha", *owner, true);
  auto *combo = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &v : lines)
    combo->append(v.id, v.name);
  if (!lines.empty())
    combo->set_active(0);
  dialog->get_content_area()->append(*combo);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Vincular", Gtk::ResponseType::ACCEPT);
  const auto unit = *selected_unit_id_;
  dialog->signal_response().connect([this, combo, unit](int r) {
    if (r != Gtk::ResponseType::ACCEPT)
      return;
    try {
      service_.structures().add_unit_to_line(unit,
                                             combo->get_active_id().raw());
      refresh_detail();
    } catch (const std::exception &e) {
      show_error(e);
    }
  });
  dialog->present();
}
void NarrativeStructureWorkspace::connect_unit_entity() {
  if (!selected_unit_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog("Vincular realidade ficcional", *owner, true);
  auto *entity = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar entidade",
      [this](const std::string &search, std::size_t limit) {
        persistence::EntityQuery query;
        query.search = search;
        query.limit = std::min(limit, std::size_t{50});
        std::vector<IncrementalSelection> result;
        for (const auto &value : service_.narrative().entities(query))
          result.push_back({value.id, value.name, value.summary});
        return result;
      });
  auto *role = Gtk::make_managed<Gtk::Entry>();
  role->set_placeholder_text("Função nesta unidade (opcional)");
  dialog->get_content_area()->append(*entity);
  dialog->get_content_area()->append(*role);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Vincular", Gtk::ResponseType::ACCEPT);
  const auto unit = *selected_unit_id_;
  dialog->signal_response().connect([this, entity, role, unit](int r) {
    if (r != Gtk::ResponseType::ACCEPT)
      return;
    try {
      if (!entity->selected_id())
        throw std::runtime_error("Escolha uma entidade para vincular");
      service_.structures().add_entity_to_unit(unit, *entity->selected_id(),
                                               role->get_text());
      refresh_detail();
    } catch (const std::exception &e) {
      show_error(e);
    }
  });
  dialog->present();
}
void NarrativeStructureWorkspace::add_link() {
  if (!selected_structure_id_ || !selected_unit_id_)
    return;
  const auto units = service_.structures().units(*selected_structure_id_);
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Novo vínculo narrativo", *owner, true);
  auto *target = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &v : units)
    if (v.id != *selected_unit_id_)
      target->append(v.id, v.title);
  if (units.size() > 1)
    target->set_active(0);
  auto *kind = Gtk::make_managed<Gtk::ComboBoxText>();
  kind->append("precedes", "Precede");
  kind->append("causes", "Causa");
  kind->append("alternative", "Alternativa");
  kind->append("depends", "Depende de");
  kind->set_active(0);
  auto *label = Gtk::make_managed<Gtk::Entry>();
  label->set_placeholder_text("Observação opcional");
  dialog->get_content_area()->append(*target);
  dialog->get_content_area()->append(*kind);
  dialog->get_content_area()->append(*label);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  const auto structure = *selected_structure_id_, source = *selected_unit_id_;
  dialog->signal_response().connect([this, target, kind, label, structure,
                                     source](int r) {
    if (r != Gtk::ResponseType::ACCEPT)
      return;
    try {
      static_cast<void>(service_.structures().create_link(
          structure, source, target->get_active_id().raw(),
          project::narrative_link_kind_from_string(kind->get_active_id().raw()),
          label->get_text()));
      refresh_detail();
    } catch (const std::exception &e) {
      show_error(e);
    }
  });
  dialog->present();
}
void NarrativeStructureWorkspace::delete_link() {
  if (!selected_link_id_)
    return;
  try {
    service_.structures().delete_link(*selected_link_id_);
    refresh_detail();
  } catch (const std::exception &e) {
    show_error(e);
  }
}

void NarrativeStructureWorkspace::create_role() {
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Novo papel narrativo", *owner, true);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome do papel");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição");
  dialog->get_content_area()->append(*name);
  dialog->get_content_area()->append(*description);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  dialog->signal_response().connect([this, name, description](int r) {
    if (r != Gtk::ResponseType::ACCEPT)
      return;
    try {
      static_cast<void>(service_.structures().create_role(
          name->get_text(), description->get_text()));
      refresh_roles();
    } catch (const std::exception &e) {
      show_error(e);
    }
  });
  dialog->present();
}
void NarrativeStructureWorkspace::delete_role() {
  if (!selected_role_id_)
    return;
  try {
    const auto roles = service_.structures().roles();
    const auto found =
        std::find_if(roles.begin(), roles.end(),
                     [&](const auto &v) { return v.id == *selected_role_id_; });
    if (found != roles.end() && found->is_builtin)
      throw std::runtime_error("Papéis narrativos do sistema são protegidos");
    service_.structures().delete_role(*selected_role_id_);
    refresh_roles();
  } catch (const std::exception &e) {
    show_error(e);
  }
}
void NarrativeStructureWorkspace::assign_role() {
  if (!selected_role_id_)
    return;
  const auto work = selected_work_id();
  auto active = service_.structures().active_narrative(selected_work_id());
  auto units = active ? service_.structures().units(active->id)
                      : std::vector<project::NarrativeUnit>{};
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Atribuir papel narrativo", *owner, true);
  auto *entity = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar entidade vinculada à Obra",
      [this, work](const std::string &search, std::size_t limit) {
        persistence::EntityQuery query;
        query.search = search;
        query.work_id = work;
        query.limit = std::min(limit, std::size_t{50});
        std::vector<IncrementalSelection> result;
        for (const auto &value : service_.narrative().entities(query))
          result.push_back({value.id, value.name, value.summary});
        return result;
      },
      "Pesquise uma entidade vinculada à Obra");
  auto *scope = Gtk::make_managed<Gtk::ComboBoxText>();
  scope->append("work", "Toda a Obra");
  if (active)
    scope->append("structure", "Estrutura narrativa ativa");
  for (const auto &v : units)
    scope->append("unit:" + v.id, "Unidade · " + v.title);
  scope->set_active(0);
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_placeholder_text("Notas opcionais");
  dialog->get_content_area()->append(*entity);
  dialog->get_content_area()->append(*scope);
  dialog->get_content_area()->append(*notes);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Atribuir", Gtk::ResponseType::ACCEPT);
  const auto role = *selected_role_id_;
  dialog->signal_response().connect(
      [this, entity, scope, notes, role, work, active](int r) {
        if (r != Gtk::ResponseType::ACCEPT)
          return;
        try {
          if (!entity->selected_id())
            throw std::runtime_error("Escolha uma entidade para atribuir");
          const auto selected = scope->get_active_id().raw();
          std::optional<std::string> structure, unit;
          if (selected == "structure" && active)
            structure = active->id;
          else if (selected.starts_with("unit:") && active) {
            structure = active->id;
            unit = selected.substr(5);
          }
          static_cast<void>(service_.structures().assign_role(
              role, *entity->selected_id(), work, structure, unit,
              notes->get_text()));
          refresh_roles();
        } catch (const std::exception &e) {
          show_error(e);
        }
      });
  dialog->present();
}
void NarrativeStructureWorkspace::delete_assignment() {
  if (!selected_assignment_id_)
    return;
  try {
    service_.structures().remove_role_assignment(*selected_assignment_id_);
    refresh_roles();
  } catch (const std::exception &e) {
    show_error(e);
  }
}

void NarrativeStructureWorkspace::show_error(const std::exception &error) {
  if (auto *owner = owner_window()) {
    auto *dialog =
        new OverlayDialog(*owner, error.what(), false, Gtk::MessageType::ERROR,
                          Gtk::ButtonsType::CLOSE, true);
    dialog->present();
  }
}

} // namespace inde::ui
