#include "inde/experimental/blocks_lab_window.hpp"
#include <algorithm>
#include <stdexcept>
namespace inde::experimental {
std::size_t BlocksLabWindow::dirty_count() const {
  return std::count_if(editors_.begin(), editors_.end(), [](auto &e) { return e->dirty; });
}
void BlocksLabWindow::open_editor(const std::string &block, const std::string &version) {
  const auto &b = state_.lab.block(block); const auto &r = state_.lab.version(block, version);
  for (std::size_t i = 0; i < editors_.size(); ++i) if (editors_[i]->block == block && editors_[i]->expected == version) {
    show_editor(i); preview_tabs_.set_current_page(0);
    editors_[i]->text.grab_focus(); sidebar_.set_current_page(1); return;
  }
  if (editors_.size() >= 16) throw std::runtime_error("Há 16 edições abertas; feche um painel para abrir outro, sem perder a montagem");
  auto editor = std::make_unique<Editor>(); auto *e = editor.get();
  e->block = block; e->expected = version; e->writable = version == b.current_revision;
  e->frame.set_child(e->box); e->caption.set_wrap(true); e->caption.set_xalign(0); e->box.set_margin(8);
  e->box.append(e->caption); e->text.set_wrap_mode(Gtk::WrapMode::WORD_CHAR); e->text.add_css_class("writing-editor");
  e->text.set_left_margin(10); e->text.set_top_margin(10); e->text.set_editable(e->writable);
  e->text.set_tooltip_text("Edição do bloco: " + b.label + " · " + version_label(block, version));
  e->text.get_buffer()->set_text(r.text); e->scroll.set_child(e->text); e->scroll.set_min_content_height(150);
  e->scroll.set_vexpand(true); e->box.append(e->scroll); e->buttons.append(e->apply); e->buttons.append(e->discard); e->buttons.append(e->close); e->box.append(e->buttons);
  e->frame.set_hexpand(true); e->frame.set_vexpand(true); e->frame.set_size_request(320, 245);
  active_editor_ = editors_.size(); editor_grid_.attach(e->frame, 0, 0);
  e->text.get_buffer()->signal_changed().connect([this, e] {
    if (rendering_) return;
    e->dirty = true; invalidate_output();
    auto block = state_.lab.blocks().find(e->block);
    status_.set_text("Rascunho de “" + (block == state_.lab.blocks().end() ? std::string("bloco desfeito") : block->second.label) + "” ainda não aplicado. A leitura e os outros usos só mudam depois de Aplicar versão.");
    render_editors();
  });
  e->apply.signal_clicked().connect([this, e] { attempt([this, e] { apply_editor(*e); }); });
  e->discard.signal_clicked().connect([this, e] { attempt([this, e] {
    ask("Descartar apenas este rascunho e voltar à versão de base? A montagem aplicada permanece intacta.", [this, e] {
      const auto &r = state_.lab.version(e->block, e->expected); rendering_ = true; e->text.get_buffer()->set_text(r.text); rendering_ = false;
      e->dirty = false; render_editors(); status_.set_text("Rascunho descartado por confirmação explícita");
    });
  }); });
  e->close.signal_clicked().connect([this, e] { attempt([this, e] {
    ask(e->dirty ? "Fechar este painel e descartar seu rascunho não aplicado? Bloco, versões em uso na montagem e montagem continuam intactos." :
        "Fechar somente este painel? Bloco, versões e montagem continuam intactos; você pode reabri-lo depois.", [this, e] {
      for (auto &panel : editors_) editor_grid_.remove(panel->frame);
      std::erase_if(editors_, [&](auto &panel) { return panel.get() == e; });
      for (auto &panel : editors_) editor_grid_.attach(panel->frame, 0, 0);
      if (active_editor_ >= editors_.size()) active_editor_ = editors_.empty() ? 0 : editors_.size() - 1;
      invalidate_output(); render_editors(); status_.set_text("Painel fechado por confirmação; montagem preservada");
    });
  }); });
  editors_.push_back(std::move(editor)); render_editors(); preview_tabs_.set_current_page(0); sidebar_.set_current_page(1); e->text.grab_focus();
}
void BlocksLabWindow::apply_editor(Editor &e) {
  const auto &b = state_.lab.block(e.block);
  if (e.expected != b.current_revision) throw std::runtime_error("Esta versão não é mais atual; rascunho preservado. Abra a versão atual para comparar, sem sobrescrever");
  const auto text = e.text.get_buffer()->get_text().raw();
  if (text == state_.lab.version(e.block, e.expected).text) { e.dirty = false; render_editors(); return; }
  const auto impact = state_.lab.impact(e.block);
  std::size_t hidden{}; for (const auto &id : impact.changed_live) if (!state_.lab.visible(state_.lab.occurrence(id), state_.profile)) ++hidden;
  auto summary = "Aplicar nova versão de “" + b.label + "”? " + std::to_string(impact.changed_live.size()) +
    " uso(s) do bloco acompanham a alteração, sendo " + std::to_string(hidden) + " ocultos; " +
    std::to_string(impact.preserved_pinned.size()) + " versão(ões) fixas ficam preservadas. Consulte Usos/impacto para os nomes. Origens não são alteradas.";
  std::size_t shown{}, index{};
  for (auto &use : state_.lab.placements()) {
    ++index;
    if (use.block_id != e.block || use.mode == PlacementMode::ReferenceOnly) continue;
    if (++shown <= 6) summary += "\nUso do bloco " + std::to_string(index) + " · " + version_label(e.block, state_.lab.revision(use).id) +
        (use.mode == PlacementMode::Pinned ? " · PRESERVADA" : " · SERÁ ALTERADA") +
        (state_.lab.visible(use, state_.profile) ? " · na leitura" : " · oculta");
  }
  if (shown > 6) summary += "\nMais " + std::to_string(shown - 6) + " uso do bloco(s); lista integral em Usos/impacto.";
  const auto block = e.block, expected = e.expected;
  ask(summary, [this, &e, block, expected, text] {
    if (command("Nova versão em uso na montagem aos usos vinculados", [&](State &s) {
      s.lab.revise(block, expected, text);
    })) {
      e.expected = state_.lab.block(block).current_revision; e.dirty = false;
    }
    render_editors();
  });
}
void BlocksLabWindow::render_editors() {
  for (auto &e : editors_) {
    const auto found = state_.lab.blocks().find(e->block);
    if (found == state_.lab.blocks().end()) {
      e->caption.set_text("Bloco retirado pelo desfazer · rascunho preservado neste painel"); e->apply.set_sensitive(false); e->text.set_editable(false); continue;
    }
    const auto &b = found->second;
    const bool exists = std::any_of(b.revisions.begin(), b.revisions.end(), [&](auto &r) { return r.id == e->expected; });
    const bool current = exists && e->expected == b.current_revision;
    e->writable = current; e->text.set_editable(current);
    e->apply.set_sensitive(current && e->dirty); e->discard.set_sensitive(exists && e->dirty);
    auto caption = b.label + " · " + (exists ? version_label(b.id, e->expected) : "base desfeita") + "\n" + origin_title(b.origin_document_id);
    caption += current ? (e->dirty ? " · RASCUNHO NÃO APLICADO" : " · versão atual") : " · versão histórica / somente leitura";
    if (!current && e->dirty) caption += " · rascunho preservado, aplicação bloqueada";
    e->caption.set_text(caption);
    e->text.set_tooltip_text("Edição do bloco: " + caption);
  }
  selecting_editor_ = true; editor_selector_.remove_all();
  for (std::size_t i = 0; i < editors_.size(); ++i) {
    const auto found = state_.lab.blocks().find(editors_[i]->block);
    editor_selector_.append(std::to_string(i), (found == state_.lab.blocks().end() ? "Bloco desfeito" : found->second.label) +
        (editors_[i]->dirty ? " · rascunho" : " · versão em uso na montagem"));
  }
  if (!editors_.empty()) editor_selector_.set_active(static_cast<int>(active_editor_));
  editor_selector_.set_visible(!editors_.empty()); selecting_editor_ = false;
  show_editor(active_editor_);
}
void BlocksLabWindow::show_editor(std::size_t index) {
  active_editor_ = index;
  for (std::size_t i = 0; i < editors_.size(); ++i) editors_[i]->frame.set_visible(i == index);
  selecting_editor_ = true;
  if (index < editors_.size()) editor_selector_.set_active(static_cast<int>(index));
  selecting_editor_ = false;
}
void BlocksLabWindow::invalidate_output() {
  prepared_.reset(); publish_.set_sensitive(false); acknowledge_.set_active(false);
  warnings_text_.get_buffer()->set_text("Prévia não preparada ou desatualizada. Aplique os rascunhos e use Preparar novo Documento.");
}
void BlocksLabWindow::prepare_output() {
  if (dirty_count()) throw std::runtime_error("Há " + std::to_string(dirty_count()) + " rascunho(s) pendente(s): aplique ou descarte explicitamente antes de criar o Documento");
  const auto projection = state_.lab.project(state_.profile);
  if (projection.text.empty()) throw std::runtime_error("A leitura está vazia; inclua um uso do bloco de texto visível");
  auto output = materialize(state_.lab, projection, state_.title);
  std::set<std::string> checked;
  for (auto &segment : projection.segments) {
    const auto doc = state_.lab.block(segment.block_id).origin_document_id;
    if (checked.insert(doc).second && source_references_.contains(doc) && source_references_.at(doc))
      output.warnings.push_back(origin_title(doc) + ": referências entre Documentos continuam na origem; não são copiadas como novos vínculos permanentes.");
  }
  output_text_.get_buffer()->set_text(output.document.content);
  std::string warnings = "Novo Documento independente: “" + output.document.title + "”\n" +
    std::to_string(output.incorporated_occurrences) + " uso do bloco(s) de texto; " +
    std::to_string(output.document.formatting.size()) + " formatos; " + std::to_string(output.document.anchors.size()) +
    " trechos marcados; " + std::to_string(output.document.semantics.annotations.size()) + " marcações.\n";
  warnings += "Grava apenas na cópia preparada. Não salva vínculos vivos, organização editorial ou a sessão de blocos.\n";
  if (output.warnings.empty()) warnings += "Nenhum aviso de conservação para os recortes atuais.\n";
  for (auto &warning : output.warnings) warnings += "• " + warning + "\n";
  prepared_ = std::move(output); warnings_text_.get_buffer()->set_text(warnings); acknowledge_.set_active(false);
  publish_.set_sensitive(false); preview_tabs_.set_current_page(3); status_.set_text("Prévia preparada. Leia os avisos e confirme antes de criar o Documento.");
}
}
