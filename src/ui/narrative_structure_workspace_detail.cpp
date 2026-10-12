#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/narrative_structure_workspace.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/ui/accessibility.hpp"
#include "narrative_structure_ui_helpers.hpp"

#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace inde::ui {
using narrative_structure_ui_detail::clear;
using narrative_structure_ui_detail::row_content;
namespace {
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

void NarrativeStructureWorkspace::refresh_detail() {
  if (refreshing_detail_)
    return;
  struct Guard {
    bool &v;
    ~Guard() { v = false; }
  } guard{refreshing_detail_};
  refreshing_detail_ = true;
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
  const auto wanted_line = selected_line_id_, wanted_unit = selected_unit_id_,
             wanted_link = selected_link_id_;
  try {
    builder_snapshot_ = service_.builder().snapshot(found->id);
  } catch (const std::exception &e) {
    show_error(e);
    return;
  }
  detail_title_.set_text(found->name + (found->is_active ? " · plano atual" : ""));
  const auto lines = service_.structures().lines(found->id);
  auto units = builder_snapshot_.units;
  if (order_combo_.get_active_id() == "writing") {
    auto position = [this](const std::string &id) {
      for (const auto &c : builder_snapshot_.contexts)
        if (c.unit_id == id && c.writing_position)
          return *c.writing_position;
      return std::numeric_limits<std::int64_t>::max();
    };
    std::stable_sort(units.begin(), units.end(), [&](auto &a, auto &b) {
      return position(a.id) < position(b.id);
    });
  }
  if (order_combo_.get_active_id() == "line") {
    const auto line = wanted_line.value_or("");
    std::map<std::string, std::int64_t> position;
    for (const auto &m : builder_snapshot_.memberships)
      if (m.line_id == line)
        position[m.unit_id] = m.position;
    std::erase_if(units,
                  [&](const auto &u) { return !position.contains(u.id); });
    std::sort(units.begin(), units.end(), [&](const auto &a, const auto &b) {
      return std::pair{position[a.id], a.id} < std::pair{position[b.id], b.id};
    });
  }
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
        v.name, std::to_string(count) + " partes · " + v.description));
    line_row_ids_.push_back(v.id);
  }
  clear(units_list_);
  unit_row_ids_.clear();
  for (const auto &v : units) {
    std::ostringstream detail;
    detail << (v.designator.empty() ? "Sem código curto" : v.designator);
    if (!v.purpose.empty())
      detail << " · " << v.purpose;
    if (!v.perspective.empty())
      detail << " · ponto de vista planejado: " << v.perspective;
    std::vector<std::string> memberships_for_unit;
    for (const auto &m : memberships)
      if (m.unit_id == v.id && line_names.contains(m.line_id))
        memberships_for_unit.push_back(line_names.at(m.line_id));
    detail << " · " << memberships_for_unit.size() << " linhas";
    const auto reality =
        std::count_if(entities.begin(), entities.end(),
                      [&](const auto &e) { return e.unit_id == v.id; });
    detail << " · " << reality << " associações a fichas";
    if (order_combo_.get_active_id() == "writing") {
      bool queued = false;
      for (const auto &c : builder_snapshot_.contexts)
        if (c.unit_id == v.id && c.writing_position)
          queued = true;
      detail << (queued ? " · na ordem de escrita"
                        : " · fora da ordem de escrita (sem posição)");
    }
    units_list_.append(*row_content(v.title, detail.str()));
    unit_row_ids_.push_back(v.id);
  }
  clear(links_list_);
  link_row_ids_.clear();
  for (const auto &v : links) {
    const auto from = unit_names.contains(v.source_unit_id)
                          ? unit_names.at(v.source_unit_id)
                          : "Parte ausente";
    const auto to = unit_names.contains(v.target_unit_id)
                        ? unit_names.at(v.target_unit_id)
                        : "Parte ausente";
    std::string scope = "não classificada";
    for (const auto &d : builder_snapshot_.scopes)
      if (d.link_id == v.id)
        scope = d.scope == "narrative"    ? "ordem do plano narrativo"
                : d.scope == "writing"    ? "ordem de escrita"
                : d.scope == "chronology" ? "cronologia"
                : d.scope == "editorial"  ? "ordem do manuscrito"
                                          : "não classificada";
    links_list_.append(*row_content(from + " " + link_label(v.kind) + " " + to,
                                    v.label + " · ordem a que se aplica: " + scope));
    link_row_ids_.push_back(v.id);
  }
  selected_line_id_.reset();
  selected_unit_id_.reset();
  selected_link_id_.reset();
  remove_line_button_.set_sensitive(false);
  remove_unit_button_.set_sensitive(false);
  membership_button_.set_sensitive(false);
  reality_button_.set_sensitive(false);
  link_button_.set_sensitive(builder_snapshot_.units.size() > 1);
  remove_link_button_.set_sensitive(false);
  dependency_scope_button_.set_sensitive(false);
  auto restore = [](Gtk::ListBox &list, const auto &ids, const auto &wanted) {
    if (!wanted)
      return;
    for (std::size_t i = 0; i < ids.size(); ++i)
      if (ids[i] == *wanted) {
        list.select_row(*list.get_row_at_index(static_cast<int>(i)));
        return;
      }
  };
  restore(lines_list_, line_row_ids_, wanted_line);
  restore(units_list_, unit_row_ids_, wanted_unit);
  restore(links_list_, link_row_ids_, wanted_link);
  if (stack_.get_visible_child_name() == "unit") {
    if (selected_unit_id_)
      refresh_unit();
    else
      stack_.set_visible_child("detail");
  }
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
  auto *dialog = new OverlayDialog("Nova parte da narrativa", *owner, true);
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
  dialog->signal_response().connect(
      [this, dialog, designator, title, summary, purpose, perspective,
       expected = builder_snapshot_,
       line = selected_line_id_.value_or("")](int r) {
        if (r != Gtk::ResponseType::ACCEPT)
          return;
        try {
          project::NarrativeUnit v{};
          v.title = title->get_text();
          v.designator = designator->get_text();
          v.summary = summary->get_text();
          v.purpose = purpose->get_text();
          v.perspective = perspective->get_text();
          auto created = service_.builder().create_unit(expected, v, line);
          selected_unit_id_ = created.id;
          refresh_detail();
        } catch (const std::exception &e) {
          dialog->set_secondary_text(author::diagnostic(e.what()));
          dialog->keep_open_on_response();
        }
      });
  dialog->present();
}
void NarrativeStructureWorkspace::delete_line() {
  if (!selected_line_id_)
    return;
  if (auto *owner = owner_window())
    confirm_project_removal(*owner, service_,
                            project::RemovalKind::NarrativeLine,
                            *selected_line_id_, [this] { refresh_detail(); });
}
void NarrativeStructureWorkspace::delete_unit() {
  if (!selected_unit_id_)
    return;
  if (auto *owner = owner_window())
    confirm_project_removal(*owner, service_,
                            project::RemovalKind::NarrativeUnit,
                            *selected_unit_id_, [this] { refresh_detail(); });
}
void NarrativeStructureWorkspace::connect_unit_line() {
  if (!selected_structure_id_)
    return;
  auto lines = service_.structures().lines(*selected_structure_id_);
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog("Vincular parte a uma linha", *owner, true);
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
      "Pesquisar elemento da história",
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
  role->set_placeholder_text("Função nesta parte (opcional)");
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
        throw std::runtime_error("Escolha um elemento da história para vincular");
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
  dialog->set_secondary_text(
      "Vínculo declarado entre partes. Não altera montagem, datas, ordem de escrita ou apresentação editorial. A precedência legada ainda não classifica qual ordem descreve; explicite isso na observação.");
  auto *source = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &v : units) source->append(v.id, v.title);
  if (selected_unit_id_) source->set_active_id(*selected_unit_id_);
  else if (!units.empty()) source->set_active(0);
  auto *target = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &v : units)
    target->append(v.id, v.title);
  if (units.size() > 1)
    target->set_active(source->get_active_row_number() == 0 ? 1 : 0);
  auto *kind = Gtk::make_managed<Gtk::ComboBoxText>();
  kind->append("precedes", "Vem antes de — ordem ainda não definida");
  kind->append("causes", "Causa");
  kind->append("alternative", "Alternativa");
  kind->append("depends", "Depende de");
  kind->set_active(0);
  auto *label = Gtk::make_managed<Gtk::Entry>();
  label->set_placeholder_text("Observação opcional");
  append_labeled_form_field(*dialog->get_content_area(), "Parte de origem", *source);
  append_labeled_form_field(*dialog->get_content_area(), "Parte de destino", *target);
  append_labeled_form_field(*dialog->get_content_area(), "Relação", *kind);
  append_labeled_form_field(*dialog->get_content_area(), "Observação", *label);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  const auto structure = *selected_structure_id_;
  dialog->signal_response().connect([this, dialog, target, kind, label, structure,
                                     source](int r) {
    if (r != Gtk::ResponseType::ACCEPT)
      return;
    try {
      static_cast<void>(service_.structures().create_link(
          structure, source->get_active_id().raw(), target->get_active_id().raw(),
          project::narrative_link_kind_from_string(kind->get_active_id().raw()),
          label->get_text()));
      refresh_detail();
    } catch (const std::exception &e) {
      dialog->set_secondary_text(author::diagnostic(e.what()));
      dialog->keep_open_on_response();
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

} // namespace inde::ui
