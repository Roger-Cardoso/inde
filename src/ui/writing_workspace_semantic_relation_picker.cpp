#include "writing_semantic_relation_support.hpp"
#include "inde/project/document_state.hpp"

namespace inde::ui {
using namespace semantic_ui;
void WritingWorkspace::pick_semantic_relation(const std::string &source,
    bool project_scope, std::size_t offset, const std::string &role) {
  if (!current_document_ || !owner_window()) return;
  try {
    auto d = capture_editor_state(); annotation(d, source);
    const auto types = service_.writing().semantic_types();
    const auto epoch = edit_epoch_, revision = edit_revision_;
    auto guard = [this, epoch, revision, docid=d.id] {
      if (!current_document_ || current_document_->id != docid ||
          edit_epoch_ != epoch || edit_revision_ != revision)
        throw std::runtime_error("Contexto mudou; reabra a escolha de alvo");
    };
    std::vector<persistence::SemanticUse> rows;
    std::size_t total{};
    constexpr std::size_t limit = 12;
    if (project_scope) {
      persistence::SemanticUseQuery q; q.exclude_document_id = d.id;
      if (!role.empty()) q.type_key = role;
      q.include_archived = true; q.offset = offset; q.limit = limit;
      auto p = service_.writing().semantic_uses(q); rows = std::move(p.rows); total = p.total;
    } else {
      application::SemanticAnchorEngine::inspect(d);
      for (const auto &a : d.semantics.annotations) {
        if (a.id == source || (!role.empty() && a.type_key != role)) continue;
        if (total++ < offset || rows.size() == limit) continue;
        rows.push_back({a.id, d.id, d.title, a.anchor_id, a.type_key, a.status, {}, 0, 0, a.scope});
      }
    }
    auto *dialog = new OverlayDialog("Escolher alvo da relação semântica", *owner_window(), true);
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    auto *box = dialog->get_content_area();
    label(*box, project_scope ? "Outros Documentos do Projeto — somente marcações persistidas; "
                               "a escolha não força salvamento."
                             : "Documento atual — inclui marcações ainda não salvas.");
    auto *scope = Gtk::make_managed<Gtk::ComboBoxText>();
    scope->append("document", "Documento atual"); scope->append("project", "Outros Documentos do Projeto");
    scope->set_active_id(project_scope ? "project" : "document"); box->append(*scope);
    auto *filter = Gtk::make_managed<Gtk::ComboBoxText>();
    filter->append("", "Todos os tipos de marcação");
    std::set<std::string> keys;
    for (const auto &t : types) if (keys.insert(t.key).second) filter->append(t.key, author::annotation_label(t));
    filter->set_active_id(role); box->append(*filter);
    // Scope/filter rebuilds must outlive the GTK notification that initiated
    // them, and must not resurrect a dialog canceled before the idle callback.
    auto alive = std::make_shared<bool>(true);
    auto rebuild = [this, dialog, source, scope, filter, alive, guard] {
      std::weak_ptr<int> life = clipboard_lifetime_;
      Glib::signal_idle().connect_once([this, dialog, source, scope, filter, alive, guard, life] {
        if (!*alive || life.expired()) return;
        // Coalesce rapid changes: read the latest pair after GTK notifications,
        // not the first queued pair, which would discard a subsequent filter.
        const bool project = scope->get_active_id() == "project";
        const auto role = filter->get_active_id().raw();
        *alive = false;
        try { guard(); retire(dialog); pick_semantic_relation(source, project, 0, role); }
        catch (const std::exception &e) { retire(dialog); show_error("Contexto mudou", e); }
      });
    };
    scope->signal_changed().connect(rebuild); filter->signal_changed().connect(rebuild);
    label(*box, std::to_string(total) + " alvo(s) • página " + std::to_string(offset / limit + 1));
    for (const auto &r : rows) {
      std::string name = r.type_key;
      for (const auto &t : types) if (t.key == r.type_key) { name = author::annotation_label(t); break; }
      auto *b = Gtk::make_managed<Gtk::Button>(clipped(r.document_title, 60) + " • " + name +
                                             " • " + status_label(r.status));
      b->set_tooltip_text("Revisar alvo: " + r.annotation_id);
      b->signal_clicked().connect([this, dialog, source, r, alive, guard] {
        try {
          guard(); *alive = false; retire(dialog);
          preview_semantic_relation(source, r.annotation_id, r.document_id);
        } catch (const std::exception &e) { *alive=false; retire(dialog); show_error("Alvo indisponível", e); }
      });
      box->append(*b);
    }
    if (rows.empty()) label(*box, "Nenhum alvo neste recorte. Ajuste tipo de marcação ou área da consulta; nada foi atribuído ou removido.");
    auto page = [this, dialog, source, project_scope, role, alive, guard](std::size_t at) {
      try { guard(); *alive=false; retire(dialog); pick_semantic_relation(source, project_scope, at, role); }
      catch (const std::exception &e) { *alive=false; retire(dialog); show_error("Contexto mudou", e); }
    };
    if (offset) button(*box, "Alvos anteriores", [page, offset] { page(offset >= limit ? offset - limit : 0); });
    if (offset + rows.size() < total) button(*box, "Próximos alvos", [page, offset] { page(offset + limit); });
    dialog->signal_response().connect([dialog, alive](int) { *alive=false; dialog->hide(); });
    dialog->present();
  } catch (const std::exception &e) { show_error("Não foi possível escolher alvo", e); }
}

void WritingWorkspace::preview_semantic_relation(const std::string &source,
    const std::string &target, const std::string &target_document) {
  if (!current_document_ || !owner_window()) return;
  try {
    const auto original = capture_editor_state();
    const bool external = target_document != original.id;
    auto snapshot = external ? service_.writing().document(target_document)
                             : std::optional<project::Document>(original);
    if (!snapshot) throw std::runtime_error("Documento alvo indisponível");
    auto from = original, to = *snapshot;
    application::SemanticAnchorEngine::inspect(from); application::SemanticAnchorEngine::inspect(to);
    const auto a = annotation(from, source), b = annotation(to, target);
    const auto types = service_.writing().semantic_types();
    const auto epoch = edit_epoch_, revision = edit_revision_;
    auto *dialog = new OverlayDialog("Revisar relação semântica", *owner_window(), true);
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    auto *box = dialog->get_content_area();
    endpoint_label(*box, "Origem", from, a, types);
    endpoint_label(*box, "Alvo", to, b, types);
    label(*box, "Escolha autoral explícita; sentido e direção não são inferidos. Trechos marcados pendentes ou tipos indisponíveis permanecem visíveis para revisão.");
    if (a.type_key != "inde.semantic.dialogue" || b.type_key != "inde.semantic.dialogue")
      label(*box, "Revisão opcional: ‘Resposta a’ fora de um par de diálogos exige "
                  "interpretação do autor; não é classificada automaticamente como erro.");
    auto *kind = Gtk::make_managed<Gtk::ComboBoxText>();
    for (const auto &key : {"inde.semantic.reference", "inde.semantic.response-to", "inde.semantic.callback"})
      kind->append(key, relation_label(key));
    kind->set_active(0); box->append(*kind);
    auto *note = Gtk::make_managed<Gtk::Entry>();
    note->set_placeholder_text("Nota autoral da relação (opcional)"); box->append(*note);
    button(*box, "Confirmar relação explícita", [this, dialog, original, snapshot, source, target,
                                                external, epoch, revision, kind, note] {
      try {
        if (!current_document_ || current_document_->id != original.id ||
            edit_epoch_ != epoch || edit_revision_ != revision)
          throw std::runtime_error("Origem mudou; reabra a prévia. Nenhuma relação aplicada.");
        if (external) {
          auto now = service_.writing().document(snapshot->id);
          if (!now || !project::same_document_state(*now, *snapshot))
            throw std::runtime_error("Alvo mudou; reabra a prévia. Nenhuma relação aplicada.");
        }
        auto next = application::TextSemanticService::relate(capture_editor_state(), source, target,
                                                             kind->get_active_id().raw());
        next.semantics.relations.back().notes = note->get_text().raw();
        // Validate before dismissing the preview; commit remains one undoable
        // editor transaction and only modifies the source Document.
        service_.writing().validate_semantic_document(next);
        retire(dialog); apply_semantic_document(std::move(next), "Relacionar anotações");
      } catch (const std::exception &e) { retire(dialog); show_error("Relação não aplicada", e); }
    });
    dialog->signal_response().connect([dialog](int) { dialog->hide(); }); dialog->present();
  } catch (const std::exception &e) { show_error("Não foi possível revisar relação", e); }
}
} // namespace inde::ui
