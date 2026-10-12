#include "writing_semantic_relation_support.hpp"

namespace inde::ui {
using namespace semantic_ui;
void WritingWorkspace::edit_semantic_relations(const std::string &source, std::size_t offset) {
  if (!current_document_ || !owner_window()) return;
  try {
    auto d = capture_editor_state();
    annotation(d, source);
    const auto epoch = edit_epoch_, revision = edit_revision_;
    const auto docid = d.id;
    auto guard = [this, epoch, revision, docid] {
      if (!current_document_ || current_document_->id != docid ||
          edit_epoch_ != epoch || edit_revision_ != revision)
        throw std::runtime_error("Contexto mudou; reabra as relações");
    };
    persistence::SemanticRelationQuery q;
    q.annotation_id = source; q.exclude_source_document_id = d.id;
    q.offset = offset; q.limit = 12;
    auto incoming = service_.writing().semantic_relations(q);
    auto *dialog = new OverlayDialog("Relações explícitas da anotação", *owner_window(), true);
    dialog->add_button("Fechar", Gtk::ResponseType::CANCEL);
    auto *box = dialog->get_content_area();
    label(*box, "Saídas usam o estado atual do Documento, inclusive não salvo. "
                "Entradas externas mostram vínculos persistidos. Nenhum alvo é escolhido automaticamente.");
    button(*box, "Nova relação explícita…", [this, dialog, source, guard] {
      try { guard(); retire(dialog); pick_semantic_relation(source); }
      catch (const std::exception &e) { retire(dialog); show_error("Contexto mudou", e); }
    });
    std::vector<project::AnnotationRelation> local;
    for (const auto &r : d.semantics.relations)
      if (r.source_annotation_id == source || r.target_annotation_id == source)
        local.push_back(r);
    label(*box, "Relações no Documento: " + std::to_string(local.size()) +
                " • entradas externas: " + std::to_string(incoming.total));
    const auto navigate = [this, dialog, guard](const std::string &id, const std::string &document) {
      try {
        guard(); retire(dialog);
        reveal_document(document);
        if (current_document_ && current_document_->id == document) show_semantic_annotation(id);
      } catch (const std::exception &e) { retire(dialog); show_error("Não foi possível navegar", e); }
    };
    // Both lists share a bounded window. Local edits override persisted outgoing
    // rows, so undo/draft removals never reappear as stale persisted links.
    for (std::size_t i = offset; i < std::min(local.size(), offset + 12); ++i) {
      const auto r = local[i];
      const bool outgoing = r.source_annotation_id == source;
      const auto peer = outgoing ? r.target_annotation_id : r.source_annotation_id;
      std::string peer_doc = d.id, title = d.title;
      if (std::none_of(d.semantics.annotations.begin(), d.semantics.annotations.end(),
                       [&](const auto &a) { return a.id == peer; })) {
        persistence::SemanticUseQuery lookup; lookup.annotation_id = peer;
        lookup.include_archived = true; lookup.limit = 1;
        auto p = service_.writing().semantic_uses(lookup);
        if (!p.rows.empty()) { peer_doc = p.rows.front().document_id; title = p.rows.front().document_title; }
        else title = "Alvo indisponível — vínculo preservado para diagnóstico";
      }
      auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
      card->add_css_class("content-card");
      label(*card, std::string(outgoing ? "Saída • " : "Entrada local • ") + relation_label(r.type_key));
      label(*card, clipped(title, 100) + "\n" + peer);
      if (!r.notes.empty()) label(*card, r.notes);
      button(*card, "Abrir anotação relacionada", [navigate, peer, peer_doc] { navigate(peer, peer_doc); });
      button(*card, "Desvincular relação", [this, dialog, r, guard] {
        try {
          guard();
          auto next = application::TextSemanticService::unrelate(capture_editor_state(), r.id);
          retire(dialog); apply_semantic_document(std::move(next), "Desvincular anotações");
        } catch (const std::exception &e) { retire(dialog); show_error("Não foi possível desvincular", e); }
      });
      box->append(*card);
    }
    for (const auto &r : incoming.rows) {
      auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
      card->add_css_class("content-card");
      label(*card, "Entrada externa • " + relation_label(r.type_key));
      label(*card, clipped(r.source.document_title, 100) + " • " + status_label(r.source.status));
      label(*card, "Anotação: " + r.source.annotation_id);
      button(*card, "Abrir origem da relação", [navigate, r] { navigate(r.source.annotation_id, r.source.document_id); });
      label(*card, "Para desvincular, abra a origem e revise suas relações. "
                   "Este detalhe não altera outro Documento.");
      box->append(*card);
    }
    if (local.empty() && !incoming.total) label(*box, "Nenhuma relação explícita atribuída");
    label(*box, "Alvos usados não podem ser removidos. Arquivar conserva identidade e vínculos; para remover, desvincule primeiro em cada origem.");
    if (offset) button(*box, "Relações anteriores", [this, dialog, source, offset, guard] {
      try { guard(); retire(dialog); edit_semantic_relations(source, offset >= 12 ? offset - 12 : 0); }
      catch (const std::exception &e) { retire(dialog); show_error("Contexto mudou", e); }
    });
    if (offset + 12 < std::max(local.size(), incoming.total))
      button(*box, "Próximas relações", [this, dialog, source, offset, guard] {
        try { guard(); retire(dialog); edit_semantic_relations(source, offset + 12); }
        catch (const std::exception &e) { retire(dialog); show_error("Contexto mudou", e); }
      });
    dialog->signal_response().connect([dialog](int) { dialog->hide(); });
    dialog->present();
  } catch (const std::exception &e) { show_error("Não foi possível abrir relações semânticas", e); }
}
} // namespace inde::ui
