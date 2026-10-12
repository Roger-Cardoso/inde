#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/narrative_structure_workspace.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/ui/work_setup.hpp"
#include "narrative_structure_ui_helpers.hpp"

#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>

namespace inde::ui {
using narrative_structure_ui_detail::clear;
using narrative_structure_ui_detail::row_content;
namespace {
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
} // namespace

void NarrativeStructureWorkspace::refresh_library() {
  const auto wanted_structure = selected_structure_id_;
  comparison_checks_.clear();
  clear(models_list_);
  clear(structures_list_);
  model_row_ids_.clear();
  structure_row_ids_.clear();
  selected_model_id_.reset();
  if (!service_.current())
    return;
  try {
    for (const auto &value :
         service_.structures().models(project::StructureLayer::Narrative)) {
      add_model_row(value);
    }
  } catch (const std::exception &e) {
    show_error(e);
    return;
  }
  const auto work = selected_work_id();
  create_button_.set_sensitive(service_.current() != nullptr);
  setup_box_.set_visible(work.empty());
  structures_panel_.set_visible(!work.empty());
  update_comparison_selection();
  if (work.empty())
    return;
  try {
    for (const auto &value : service_.structures().narrative_structures(work)) {
      std::ostringstream detail;
      detail << (value.is_active ? "ATIVA · " : "")
             << origin_label(value.creation_kind);
      const auto unit_count = service_.structures().units(value.id).size();
      const auto line_count = service_.structures().lines(value.id).size();
      detail << " · " << unit_count << " partes · " << line_count
             << " linhas";
      if (!value.description.empty())
        detail << "\n" << value.description;
      add_structure_row(value, detail.str());
    }
    if (selection_source_.empty() && wanted_structure)
      for (std::size_t i = 0; i < structure_row_ids_.size(); ++i)
        if (structure_row_ids_[i] == *wanted_structure)
          structures_list_.select_row(
              *structures_list_.get_row_at_index(static_cast<int>(i)));
  } catch (const std::exception &e) {
    show_error(e);
  }
}

void NarrativeStructureWorkspace::delete_model() {
  if (!selected_model_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  const auto id = *selected_model_id_;
  confirm_project_removal(
      *owner, service_, project::RemovalKind::StructureModel, id, [this] {
        selected_model_id_.reset();
        refresh_library();
        signal_status_message_.emit("Modelo narrativo do usuário removido.");
      });
}
void NarrativeStructureWorkspace::create_blank() {
  if (selected_work_id().empty()) {
    start_work();
    return;
  }
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Novo plano narrativa", *owner, true);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome da alternativa");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição opcional");
  dialog->get_content_area()->append(*name);
  dialog->get_content_area()->append(*description);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  dialog->signal_response().connect(
      [this, dialog, name, description](int response) {
        if (response != Gtk::ResponseType::ACCEPT)
          return;
        try {
          static_cast<void>(service_.structures().create_narrative(
              selected_work_id(), name->get_text(), description->get_text()));
          refresh_library();
          signal_status_message_.emit("Plano narrativo vazio criado.");
        } catch (const std::exception &e) {
          dialog->set_secondary_text(author::diagnostic(e.what()));
          dialog->keep_open_on_response();
        }
      });
  dialog->present();
}
void NarrativeStructureWorkspace::instantiate_model() {
  if (selected_work_id().empty()) {
    const auto model = selected_model_id_;
    if (auto *owner = owner_window())
      show_work_setup(*owner, service_, [this, model](const std::string &id) {
        refresh_works();
        work_combo_.set_active_id(id);
        refresh_library();
        selected_model_id_ = model;
        signal_catalog_changed_.emit();
        Glib::signal_idle().connect_once(
            sigc::track_obj([this] { instantiate_model(); }, *this));
      });
    return;
  }
  auto *owner = owner_window();
  if (!owner)
    return;
  auto models =
      service_.structures().models(project::StructureLayer::Narrative);
  auto *dialog = new OverlayDialog("Aplicar modelo narrativo", *owner, true);
  auto *combo = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &v : models)
    combo->append(v.id, v.name + (v.is_builtin ? " · Sistema" : " · Usuário"));
  if (selected_model_id_)
    combo->set_active_id(*selected_model_id_);
  if (combo->get_active_row_number() < 0 && !models.empty())
    combo->set_active(0);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome do novo plano");
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
          "Plano criado a partir do modelo; o plano atual não foi alterado.");
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
      derived ? "Criar alternativa a partir deste plano" : "Duplicar plano", *owner, true);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome da nova alternativa");
  dialog->get_content_area()->append(*name);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button(derived ? "Criar alternativa" : "Duplicar",
                     Gtk::ResponseType::ACCEPT);
  const auto id = *selected_structure_id_;
  application::NarrativeBuilderSnapshot expected;
  try {
    expected = service_.builder().snapshot(id);
  } catch (const std::exception &e) {
    delete dialog;
    show_error(e);
    return;
  }
  dialog->set_secondary_text(
      "Uma nova alternativa de planejamento, sem ativação automática.");
  auto *policy = Gtk::make_managed<Gtk::Label>(
      "Copia linhas, partes, ordens próprias, datas, papéis, Local, fila, relações em foco e vínculos textuais. Documentos, elementos da história, relações e referências temporais continuam compartilhados; editar esses cadastros pode afetar ambas as alternativas. A nova alternativa não será ativada nem mudará o editorial.");
  policy->set_xalign(0);
  policy->set_wrap(true);
  policy->set_max_width_chars(72);
  dialog->get_content_area()->append(*policy);
  auto *shared = Gtk::make_managed<Gtk::CheckButton>(
      "Entendo que texto e cadastros permanecem compartilhados");
  dialog->get_content_area()->append(*shared);
  dialog->signal_response().connect(
      [this, dialog, expected, name, shared, derived](int response) {
        if (response != Gtk::ResponseType::ACCEPT)
          return;
        try {
          if (!shared->get_active())
            throw std::runtime_error("Confirme a política de compartilhamento "
                                     "antes de criar a alternativa.");
          static_cast<void>(
              service_.builder().derive(expected, name->get_text(), derived));
          refresh_library();
          signal_status_message_.emit(
              derived ? "Estrutura derivada com origem e histórico preservados."
                      : "Plano duplicado com novas identificações; textos e cadastros continuam compartilhados.");
        } catch (const std::exception &e) {
          dialog->set_secondary_text(author::diagnostic(e.what()));
          dialog->keep_open_on_response();
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
  dialog->set_secondary_text(
      "Modelo é um esqueleto reutilizável de linhas, partes e vínculos; não é uma cópia completa da alternativa. Não transporta datas, papéis, ordem de escrita, Locais, presenças planejadas, relações em foco, exceções de continuidade ou realizações textuais.");
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
      [this, dialog, id, name, description](int response) {
        if (response != Gtk::ResponseType::ACCEPT)
          return;
        try {
          static_cast<void>(service_.structures().capture_narrative_model(
              id, name->get_text(), description->get_text()));
          refresh_library();
          signal_status_message_.emit("Modelo narrativo reutilizável criado "
                                      "com linhas, pertencimentos e vínculos.");
        } catch (const std::exception &e) {
          dialog->set_secondary_text(author::diagnostic(e.what()));
          dialog->keep_open_on_response();
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
      signal_status_message_.emit("Este plano narrativo já está ativo.");
      return;
    }
    auto *owner = owner_window();
    if (!owner)
      return;
    auto *dialog =
        new OverlayDialog("Usar este plano como plano atual", *owner, true);
    dialog->set_secondary_text(
        "A seleção ativa da Obra será trocada. Nenhuma alternativa será "
        "apagada; a estrutura alvo possui " +
        std::to_string(impact.target_units) + " partes.");
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    dialog->add_button("Usar como plano atual", Gtk::ResponseType::ACCEPT);
    const auto id = *selected_structure_id_;
    dialog->signal_response().connect([this, id](int response) {
      if (response != Gtk::ResponseType::ACCEPT)
        return;
      try {
        service_.structures().activate_narrative(id);
        refresh_library();
        signal_status_message_.emit("Plano narrativo atual atualizado.");
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
  const auto id = *selected_structure_id_;
  confirm_project_removal(
      *owner, service_, project::RemovalKind::NarrativeStructure, id, [this] {
        selected_structure_id_.reset();
        refresh_library();
        signal_status_message_.emit("Plano narrativo excluído.");
      });
}

} // namespace inde::ui
