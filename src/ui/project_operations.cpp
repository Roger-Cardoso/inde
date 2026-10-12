#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/application/project_operation_error.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/main_window.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/catalog_vocabulary.hpp"
#include <map>
#include <set>

namespace inde::ui {
namespace {
std::string impact_label(const project::OperationImpact &impact) {
  static const std::map<std::string, std::string> names{
    {"compositions","Montagens de texto"}, {"composition_outputs","Origem e histórico dos documentos criados com montagens"},
    {"entities","Elementos da história"}, {"entity_aliases","Nomes alternativos dos elementos"},
    {"entity_types","Categorias de elemento"}, {"document_anchors","Trechos marcados"},
    {"document_semantics","Marcações do texto e revisão"}, {"semantic_annotations","Marcações do texto"},
    {"semantic_entity_bindings","Associações de marcações a fichas"}, {"semantic_type_definitions","Tipos de marcação"},
    {"editorial_nodes","Elementos do manuscrito"}, {"editorial_structures","Organizações do manuscrito"},
    {"editorial_entity_references","Fichas apresentadas no manuscrito"}, {"editorial_node_trash","Elementos do manuscrito guardados para restauração"},
    {"structural_element_types","Categorias de elemento do manuscrito"}, {"fictional_time_axes","Linhas do tempo"},
    {"fictional_time_points","Marcos nas linhas do tempo"}, {"fictional_calendars","Calendários e relógios"},
    {"axis_calendars","Calendários associados às linhas do tempo"}, {"calendar_revisions","Versões das regras de calendários e relógios"},
    {"narrative_structures","Planos narrativos"}, {"narrative_units","Partes da narrativa"},
    {"narrative_unit_lines","Partes nas linhas narrativas"}, {"narrative_unit_entities","Elementos da história nas partes"},
    {"narrative_realizations","Textos que desenvolvem partes da narrativa"}, {"narrative_text_targets","Trechos e usos de blocos associados ao plano"},
    {"narrative_text_decisions","Conferências entre texto e planejamento"}, {"narrative_continuity_exceptions","Escolhas intencionais do autor"},
    {"narrative_unit_contexts","Lugar e ordem de escrita das partes"}, {"narrative_unit_presence_intents","Presenças planejadas nas partes"},
    {"narrative_unit_relations","Relações em foco nas partes"}, {"narrative_branch_provenance","Origem e histórico das alternativas de planejamento"},
    {"narrative_dependency_scopes","Ordens às quais se aplicam as regras entre partes"}, {"narrative_roles","Papéis na história"},
    {"entity_work_scopes","Elementos associados às obras"}, {"entity_presences","Presenças de elementos da história"}
  };
  if (impact.table == "intellectual_properties") return author::group_name(true, true);
  const auto found=names.find(impact.table);
  return found==names.end() ? impact.label : found->second;
}
std::string removal_caption(const project::OperationPreview &preview) {
  if (preview.reversing) return preview.description; // Historical records stay verbatim.
  using K=project::RemovalKind;
  std::string caption;
  switch (preview.removal) {
  case K::Entity: caption="Excluir elemento da história";break;
  case K::IntellectualProperty: caption="Excluir "+author::group_name();break;
  case K::EditorialBranch: caption="Excluir elemento do manuscrito e suas partes internas";break;
  case K::EditorialStructure: caption="Excluir organização do manuscrito";break;
  case K::NarrativeStructure: caption="Excluir plano narrativo";break;
  case K::NarrativeLine: caption="Excluir linha narrativa";break;
  case K::NarrativeUnit: caption="Excluir parte da narrativa";break;
  case K::NarrativeRole: caption="Excluir papel na história";break;
  case K::EntityType: caption="Excluir categoria de elemento";break;
  case K::RelationType: caption="Excluir tipo de relação";break;
  case K::StructuralType: caption="Excluir categoria de elemento do manuscrito";break;
  case K::TimeAxis: caption="Excluir linha do tempo";break;
  case K::TimePoint: caption="Excluir marco na linha do tempo";break;
  case K::TemporalReference: caption="Excluir data ou período";break;
  case K::CalendarSystem: caption="Excluir esta versão do calendário e relógio";break;
  case K::DocumentAnchor: caption="Retirar trecho marcado";break;
  case K::TextAnnotation: caption="Retirar marcação do texto";break;
  case K::EntityWorkScope: caption="Desvincular elemento da obra";break;
  case K::Document: caption="Excluir documento";break;
  case K::DocumentGroup: caption="Excluir grupo de documentos";break;
  case K::Work: caption="Excluir obra";break;
  case K::Relation: caption="Excluir relação";break;
  case K::CartographicMap: caption="Excluir mapa";break;
  case K::StructureModel: caption="Excluir modelo";break;
  }
  // The store appends the author-chosen target after this separator. Preserve
  // that suffix exactly; never run a vocabulary replacement over a name.
  const auto name=preview.description.find(" — ");
  if (name!=std::string::npos) caption+=preview.description.substr(name);
  return caption;
}
void configure(Gtk::Label &label) {
  label.set_wrap(true);
  label.set_xalign(0);
  label.set_halign(Gtk::Align::START);
  label.set_max_width_chars(68);
}
void retire(OverlayDialog *dialog) {
  dialog->hide();
  Glib::signal_idle().connect_once([dialog] { delete dialog; });
}
std::string effects(const project::OperationPreview &preview) {
  if (!preview.allowed)
    return "Operação bloqueada\n" + preview.blocked_reason;
  std::string text = "Efeitos observados no Projeto:\n";
  if (!preview.reversing &&
      preview.removal == project::RemovalKind::CartographicMap)
    text += "\nO mapa, seu terreno, posições e bloqueios serão excluídos. "
            "Os Lugares cadastrados e seus demais vínculos no Projeto serão "
            "preservados. Exportações não serão apagadas.\n";
  if (!preview.reversing &&
      preview.removal == project::RemovalKind::CalendarSystem)
    text += "\nSomente esta versão do sistema será excluída. Seu sistema de "
            "origem, outros calendários e datas serão preservados.\n";
  for (const auto &i : preview.impacts) {
    text += "\n" + impact_label(i) + ": ";
    if (i.inserted)
      text +=
          std::to_string(i.inserted) +
          (i.inserted == 1 ? " restaurado/criado; " : " restaurados/criados; ");
    if (i.updated)
      text += std::to_string(i.updated) +
              (i.updated == 1 ? " alterado; " : " alterados; ");
    if (i.removed)
      text += std::to_string(i.removed) +
              (i.removed == 1 ? " removido; " : " removidos; ");
    for (const auto &example : i.examples)
      text += "\n  • " + example;
    if (!i.fields.empty()) {
      text += "\n  Campos: ";
      static const std::map<std::string, std::string> fields = {
          {"content", "texto"},
          {"title", "título"},
          {"name", "nome"},
          {"summary", "resumo"},
          {"description", "descrição"},
          {"synopsis", "sinopse"},
          {"subtitle", "subtítulo"},
          {"group_id", "grupo"},
          {"editorial_node_id", "elemento do manuscrito"},
          {"parent_id", "hierarquia"},
          {"updated_at", "data de atualização"},
          {"payload", "camadas e revisão"},
          {"is_active", "estrutura ativa"},
          {"position", "ordem"},
          {"status", "estado"},
          {"purpose", "finalidade"},
          {"perspective", "perspectiva"},
          {"revision_of_id", "fonte da revisão"},
          {"revision_label", "nome da revisão"},
          {"work_id", "Obra"},
          {"structure_id", "estrutura"},
          {"type_id", "tipo"},
          {"designator", "designação"},
          {"notes", "notas"},
          {"label", "rótulo"},
          {"start_offset", "início do trecho"},
          {"end_offset", "fim do trecho"}};
      std::set<std::string> descriptions;
      for (const auto &field : i.fields) {
        const auto found = fields.find(field);
        descriptions.insert(found == fields.end() ? "outros metadados"
                                                  : found->second);
      }
      for (const auto &description : descriptions)
        text += description + "; ";
    }
  }
  return text + "\n\nA operação não é o desfazer da digitação. A confirmação "
                "revalida estes efeitos; conflitos ou novos usos bloqueiam a "
                "reversão. Arquivos externos e exportações não são revertidos.";
}
} // namespace
OperationConfirmation::OperationConfirmation(
    application::ProjectService &service, project::OperationPreview preview,
    std::function<void()> completed,
    std::function<void(const project::OperationPreview &,
                       const project::RemovalUse &)>
        open_use)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 12), service_(service),
      preview_(std::move(preview)), completed_(std::move(completed)),
      open_use_(std::move(open_use)) {
  for (auto *l : {&description_, &effects_, &message_})
    configure(*l);
  description_.add_css_class("title-3");
  message_.add_css_class("secondary-text");
  set_accessible_label(acknowledge_,
                       "Reconhecer os efeitos da operação do Projeto");
  using K=project::RemovalKind;
  confirm_.set_label(preview_.reversing ? "Reverter operação do Projeto"
      : preview_.removal==K::EntityWorkScope ? "Desvincular e registrar"
      : preview_.removal==K::DocumentAnchor || preview_.removal==K::TextAnnotation ? "Retirar e registrar"
      : "Excluir e registrar");
  confirm_.add_css_class("destructive-action");
  actions_.append(refresh_);
  actions_.append(confirm_);
  append(description_);
  append(effects_);
  configure(usage_heading_);
  append(usage_heading_);
  usage_search_.set_placeholder_text("Filtrar usos pelo nome ou local");
  append(usage_search_);
  pagination_.append(previous_);
  pagination_.append(next_);
  append(pagination_);
  append(uses_);
  append(acknowledge_);
  append(actions_);
  append(message_);
  acknowledge_.signal_toggled().connect([this] {
    confirm_.set_sensitive(preview_.allowed && acknowledge_.get_active() &&
                           !committed_);
  });
  refresh_.signal_clicked().connect([this] { prepare(); });
  confirm_.signal_clicked().connect([this] { confirm(); });
  previous_.signal_clicked().connect([this] {
    if (usage_offset_ >= 10)
      usage_offset_ -= 10;
    show_uses();
  });
  next_.signal_clicked().connect([this] {
    usage_offset_ += 10;
    show_uses();
  });
  usage_search_.signal_search_changed().connect([this] {
    usage_offset_ = 0;
    show_uses();
  });
  prepare();
}
void OperationConfirmation::show_uses() {
  while (auto *child = uses_.get_first_child())
    uses_.remove(*child);
  std::vector<project::RemovalUse> items;
  const auto query = usage_search_.get_text().casefold();
  for (const auto &u : preview_.uses) {
    const auto value = Glib::ustring(u.name + " " + u.path).casefold();
    if (query.empty() || value.find(query) != Glib::ustring::npos)
      items.push_back(u);
  }
  if (usage_offset_ >= items.size())
    usage_offset_ = 0;
  usage_heading_.set_visible(!preview_.uses.empty());
  usage_search_.set_visible(!preview_.uses.empty());
  pagination_.set_visible(items.size() > 10);
  previous_.set_sensitive(usage_offset_ > 0);
  next_.set_sensitive(usage_offset_ + 10 < items.size());
  usage_heading_.set_text(
      "Auxiliar de exclusão · " + std::to_string(items.size()) + " de " +
      std::to_string(preview_.uses.size()) + " usos · " +
      std::to_string(items.empty() ? 0 : usage_offset_ + 1) + "–" +
      std::to_string(std::min(items.size(), usage_offset_ + 10)) +
      "\nAbra um local, resolva o vínculo e use Voltar à exclusão. Nenhum "
      "vínculo é removido automaticamente.");
  for (std::size_t i = usage_offset_;
       i < std::min(items.size(), usage_offset_ + 10); ++i) {
    const auto use = items[i];
    auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    card->add_css_class("content-card");
    auto *name = Gtk::make_managed<Gtk::Label>(use.name);
    configure(*name);
    name->set_lines(3);
    name->set_ellipsize(Pango::EllipsizeMode::END);
    name->set_tooltip_text(use.name);
    name->add_css_class("heading");
    card->append(*name);
    auto *path = Gtk::make_managed<Gtk::Label>(use.path);
    configure(*path);
    path->set_lines(3);
    path->set_ellipsize(Pango::EllipsizeMode::END);
    path->set_tooltip_text(use.path);
    card->append(*path);
    auto *guide = Gtk::make_managed<Gtk::Label>(use.guidance);
    configure(*guide);
    card->append(*guide);
    if (open_use_ && !use.location_table.empty() && !use.location_id.empty()) {
      auto *open = Gtk::make_managed<Gtk::Button>("Abrir local do uso");
      open->set_halign(Gtk::Align::START);
      open->set_tooltip_text(use.path + " — " + use.name);
      open->signal_clicked().connect([this, use] {
        try {
          open_use_(preview_, use);
        } catch (const std::exception &e) {
          message_.set_text(std::string("Não foi possível abrir: ") + author::diagnostic(e.what()));
        }
      });
      card->append(*open);
    }
    uses_.append(*card);
  }
}
void OperationConfirmation::prepare() {
  acknowledge_.set_active(false);
  confirm_.set_sensitive(false);
  message_.set_text("Nada foi confirmado nesta prévia.");
  try {
    if (!preview_.project_id.empty() &&
        (!service_.current() ||
         service_.current()->manifest().project_id != preview_.project_id))
      throw std::runtime_error(
          "O Projeto ativo mudou; reabra a exclusão no Projeto de origem.");
    preview_ =
        preview_.reversing
            ? service_.reversal_impact(preview_.operation_id)
            : service_.removal_impact(preview_.removal, preview_.target_id);
  } catch (const std::exception &e) {
    preview_.allowed = false;
    preview_.blocked_reason = author::diagnostic(e.what());
    preview_.uses.clear();
  }
  description_.set_text(removal_caption(preview_));
  description_.set_tooltip_text(preview_.description);
  effects_.set_text(effects(preview_));
  acknowledge_.set_sensitive(preview_.allowed && !committed_);
  show_uses();
}
void OperationConfirmation::confirm() {
  if (committed_ || !preview_.allowed || !acknowledge_.get_active())
    return;
  confirm_.set_sensitive(false);
  try {
    service_.confirm_project_operation(preview_);
  } catch (const application::ProjectOperationCommittedError &e) {
    committed_ = true;
    refresh_.set_sensitive(false);
    acknowledge_.set_sensitive(false);
    message_.set_text(author::diagnostic(e.what()));
    try {
      if (completed_)
        completed_();
    } catch (const std::exception &) {
    }
    return;
  } catch (const std::exception &e) {
    message_.set_text(std::string("Não confirmado: ") + author::diagnostic(e.what()));
    acknowledge_.set_active(false);
    return;
  }
  committed_ = true;
  refresh_.set_sensitive(false);
  acknowledge_.set_sensitive(false);
  message_.set_text(
      "Operação confirmada e registrada no histórico do Projeto.");
  try {
    if (completed_)
      completed_();
  } catch (const std::exception &e) {
    message_.set_text(
        std::string("Operação confirmada; atualização da interface "
                    "indisponível. Reabra o Projeto: ") +
        author::diagnostic(e.what()));
  }
}
ProjectOperationsView::ProjectOperationsView(
    application::ProjectService &service, std::function<void()> changed)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 10), service_(service),
      changed_(std::move(changed)) {
  configure(info_);
  configure(empty_);
  info_.set_text("Histórico das operações novas de cadastro e organização. Não "
                 "substitui Desfazer edição, não oferece refazer automático e "
                 "não torna registros antigos reversíveis retroativamente.");
  rows_.set_selection_mode(Gtk::SelectionMode::NONE);
  rows_.add_css_class("boxed-list");
  queue_.append(info_);
  queue_.append(rows_);
  queue_.append(empty_);
  pagination_.append(previous_);
  pagination_.append(next_);
  queue_.append(pagination_);
  detail_.append(back_);
  back_.set_halign(Gtk::Align::START);
  back_.add_css_class("flat");
  pages_.set_hhomogeneous(false);
  pages_.set_vhomogeneous(
      false); // A short detail must not inherit the list height.
  pages_.add(queue_, "queue");
  pages_.add(detail_, "detail");
  append(pages_);
  previous_.signal_clicked().connect([this] {
    if (offset_ >= 25)
      offset_ -= 25;
    list();
  });
  next_.signal_clicked().connect([this] {
    offset_ += 25;
    list();
  });
  back_.signal_clicked().connect([this] { list(); });
  list();
}
void ProjectOperationsView::list() {
  while (auto *child = rows_.get_first_child())
    rows_.remove(*child);
  try {
    const auto rows = service_.project_operations(offset_);
    for (const auto &row : rows) {
      std::string state = row.state == "reverted" ? "revertida"
                          : row.compensation      ? "reversão registrada"
                                                  : "registrada";
      if (service_.current() &&
          row.capture_project_id != service_.current()->manifest().project_id)
        state = "histórico herdado — não reversível nesta cópia";
      auto *button = Gtk::make_managed<Gtk::Button>();
      auto *label = Gtk::make_managed<Gtk::Label>(
          row.label + "\n" + row.created_at + " · " + state);
      configure(*label);
      button->set_child(*label);
      button->signal_clicked().connect([this, id = row.id] { detail(id); });
      rows_.append(*button);
    }
    empty_.set_text(rows.empty() ? "Nenhuma operação nesta página." : "");
    previous_.set_sensitive(offset_ != 0);
    next_.set_sensitive(rows.size() == 25);
  } catch (const std::exception &e) {
    empty_.set_text(std::string("Histórico indisponível: ") + author::diagnostic(e.what()));
    next_.set_sensitive(false);
  }
  pages_.set_visible_child("queue");
}
void ProjectOperationsView::detail(const std::string &id) {
  auto *child = back_.get_next_sibling();
  while (child) {
    auto *next = child->get_next_sibling();
    detail_.remove(*child);
    child = next;
  }
  try {
    auto *panel = Gtk::make_managed<OperationConfirmation>(
        service_, service_.reversal_impact(id), changed_);
    detail_.append(*panel);
    pages_.set_visible_child("detail");
  } catch (const std::exception &e) {
    empty_.set_text(std::string("Prévia indisponível: ") + author::diagnostic(e.what()));
  }
}
void confirm_project_removal(Gtk::Window &owner,
                             application::ProjectService &service,
                             project::RemovalKind kind, const std::string &id,
                             std::function<void()> completed) {
  auto *dialog = new OverlayDialog("Impacto da remoção", owner);
  dialog->set_secondary_text(
      "Confira antes de alterar o Projeto. Cancelar não confirma a operação.");
  dialog->add_button("Cancelar / fechar", Gtk::ResponseType::CANCEL);
  project::OperationPreview p;
  p.removal = kind;
  p.target_id = id;
  std::function<void(const project::OperationPreview &,
                     const project::RemovalUse &)>
      open;
  if (auto *window = dynamic_cast<MainWindow *>(&owner))
    open = [window, dialog, completed](const auto &preview, const auto &use) {
      dialog->hide();
      try {
        window->navigate_removal_use(preview, use, completed);
      } catch (...) {
        dialog->present();
        throw;
      }
      retire(dialog);
    };
  auto *panel = Gtk::make_managed<OperationConfirmation>(
      service, std::move(p),
      [dialog, completed = std::move(completed)] {
        if (completed)
          completed();
        retire(dialog);
      },
      std::move(open));
  dialog->get_content_area()->append(*panel);
  // OverlayDialog already hides and deletes itself after a footer/Escape
  // response. Only the successful body action uses deferred retirement.
  dialog->present();
}
} // namespace inde::ui
