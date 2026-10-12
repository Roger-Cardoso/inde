#include "narrative_structure_ui_helpers.hpp"
#include "inde/ui/narrative_structure_workspace.hpp"
#include "inde/ui/project_operations.hpp"

#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>

namespace inde::ui {
using narrative_structure_ui_detail::clear;
using narrative_structure_ui_detail::row_content;

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
          entity_value ? entity_value->name : "Elemento da história ausente";
      std::string scope =
          v.unit_id ? "Parte da narrativa"
                    : (v.structure_id ? "Plano narrativo" : "Obra");
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

void NarrativeStructureWorkspace::create_role() {
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Novo papel na história", *owner, true);
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
  if (auto *owner = owner_window()) confirm_project_removal(*owner, service_, project::RemovalKind::NarrativeRole,
      *selected_role_id_, [this] { refresh_roles(); });
}
void NarrativeStructureWorkspace::assign_role() {
  if (!selected_role_id_)
    return;
  const auto work = selected_work_id();
  if (work.empty()) { start_work();return; }
  auto active = service_.structures().active_narrative(selected_work_id());
  auto units = active ? service_.structures().units(active->id)
                      : std::vector<project::NarrativeUnit>{};
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Definir papel na história", *owner, true);
  auto *entity = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar elemento associado à Obra",
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
      "Pesquise um elemento da história associado à obra");
  auto *scope = Gtk::make_managed<Gtk::ComboBoxText>();
  scope->append("work", "Toda a Obra");
  if (active)
    scope->append("structure", "Plano narrativo atual");
  for (const auto &v : units)
    scope->append("unit:" + v.id, "Parte · " + v.title);
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
            throw std::runtime_error("Escolha um elemento da história para atribuir");
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

} // namespace inde::ui
