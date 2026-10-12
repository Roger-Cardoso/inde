#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/writing_workspace.hpp"

#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/project_operations.hpp"

#include "inde/project/manifest.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <set>

#include "writing_workspace_util.hpp"

namespace inde::ui {

namespace {
void retire_text_reference_dialog(OverlayDialog *dialog) {
  dialog->hide();
  Glib::signal_idle().connect_once([dialog] { delete dialog; });
}
} // namespace

void WritingWorkspace::clear_anchor_marks() {
  auto buffer = editor_.get_buffer();
  for (auto &anchor : anchor_marks_) {
    if (anchor.start)
      buffer->delete_mark(anchor.start);
    if (anchor.end)
      buffer->delete_mark(anchor.end);
  }
  anchor_marks_.clear();
  selected_anchor_id_.reset();
}

void WritingWorkspace::restore_anchor_marks(
    const std::vector<project::DocumentAnchor> &anchors) {
  clear_anchor_marks();
  auto buffer = editor_.get_buffer();
  const auto total = static_cast<std::size_t>(buffer->get_char_count());
  for (const auto &anchor : anchors) {
    bool start_left=true,end_left=false;
    if(current_document_)for(auto&e:current_document_->semantics.anchors)if(e.anchor_id==anchor.id){start_left=e.start_gravity==project::AnchorGravity::Left;end_left=e.end_gravity==project::AnchorGravity::Left;break;}
    const auto start = std::min(anchor.start_offset, total);
    const auto end = std::min(anchor.end_offset, total);
    anchor_marks_.push_back(
        {anchor.id, anchor.label,
         buffer->create_mark(
             "anchor-start-" + anchor.id,
             buffer->get_iter_at_offset(static_cast<int>(start)), start_left),
         buffer->create_mark("anchor-end-" + anchor.id,
                             buffer->get_iter_at_offset(static_cast<int>(end)),
                             end_left)});
  }
}

std::vector<project::DocumentAnchor> WritingWorkspace::capture_anchors() {
  std::vector<project::DocumentAnchor> result;
  auto buffer = editor_.get_buffer();
  for (const auto &anchor : anchor_marks_) {
    const auto start = buffer->get_iter_at_mark(anchor.start).get_offset();
    const auto end = buffer->get_iter_at_mark(anchor.end).get_offset();
    result.push_back(
        {anchor.id, anchor.label,
         static_cast<std::size_t>(std::max(0, std::min(start, end))),
         static_cast<std::size_t>(std::max(0, std::max(start, end)))});
  }
  std::sort(result.begin(), result.end(),
            [](const auto &left, const auto &right) {
              if (left.start_offset != right.start_offset)
                return left.start_offset < right.start_offset;
              return left.label < right.label;
            });
  return result;
}

void WritingWorkspace::refresh_anchor_list() {
  clear_list(anchors_list_);
  for (const auto &anchor : capture_anchors()) {
    auto *button = Gtk::make_managed<Gtk::Button>(anchor.label);
    button->set_halign(Gtk::Align::FILL);
    button->set_tooltip_text(
        anchor.start_offset == anchor.end_offset
            ? "Posição " + std::to_string(anchor.start_offset)
            : "Trecho " + std::to_string(anchor.start_offset) + "–" +
                  std::to_string(anchor.end_offset));
    button->signal_clicked().connect([this, id = anchor.id] {
      select_anchor(id);
      open_selected_anchor();
    });
    anchors_list_.append(*button);
  }
  if (anchor_marks_.empty()) {
    auto *empty = Gtk::make_managed<Gtk::Label>(
        "Selecione um trecho ou posicione o cursor para criar um trecho marcado.");
    prepare_tool_label(*empty);
    empty->set_margin(8);
    anchors_list_.append(*empty);
  }
  const bool selected = selected_anchor_id_.has_value();
  rename_anchor_button_.set_sensitive(selected);
  remove_anchor_button_.set_sensitive(selected);
  anchor_usages_button_.set_sensitive(selected);
}

void WritingWorkspace::select_anchor(std::string id) {
  selected_anchor_id_ = std::move(id);
  refresh_anchor_list();
}

void WritingWorkspace::open_selected_anchor() {
  if (!selected_anchor_id_)
    return;
  const auto found = std::find_if(
      anchor_marks_.begin(), anchor_marks_.end(),
      [&](const auto &anchor) { return anchor.id == *selected_anchor_id_; });
  if (found == anchor_marks_.end())
    return;
  auto buffer = editor_.get_buffer();
  auto start = buffer->get_iter_at_mark(found->start);
  auto end = buffer->get_iter_at_mark(found->end);
  if (start.get_offset() == end.get_offset())
    buffer->place_cursor(start);
  else
    buffer->select_range(start, end);
  editor_.scroll_to(start, 0.2);
  editor_.grab_focus();
}

void WritingWorkspace::add_anchor() {
  if (!current_document_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto buffer = editor_.get_buffer();
  Gtk::TextBuffer::iterator start;
  Gtk::TextBuffer::iterator end;
  if (!buffer->get_selection_bounds(start, end)) {
    start = buffer->get_iter_at_mark(buffer->get_insert());
    end = start;
  }
  const auto start_offset = start.get_offset();
  const auto end_offset = end.get_offset();
  auto *dialog = new OverlayDialog("Marcar trecho textual", *owner, true);
  dialog->set_secondary_text(
      start_offset == end_offset
          ? "O trecho marcado acompanhará esta posição durante a edição."
          : "O trecho marcado acompanhará o trecho selecionado durante a edição.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar trecho marcado", Gtk::ResponseType::ACCEPT);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome do trecho marcado");
  name->set_margin(16);
  dialog->get_content_area()->append(*name);
  dialog->signal_response().connect(
      [this, dialog, name, start_offset, end_offset](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            if (name->get_text().empty())
              throw std::runtime_error("O nome do trecho marcado é obrigatório");
            const auto id = project::new_uuid();
            auto buffer = editor_.get_buffer();
            anchor_marks_.push_back(
                {id, name->get_text(),
                 buffer->create_mark("anchor-start-" + id,
                                     buffer->get_iter_at_offset(start_offset),
                                     true),
                 buffer->create_mark("anchor-end-" + id,
                                     buffer->get_iter_at_offset(end_offset),
                                     false)});
            selected_anchor_id_ = id;
            refresh_anchor_list();
            mark_dirty();
          } catch (const std::exception &error) {
            show_error("Não foi possível criar o trecho marcado", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void WritingWorkspace::rename_anchor() {
  if (!selected_anchor_id_)
    return;
  auto found = std::find_if(
      anchor_marks_.begin(), anchor_marks_.end(),
      [&](const auto &anchor) { return anchor.id == *selected_anchor_id_; });
  auto *owner = owner_window();
  if (!owner || found == anchor_marks_.end())
    return;
  auto *dialog = new OverlayDialog("Renomear trecho marcado", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text(found->label);
  name->set_margin(16);
  dialog->get_content_area()->append(*name);
  const auto id = found->id;
  dialog->signal_response().connect([this, dialog, name, id](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (name->get_text().empty())
          throw std::runtime_error("O nome do trecho marcado é obrigatório");
        auto current =
            std::find_if(anchor_marks_.begin(), anchor_marks_.end(),
                         [&](const auto &anchor) { return anchor.id == id; });
        if (current != anchor_marks_.end())
          current->label = name->get_text();
        refresh_anchor_list();
        refresh_entity_references();
        mark_dirty();
      } catch (const std::exception &error) {
        show_error("Não foi possível renomear o trecho marcado", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void WritingWorkspace::remove_anchor() {
  if (!selected_anchor_id_)
    return;
  const auto id = *selected_anchor_id_;
  try {
    if(current_document_&&std::any_of(current_document_->semantics.anchors.begin(),current_document_->semantics.anchors.end(),[&](auto&e){return e.anchor_id==id;}))throw std::runtime_error("Este trecho marcado pertence a uma marcação do texto. Remova ou localize novamente o trecho da marcação em Marcações do texto.");
    if (current_document_ &&
        service_.writing()
                .text_references({current_document_->id, true, id, 1, 0})
                .total != 0)
      throw std::runtime_error(
          "Este trecho marcado é referenciado por outros documentos. Abra Onde este trecho é usado e desvincule as referências antes de retirá-lo.");
  } catch (const std::exception &error) {
    try {
      flush_changes();
      const auto preview = service_.removal_impact(project::RemovalKind::DocumentAnchor, id);
      if (preview.uses.empty()) show_error("Não foi possível remover o trecho marcado", error);
      else confirm_project_removal(*owner_window(), service_, project::RemovalKind::DocumentAnchor, id, {});
    } catch (const std::exception &e) { show_error("Trecho marcado preservada", e); }
    return;
  }
  auto found =
      std::find_if(anchor_marks_.begin(), anchor_marks_.end(),
                   [&](const auto &anchor) { return anchor.id == id; });
  if (found == anchor_marks_.end())
    return;
  auto buffer = editor_.get_buffer();
  buffer->delete_mark(found->start);
  buffer->delete_mark(found->end);
  anchor_marks_.erase(found);
  for (auto &reference : entity_references_) {
    if (reference.anchor_id == id)
      reference.anchor_id.reset();
  }
  selected_anchor_id_.reset();
  refresh_anchor_list();
  refresh_entity_references();
  mark_dirty();
}

std::optional<std::string> WritingWorkspace::selected_anchor_label(
    const std::optional<std::string> &id) const {
  if (!id)
    return std::nullopt;
  const auto found =
      std::find_if(anchor_marks_.begin(), anchor_marks_.end(),
                   [&](const auto &anchor) { return anchor.id == *id; });
  return found == anchor_marks_.end()
             ? std::optional<std::string>{"Trecho indisponível"}
             : std::optional<std::string>{found->label};
}

void WritingWorkspace::refresh_entity_references() {
  clear_list(references_list_);
  for (const auto &reference : entity_references_) {
    auto label = entity_label(reference.entity_id);
    if (const auto anchor = selected_anchor_label(reference.anchor_id))
      label += " — em “" + *anchor + "”";
    else
      label += " — Documento inteiro";
    auto *button = Gtk::make_managed<Gtk::Button>(label);
    button->set_halign(Gtk::Align::FILL);
    button->set_tooltip_text(reference.notes.empty()
                                 ? "Referência explícita criada pelo usuário"
                                 : reference.notes);
    button->signal_clicked().connect(
        [this, id = reference.id] { select_entity_reference(id); });
    references_list_.append(*button);
  }
  std::size_t semantic_references{};
  if(current_document_) {
    const auto types=service_.writing().semantic_types();
    for(const auto &a:current_document_->semantics.annotations) {
      if(a.status=="archived") continue;
      std::set<std::string> ids;
      for(const auto &[key,f]:a.fields) {(void)key;
        if(f.kind==project::SemanticFieldKind::Entity) ids.insert(f.values.begin(),f.values.end());}
      for(const auto &id:ids) {
        ++semantic_references;const auto *type=project::find_annotation_type(types,a.type_key,a.type_version);
        auto *button=Gtk::make_managed<Gtk::Button>(entity_label(id)+" — "+(type?type->label:a.type_key)+" (marcação do texto)");
        button->set_tooltip_text("Revisar a associação na marcação do texto. Não é um segundo vínculo independente.");
        button->signal_clicked().connect([this,id=a.id]{show_semantic_annotation(id);});references_list_.append(*button);
      }
    }
  }
  if (entity_references_.empty() && !semantic_references) {
    auto *empty = Gtk::make_managed<Gtk::Label>(
        "Nenhum elemento foi associado explicitamente ao Documento.");
    prepare_tool_label(*empty);
    empty->set_margin(8);
    references_list_.append(*empty);
  }
  const bool selected = selected_entity_reference_id_.has_value();
  open_reference_button_.set_sensitive(selected);
  remove_reference_button_.set_sensitive(selected);
}

void WritingWorkspace::select_entity_reference(std::string id) {
  selected_entity_reference_id_ = std::move(id);
  refresh_entity_references();
}

void WritingWorkspace::add_entity_reference() {
  if (!current_document_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog("Vincular elemento da história ao Documento", *owner, true);
  dialog->set_secondary_text(
      "O vínculo é explícito e não altera o texto nem os dados do elemento da história.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Vincular", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *entity = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar elemento da história",
      [this](const std::string &search, std::size_t limit) {
        std::vector<IncrementalSelection> result;
        persistence::EntityQuery query;
        query.search = search;
        query.limit = std::min(limit, std::size_t{50});
        for (const auto &value : service_.narrative().entities(query))
          result.push_back({value.id, value.name, value.summary});
        return result;
      });
  auto *anchor = Gtk::make_managed<Gtk::ComboBoxText>();
  anchor->append("none", "Documento inteiro");
  for (const auto &value : capture_anchors())
    anchor->append(value.id, value.label);
  anchor->set_active_id(selected_anchor_id_.value_or("none"));
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_placeholder_text("Nota opcional sobre o uso");
  form->set_margin(16);
  append_labeled_form_field(*form, "Elemento da história", *entity,
                            "Pesquise e escolha um elemento da história.");
  append_labeled_form_field(
      *form, "Trecho marcado textual", *anchor,
      "Use Documento inteiro quando não houver trecho específico.");
  append_labeled_form_field(*form, "Nota", *notes);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, entity, anchor,
                                     notes](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (!entity->selected_id())
          throw std::runtime_error("Escolha um elemento da história para vincular");
        const auto anchor_id = anchor->get_active_id().raw();
        project::DocumentEntityReference reference{
            project::new_uuid(), *entity->selected_id(),
            anchor_id.empty() || anchor_id == "none"
                ? std::nullopt
                : std::optional<std::string>{anchor_id},
            notes->get_text()};
        const auto duplicate =
            std::find_if(entity_references_.begin(), entity_references_.end(),
                         [&](const auto &existing) {
                           return existing.entity_id == reference.entity_id &&
                                  existing.anchor_id == reference.anchor_id;
                         });
        if (duplicate != entity_references_.end())
          throw std::runtime_error(
              "O elemento da história já está vinculada ao mesmo contexto");
        entity_references_.push_back(reference);
        selected_entity_reference_id_ = reference.id;
        refresh_entity_references();
        mark_dirty();
      } catch (const std::exception &error) {
        show_error("Não foi possível vincular o elemento da história", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void WritingWorkspace::remove_entity_reference() {
  if (!selected_entity_reference_id_)
    return;
  const auto id = *selected_entity_reference_id_;
  entity_references_.erase(
      std::remove_if(entity_references_.begin(), entity_references_.end(),
                     [&](const auto &reference) { return reference.id == id; }),
      entity_references_.end());
  selected_entity_reference_id_.reset();
  refresh_entity_references();
  mark_dirty();
}

void WritingWorkspace::open_reference_entity() {
  if (!selected_entity_reference_id_)
    return;
  const auto found =
      std::find_if(entity_references_.begin(), entity_references_.end(),
                   [&](const auto &reference) {
                     return reference.id == *selected_entity_reference_id_;
                   });
  if (found == entity_references_.end())
    return;
  try {
    flush_changes();
    signal_entity_source_requested_.emit(found->entity_id);
  } catch (const std::exception &error) {
    show_error("Não foi possível salvar antes de navegar", error);
  }
}

void WritingWorkspace::show_text_references(
    bool incoming, std::optional<std::string> anchor_id, std::size_t offset) {
  if (!current_document_ || !owner_window())
    return;
  try {
    flush_changes();
    const auto page = service_.writing().text_references(
        {current_document_->id, incoming, anchor_id, 50, offset});
    auto *dialog = new OverlayDialog(anchor_id ? "Onde este trecho é usado"
                                               : "Referências textuais",
                                     *owner_window(), true);
    dialog->set_secondary_text(
        current_document_->title + " — " +
        (incoming ? "Documentos que referenciam esta fonte."
                  : "Fontes referenciadas por este Documento.") +
        " O vínculo não incorpora nem copia o texto. Alterações de vínculo são "
        "salvas imediatamente.");
    dialog->add_button("Fechar", Gtk::ResponseType::CLOSE);
    auto *body = dialog->get_content_area();
    body->set_margin(16);
    auto *modes = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    auto *out = Gtk::make_managed<Gtk::Button>("Referências deste Documento");
    auto *in = Gtk::make_managed<Gtk::Button>("Usos deste Documento");
    out->signal_clicked().connect([this, dialog] {
      retire_text_reference_dialog(dialog);
      show_text_references(false);
    });
    in->signal_clicked().connect([this, dialog] {
      retire_text_reference_dialog(dialog);
      show_text_references(true);
    });
    modes->append(*out);
    modes->append(*in);
    body->append(*modes);
    if (!incoming) {
      auto *add =
          Gtk::make_managed<Gtk::Button>("Referenciar Documento ou trecho marcado");
      add->signal_clicked().connect([this, dialog] {
        retire_text_reference_dialog(dialog);
        add_text_reference();
      });
      body->append(*add);
    }
    auto *summary = Gtk::make_managed<Gtk::Label>(
        page.total == 0
            ? "Nenhuma referência explícita neste contexto."
            : "Exibindo " +
                  std::to_string(page.items.empty() ? 0 : offset + 1) + "–" +
                  std::to_string(offset + page.items.size()) + " de " +
                  std::to_string(page.total) + " referências.");
    prepare_tool_label(*summary);
    body->append(*summary);
    for (const auto &item : page.items) {
      auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
      auto *label = Gtk::make_managed<Gtk::Label>(
          item.source_title + " → " + item.target_title + " — " +
          item.target_anchor_label.value_or("Documento inteiro"));
      prepare_tool_label(*label);
      row->append(*label);
      if (!item.reference.notes.empty()) {
        auto *note = Gtk::make_managed<Gtk::Label>(item.reference.notes);
        prepare_tool_label(*note);
        row->append(*note);
      }
      auto *actions =
          Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
      auto *open = Gtk::make_managed<Gtk::Button>(
          incoming ? "Abrir Documento que usa" : "Abrir fonte");
      open->signal_clicked().connect([this, dialog, item, incoming] {
        retire_text_reference_dialog(dialog);
        const auto id = incoming ? item.reference.source_document_id
                                 : item.reference.target_document_id;
        open_document(id);
        if (!incoming && item.reference.target_anchor_id && current_document_ &&
            current_document_->id == id) {
          select_anchor(*item.reference.target_anchor_id);
          open_selected_anchor();
        }
        if (tools_visible_)
          toggle_tools();
      });
      auto *remove = Gtk::make_managed<Gtk::Button>("Desvincular");
      remove->signal_clicked().connect(
          [this, dialog, item, incoming, anchor_id] {
            retire_text_reference_dialog(dialog);
            auto *confirm = new OverlayDialog("Desvincular referência textual",
                                              *owner_window(), true);
            confirm->set_secondary_text(
                item.source_title + " → " + item.target_title + " — " +
                item.target_anchor_label.value_or("Documento inteiro") +
                ". O conteúdo de ambos os Documentos será preservado.");
            confirm->add_button("Cancelar", Gtk::ResponseType::CANCEL);
            confirm->add_button("Desvincular", Gtk::ResponseType::ACCEPT);
            confirm->signal_response().connect(
                [this, confirm, item, incoming, anchor_id](int response) {
                  confirm->hide();
                  try {
                    if (response == Gtk::ResponseType::ACCEPT)
                      service_.writing().remove_text_reference(
                          item.reference.source_document_id, item.reference.id);
                    show_text_references(incoming, anchor_id);
                  } catch (const std::exception &error) {
                    show_error("Não foi possível desvincular", error);
                  }
                });
            confirm->present();
          });
      actions->append(*open);
      actions->append(*remove);
      row->append(*actions);
      row->set_margin_bottom(12);
      body->append(*row);
    }
    auto *pages = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    auto *previous = Gtk::make_managed<Gtk::Button>("Anterior");
    previous->set_sensitive(offset != 0);
    previous->signal_clicked().connect([this, dialog, incoming, anchor_id,
                                        offset] {
      retire_text_reference_dialog(dialog);
      show_text_references(incoming, anchor_id, offset >= 50 ? offset - 50 : 0);
    });
    auto *next = Gtk::make_managed<Gtk::Button>("Próxima");
    next->set_sensitive(offset + page.items.size() < page.total);
    next->signal_clicked().connect([this, dialog, incoming, anchor_id, offset] {
      retire_text_reference_dialog(dialog);
      show_text_references(incoming, anchor_id, offset + 50);
    });
    pages->append(*previous);
    pages->append(*next);
    body->append(*pages);
    dialog->signal_response().connect([dialog](int) { dialog->hide(); });
    dialog->present();
  } catch (const std::exception &error) {
    show_error("Não foi possível consultar referências textuais", error);
  }
}

void WritingWorkspace::add_text_reference() {
  if (!current_document_ || !owner_window())
    return;
  const auto source_id = current_document_->id;
  auto *dialog = new OverlayDialog("Referenciar Documento ou trecho marcado",
                                   *owner_window(), true);
  dialog->set_secondary_text(
      "Escolha uma fonte. A referência preserva a identidade e abre o texto atual, sem incorporá-lo.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Referenciar", Gtk::ResponseType::ACCEPT);
  auto *body = dialog->get_content_area();
  body->set_margin(16);
  auto *document = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar Documento de destino",
      [this, source_id](const std::string &search, std::size_t limit) {
        persistence::DocumentQuery query;
        query.search = search;
        query.limit = std::min(limit, std::size_t{50});
        std::vector<IncrementalSelection> result;
        for (const auto &item : service_.writing().document_summaries(query))
          if (item.id != source_id)
            result.push_back({item.id, item.title, "Documento do Projeto"});
        return result;
      });
  auto *anchor = Gtk::make_managed<Gtk::ComboBoxText>();
  auto *target_error = Gtk::make_managed<Gtk::Label>();
  prepare_tool_label(*target_error);
  target_error->set_visible(false);
  anchor->append("none", "Documento inteiro");
  anchor->set_active_id("none");
  document->signal_selection_changed().connect([this, document, anchor,
                                                target_error] {
    target_error->set_visible(false);
    anchor->remove_all();
    anchor->append("none", "Documento inteiro");
    anchor->set_active_id("none");
    try {
      if (document->selected_id()) {
        const auto target =
            service_.writing().document(*document->selected_id());
        if (!target)
          throw std::runtime_error("Documento de destino indisponível");
        for (const auto &item : target->anchors)
          anchor->append(item.id, item.label);
      }
    } catch (const std::exception &error) {
      target_error->set_text(
          std::string{"Não foi possível carregar os trechos marcados: "} + author::diagnostic(error.what()));
      target_error->set_visible(true);
    }
  });
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_max_length(4096);
  append_labeled_form_field(*body, "Documento de destino", *document);
  append_labeled_form_field(*body, "Destino no texto", *anchor);
  body->append(*target_error);
  append_labeled_form_field(*body, "Nota opcional", *notes);
  dialog->signal_response().connect(
      [this, dialog, document, anchor, notes, source_id](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            if (!document->selected_id())
              throw std::runtime_error("Escolha um Documento de destino");
            const auto id = anchor->get_active_id().raw();
            static_cast<void>(service_.writing().add_text_reference(
                source_id, *document->selected_id(),
                id.empty() || id == "none" ? std::nullopt
                                           : std::optional<std::string>{id},
                notes->get_text()));
          } catch (const std::exception &error) {
            dialog->hide();
            show_error("Não foi possível criar a referência textual", error);
            return;
          }
        }
        dialog->hide();
        show_text_references();
      });
  dialog->present();
}

} // namespace inde::ui
