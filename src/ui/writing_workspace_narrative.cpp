#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "narrative_unit_picker.hpp"
#include <algorithm>
namespace inde::ui {
void WritingWorkspace::reveal_realization(
    const application::BuilderRealization &r) {
  flush_changes();
  if (!r.composition_id.empty()) {
    const auto c = service_.compositions().load(r.composition_id);
    (void)c.value.blocks.occurrence(r.occurrence_id);
    composition_page_.reveal_occurrence(r.composition_id, r.occurrence_id);
    page_stack_.set_visible_child("compositions");
    return;
  }
  const auto d = service_.writing().document(r.document_id);
  if (!d)
    throw std::runtime_error("Documento ausente");
  std::optional<project::DocumentAnchor> anchor;
  if (!r.anchor_id.empty()) {
    auto at = std::find_if(d->anchors.begin(), d->anchors.end(),
                           [&](auto &a) { return a.id == r.anchor_id; });
    if (at == d->anchors.end() || at->start_offset == at->end_offset)
      throw std::runtime_error(
          "Trecho removido ou vazio; confira sua associação ao planejamento");
    for (const auto &v : d->semantics.anchors)
      if (v.anchor_id == r.anchor_id &&
          v.state != project::AnchorResolution::Resolved)
        throw std::runtime_error(
            "Trecho marcado não localizado com segurança; confira-o antes de navegar");
    anchor = *at;
  }
  reveal_document(r.document_id);
  if (current_document_id() != r.document_id)
    throw std::runtime_error("Não foi possível abrir o Documento");
  if (anchor) {
    position_restore_.disconnect();
    const auto buffer = editor_.get_buffer();
    buffer->select_range(
        buffer->get_iter_at_offset(static_cast<int>(anchor->end_offset)),
        buffer->get_iter_at_offset(static_cast<int>(anchor->start_offset)));
    edit_session_.update_selection({anchor->end_offset, anchor->start_offset});
    editor_.scroll_to(buffer->get_insert());
    editor_.grab_focus();
    const auto id = r.document_id, aid = r.anchor_id;
    const auto revision = edit_revision_;
    position_restore_ = Glib::signal_timeout().connect(
        sigc::track_object(
            [this, id, aid, revision, attempts = 60, stable = 0,
             upper = -1.0]() mutable {
              if (current_document_id() != id || edit_revision_ != revision ||
                  --attempts <= 0 || ime_active_)
                return false;
              if (!editor_.get_mapped())
                return true;
              select_anchor(aid);
              editor_.grab_focus();
              open_selected_anchor();
              const auto adjustment = editor_scroll_.get_vadjustment();
              GdkRectangle target{}, visible{};
              auto position = editor_.get_buffer()->get_iter_at_mark(
                  editor_.get_buffer()->get_insert());
              gtk_text_view_get_iter_location(editor_.gobj(), position.gobj(),
                                              &target);
              gtk_text_view_get_visible_rect(editor_.gobj(), &visible);
              const bool shown = visible.height > 0 && target.y >= visible.y &&
                                 target.y < visible.y + visible.height;
              if (shown && upper == adjustment->get_upper())
                ++stable;
              else
                stable = 0;
              upper = adjustment->get_upper();
              return stable < 2;
            },
            *this),
        50);
  }
}
void WritingWorkspace::show_narrative_association() {
  try {
    const auto context = interaction_context();
    flush_changes();
    require_interaction_context(context);
    auto *owner = dynamic_cast<Gtk::Window *>(get_root());
    if (!owner)
      return;
    auto *dialog =
        new OverlayDialog("Associar seleção a parte da narrativa", *owner, true);
    dialog->set_secondary_text(
        "A seleção ganhará um trecho marcado estável. Nenhum texto será copiado e nenhuma ordem será alterada. A associação é declarada, não uma interpretação automática. Resultados: até 50; refine a pesquisa.");
    auto *picker = Gtk::make_managed<NarrativeUnitPicker>(service_);
    dialog->get_content_area()->append(*picker);
    auto *name = Gtk::make_managed<Gtk::Entry>();
    name->set_text("Trecho da narrativa");
    name->set_max_length(128);
    append_labeled_form_field(*dialog->get_content_area(), "Nome do trecho",
                              *name);
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    dialog->add_button("Associar trecho", Gtk::ResponseType::ACCEPT);
    dialog->signal_response().connect(
        [this, dialog, picker, name, context](int response) {
          if (response != Gtk::ResponseType::ACCEPT)
            return;
          try {
            require_interaction_context(context);
            flush_changes();
            require_interaction_context(context);
            service_.builder().attach_range(
                picker->snapshot(), picker->unit(), *saved_document_,
                context.range.start, context.range.end, name->get_text().raw());
            const auto persisted =
                service_.writing().document(current_document_->id);
            if (!persisted)
              throw std::runtime_error("Operação gravada, mas Documento "
                                       "indisponível; não repita, reabra-o");
            auto after = context.state;
            after.document = *persisted;
            auto history = edit_session_;
            history.commit(after, "Associar trecho à parte");
            save_worker_.clear();
            try {
              restore_edit_state(after);
            } catch (const std::exception &e) {
              saved_document_ = persisted;
              history.acknowledge_saved(*persisted);
              edit_session_ = std::move(history);
              throw std::runtime_error(
                  std::string(
                      "Associação gravada, mas a interface não conseguiu se "
                      "atualizar. Não repita; reabra o Documento. Detalhe: ") +
                  author::diagnostic(e.what()));
            }
            ++edit_revision_;
            history.acknowledge_saved(*persisted);
            edit_session_ = std::move(history);
            saved_document_ = persisted;
            saved_revision_ = edit_revision_;
            save_submitted_ = false;
            save_error_.clear();
            update_undo_actions();
            update_editor_status();
            schedule_recovery();
            signal_status_message_.emit(
                "Trecho associado. Desfazer textual retira o trecho marcado e deixa uma pendência visível; Histórico do Projeto reverte a operação completa.");
          } catch (const std::exception &e) {
            dialog->set_secondary_text(author::diagnostic(e.what()));
            dialog->keep_open_on_response();
          }
        });
    dialog->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível associar seleção", e);
  }
}
void WritingWorkspace::show_narrative_uses(bool composition) {
  try {
    flush_changes();
    const auto origin = capture_view();
    auto *owner = dynamic_cast<Gtk::Window *>(get_root());
    if (!owner)
      return;
    std::string document, composition_id, occurrence;
    if (composition) {
      const auto v = composition_page_.capture_view();
      composition_id = view_id(v.at("composition")).value_or("");
      occurrence = view_id(v.at("occurrence")).value_or("");
    } else
      document = current_document_id();
    const auto uses = std::make_shared<const std::vector<project::RemovalUse>>(
        service_.builder().text_uses(document, composition_id, occurrence));
    auto *dialog = new OverlayDialog("Planejamento deste texto associado",
                                     *owner, true);
    dialog->set_secondary_text(
        "Associações declaradas: " + std::to_string(uses->size()) +
        ". Não são elementos da história inferidas nem equivalência de significado. Até 50 resultados por pesquisa; abrir uma parte permitirá voltar à posição salva da escrita.");
    auto *picker = Gtk::make_managed<IncrementalSelector>(
        "Pesquisar parte ou plano",
        [uses](const std::string &query, std::size_t limit) {
          std::vector<IncrementalSelection> out;
          const auto key = project::alias_key(query);
          for (const auto &v : *uses)
            if (project::alias_key(v.name + " " + v.path).find(key) !=
                std::string::npos) {
              out.push_back({v.record_id, v.name, v.path});
              if (out.size() >= limit)
                break;
            }
          return out;
        });
    dialog->get_content_area()->append(*picker);
    dialog->add_button("Fechar", Gtk::ResponseType::CANCEL);
    dialog->add_button("Abrir parte no planejamento",
                       Gtk::ResponseType::ACCEPT);
    dialog->signal_response().connect([this, dialog, picker, uses,
                                       origin](int response) {
      if (response != Gtk::ResponseType::ACCEPT)
        return;
      try {
        if (!picker->selected_id())
          throw std::runtime_error(
              "Escolha um texto associado; se não houver, associe uma "
              "seleção de texto pelo botão direito");
        const auto at = std::find_if(uses->begin(), uses->end(), [&](auto &v) {
          return v.record_id == *picker->selected_id();
        });
        if (at == uses->end())
          throw std::runtime_error("Uso não encontrado");
        signal_narrative_source_requested_.emit(*at, origin);
      } catch (const std::exception &e) {
        dialog->set_secondary_text(author::diagnostic(e.what()));
        dialog->keep_open_on_response();
      }
    });
    dialog->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível consultar o planejamento do texto", e);
  }
}
} // namespace inde::ui
