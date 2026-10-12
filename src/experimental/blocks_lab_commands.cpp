#include "inde/experimental/blocks_lab_window.hpp"
#include <algorithm>
#include <stdexcept>
namespace inde::experimental {
void BlocksLabWindow::attempt(const std::function<void()> &action) {
  try { action(); } catch (const std::exception &e) { status_.set_text(std::string("Não aplicado: ") + e.what()); }
}
void BlocksLabWindow::ask(const std::string &text, std::function<void()> action) {
  if (pending_) throw std::runtime_error("Confirme ou cancele a operação pendente");
  pending_ = std::move(action); confirmation_text_.set_text(text); confirmation_.set_visible(true);
  horizontal_.set_sensitive(false); top_.set_sensitive(false); cancel_.grab_focus();
}
bool BlocksLabWindow::command(const std::string &label, const std::function<void(State &)> &action) {
  bool applied{};
  attempt([&] {
    const auto old_selected = selected_, old_library = library_selected_;
    auto next = state_;
    try { action(next); (void)materialize(next.lab, next.lab.project(next.profile), next.title.empty() ? "Montagem sem título" : next.title); }
    catch (...) { selected_ = old_selected; library_selected_ = old_library; throw; }
    undo_.push_back(state_); if (undo_.size() > 16) undo_.erase(undo_.begin()); redo_.clear();
    state_ = std::move(next); changed_ = true; render(); status_.set_text(label + " · origens intactas; montagem em memória");
    applied = true;
  });
  return applied;
}
void BlocksLabWindow::wire_commands() {
  confirm_.signal_clicked().connect([this] {
    auto action = std::move(pending_); pending_ = {}; confirmation_.set_visible(false);
    horizontal_.set_sensitive(true); top_.set_sensitive(true); if (action) attempt(action);
  });
  cancel_.signal_clicked().connect([this] {
    pending_ = {}; confirmation_.set_visible(false); horizontal_.set_sensitive(true); top_.set_sensitive(true);
    status_.set_text("Operação cancelada; rascunhos e montagem preservados");
  });
  signal_close_request().connect([this] {
    if (!changed_ && !dirty_count()) return false;
    attempt([this] { ask("Fechar descarta os blocos, versões e rascunhos desta sessão. Documentos já criados na cópia continuam salvos. Descartar e fechar?", [this] { hide(); }); });
    return true;
  }, false);
  documents_.signal_changed().connect([this] { if (!rendering_) attempt([this] { load_source(); }); });
  previous_.signal_clicked().connect([this] { if (source_page_ >= 50) { source_page_ -= 50; source_page(); } });
  next_.signal_clicked().connect([this] { source_page_ += 50; source_page(); });
  auto import = [this](bool all) {
    attempt([&, all] {
      Gtk::TextBuffer::iterator a, b; std::size_t start{}, end = project::utf8_character_count(source_document_.content);
      if (!all) {
        if (!source_text_.get_buffer()->get_selection_bounds(a, b)) throw std::runtime_error("Selecione o trecho na origem");
        start = a.get_offset(); end = b.get_offset();
      }
      const auto label = name_.get_text().raw();
      if (label.empty()) throw std::runtime_error("Dê um nome ao bloco no campo acima das abas");
      command("Bloco extraído e selecionado", [&](State &s) {
        auto block = s.lab.import_document(source_document_, start, end); s.lab.rename_block(block, label);
        selected_ = s.lab.place(block); library_selected_ = block;
      });
      if (!selected_.empty()) select_occurrence(selected_);
    });
  };
  whole_.signal_clicked().connect([import] { import(true); }); extract_.signal_clicked().connect([import] { import(false); });
  library_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    if (rendering_ || !row) return;
    attempt([&] {
      const auto block = library_ids_.at(row->get_index()); library_selected_ = block;
      auto use = std::find_if(state_.lab.placements().begin(), state_.lab.placements().end(),
          [&](auto &p) { return p.block_id == block && p.id == selected_; });
      if (use == state_.lab.placements().end()) use = std::find_if(state_.lab.placements().begin(), state_.lab.placements().end(),
          [&](auto &p) { return p.block_id == block; });
      if (use != state_.lab.placements().end()) select_occurrence(use->id, false);
      else { selected_.clear(); render(); }
    });
  });
  occurrences_.signal_row_selected().connect([this](Gtk::ListBoxRow *row) {
    if (!rendering_ && row) attempt([&] { select_occurrence(occurrence_ids_.at(row->get_index())); });
  });
  open_.signal_clicked().connect([this] { attempt([this] { open_editor(library_selected_, versions_.get_active_id().raw()); }); });
  reuse_.signal_clicked().connect([this] {
    command("Uso do bloco do bloco acrescentada", [this](State &s) {
      const auto &b = s.lab.block(library_selected_); const auto rev = versions_.get_active_id().raw();
      selected_ = rev == b.current_revision ? s.lab.place(b.id) : s.lab.place(b.id, PlacementMode::Pinned, rev);
    });
    if (!selected_.empty()) attempt([this] { select_occurrence(selected_); });
  });
  rename_.signal_clicked().connect([this] { command("Bloco nomeado", [this](State &s) { s.lab.rename_block(library_selected_, name_.get_text().raw()); }); });
  auto split = [this](bool cursor) {
    attempt([&, cursor] {
      const auto p = state_.lab.occurrence(selected_); const auto &r = state_.lab.revision(p);
      Editor *editor{}; for (auto &e : editors_) if (e->block == p.block_id && e->expected == r.id) editor = e.get();
      if (!editor) throw std::runtime_error("Abra a versão deste uso do bloco e escolha a faixa ou cursor");
      if (editor->dirty) throw std::runtime_error("Aplique ou descarte o rascunho antes de dividir esta versão");
      Gtk::TextBuffer::iterator a, b; auto buffer = editor->text.get_buffer();
      if (cursor) a = b = buffer->get_iter_at_mark(buffer->get_insert());
      else if (!buffer->get_selection_bounds(a, b)) throw std::runtime_error("Selecione a faixa que deve virar bloco");
      const auto start = static_cast<std::size_t>(a.get_offset()), end = static_cast<std::size_t>(b.get_offset());
      const auto label = name_.get_text().raw();
      ask("Separar somente este uso do bloco nos limites escolhidos. Outros usos dos blocos continuam no bloco anterior; camadas que atravessam o corte serão avisadas. Confirmar?", [=, this] {
        command("Divisão autoral aplicada; montagem anterior disponível no desfazer", [=, this](State &s) {
          selected_ = s.lab.split_selection(p.id, start, end, label); library_selected_ = s.lab.occurrence(selected_).block_id;
        }); select_occurrence(selected_);
      });
    });
  };
  split_.signal_clicked().connect([split] { split(false); }); cursor_split_.signal_clicked().connect([split] { split(true); });
  repeat_.signal_clicked().connect([this] {
    command("Uso do bloco vinculada acrescentada e selecionada", [this](State &s) { selected_ = s.lab.place(s.lab.occurrence(selected_).block_id); });
    if (!selected_.empty()) attempt([this] { select_occurrence(selected_); });
  });
  pin_.signal_clicked().connect([this] {
    command("Uso do bloco preservada na versão escolhida", [this](State &s) {
      const auto p = s.lab.occurrence(selected_); if (p.block_id != library_selected_) throw std::runtime_error("Escolha uma versão do bloco deste uso do bloco");
      s.lab.use_version(p.id, versions_.get_active_id().raw(), s.lab.revision(p).id);
    });
  });
  follow_.signal_clicked().connect([this] { command("Uso do bloco acompanha a versão atual", [this](State &s) { s.lab.follow_current(selected_); }); });
  fork_.signal_clicked().connect([this] {
    command("Uso do bloco agora usa um bloco independente", [this](State &s) {
      const auto p = s.lab.occurrence(selected_); s.lab.fork(p.id, s.lab.revision(p).id);
      library_selected_ = s.lab.occurrence(p.id).block_id; s.lab.rename_block(library_selected_, name_.get_text().raw());
    }); if (!selected_.empty()) attempt([this] { select_occurrence(selected_); });
  });
  ref_.signal_clicked().connect([this] { attempt([this] {
    const auto id = selected_; (void)state_.lab.occurrence(id);
    ask("Converter este uso do bloco em referência: seu texto sai da leitura, mas o bloco, versões e vínculo permanecem na montagem. Confirmar?", [=, this] {
      command("Referência sem texto criada", [=](State &s) { s.lab.reference_only(id); });
    });
  }); });
  origin_.signal_clicked().connect([this] { attempt([this] {
    const auto &b = state_.lab.block(state_.lab.occurrence(selected_).block_id);
    if (!sources_.contains(b.origin_document_id)) throw std::runtime_error("Origem não está carregada nesta sessão");
    source_document_ = sources_.at(b.origin_document_id); source_text_.get_buffer()->set_text(source_document_.content);
    rendering_ = true; if (!documents_.set_active_id(source_document_.id)) { documents_.append(source_document_.id, source_document_.title); documents_.set_active_id(source_document_.id); } rendering_ = false;
    sidebar_.set_current_page(0); status_.set_text("Origem congelada em leitura; nenhuma edição do bloco foi gravada nela");
  }); });
  auto move = [this](int direction) { command("Ordem da montagem alterada", [=, this](State &s) {
    std::vector<std::string> ids; for (auto &p : s.lab.placements()) ids.push_back(p.id);
    auto found = std::find(ids.begin(), ids.end(), selected_);
    if (found == ids.end()) throw std::runtime_error("Selecione um uso do bloco");
    if ((direction < 0 && found != ids.begin()) || (direction > 0 && found + 1 != ids.end())) std::iter_swap(found, found + direction);
    s.lab.reorder(ids);
  }); };
  up_.signal_clicked().connect([move] { move(-1); }); down_.signal_clicked().connect([move] { move(1); });
  hide_.signal_clicked().connect([this] { command("Visibilidade explícita alterada; bloco não apagado", [this](State &s) { s.lab.set_hidden(selected_, !s.lab.occurrence(selected_).hidden); }); });
  remove_.signal_clicked().connect([this] { attempt([this] {
    const auto id = selected_; (void)state_.lab.occurrence(id);
    ask("Retirar apenas este uso do bloco da montagem? O bloco e suas versões ficam na Biblioteca; outros usos dos blocos não serão retirados.", [=, this] {
      command("Uso do bloco retirada; bloco e versões preservados", [=, this](State &s) { s.lab.remove_placement(id); selected_.clear(); });
    });
  }); });
  alternative_.signal_clicked().connect([this] {
    command("Alternativa criada e aberta para edição", [this](State &s) {
      selected_ = s.lab.alternative(selected_, name_.get_text().raw()); const auto &p = s.lab.occurrence(selected_);
      s.profile.selected_variants[*p.variant_group] = *p.variant; library_selected_ = p.block_id;
    }); if (!selected_.empty()) attempt([this] {
      select_occurrence(selected_); const auto &p = state_.lab.occurrence(selected_);
      if (p.variant) { rendering_ = true; alternatives_.set_active_id(*p.variant); rendering_ = false; }
    });
  });
  promote_.signal_clicked().connect([this] { attempt([this] {
    const auto id = selected_, label = name_.get_text().raw(); const auto p = state_.lab.occurrence(id);
    if (!p.variant) throw std::runtime_error("Selecione uma alternativa para promover");
    ask("Transformar em bloco independente. Este uso do bloco vira parte comum da montagem; Base e outras opções desta família ficam ocultas, não apagadas. Confirmar?", [=, this] {
      command("Alternativa promovida; Base preservada e identificada como oculta", [=, this](State &s) {
        library_selected_ = s.lab.promote(id, label); if (p.variant_group) s.profile.selected_variants.erase(*p.variant_group);
      }); select_occurrence(id);
    });
  }); });
  rename_variant_.signal_clicked().connect([this] { command("Alternativa nomeada", [this](State &s) {
    const auto p = s.lab.occurrence(selected_); if (!p.variant) throw std::runtime_error("Selecione uma alternativa"); s.lab.rename_variant(*p.variant, name_.get_text().raw());
  }); });
  alternatives_.signal_changed().connect([this] { if (!rendering_) command("Opção de leitura alterada; edição não foi trocada", [this](State &s) {
    const auto id = alternatives_.get_active_id().raw();
    if (id == "base") s.profile.selected_variants.clear();
    else s.profile.selected_variants[s.lab.variant_groups().at(id)] = id;
  }); });
  new_layer_.signal_clicked().connect([this] { command("Camada criada e visível", [this](State &s) {
    const auto id = s.lab.add_layer(name_.get_text().raw()); s.profile.visible_layers.insert(id);
  }); });
  rename_layer_.signal_clicked().connect([this] { command("Camada nomeada", [this](State &s) { s.lab.rename_layer(layers_.get_active_id().raw(), name_.get_text().raw()); }); });
  assign_layer_.signal_clicked().connect([this] { command("Associação à camada alterada", [this](State &s) {
    const auto p = s.lab.occurrence(selected_); const auto id = layers_.get_active_id().raw();
    if (id.empty()) throw std::runtime_error("Crie e escolha uma camada");
    s.lab.classify(p.id, p.variant, p.layer == id ? std::nullopt : std::optional<std::string>{id});
  }); });
  layer_visibility_.signal_clicked().connect([this] { command("Leitura da camada alterada; trechos preservados", [this](State &s) {
    const auto id = layers_.get_active_id().raw(); if (id.empty()) throw std::runtime_error("Escolha uma camada");
    if (!s.profile.visible_layers.erase(id)) s.profile.visible_layers.insert(id);
  }); });
  undo_button_.signal_clicked().connect([this] { if (!undo_.empty()) {
    redo_.push_back(state_); state_ = undo_.back(); undo_.pop_back(); rendering_ = true; title_.set_text(state_.title); rendering_ = false; changed_ = true; render(); status_.set_text("Montagem desfeita; rascunhos abertos não foram descartados");
  } });
  redo_button_.signal_clicked().connect([this] { if (!redo_.empty()) {
    undo_.push_back(state_); state_ = redo_.back(); redo_.pop_back(); render(); status_.set_text("Montagem refeita");
  } });
  reset_.signal_clicked().connect([this] { attempt([this] {
    ask("Nova montagem descarta esta sessão em memória e todos os rascunhos abertos. Documentos já criados na cópia continuam salvos. Confirmar?", [this] {
      for (auto &e : editors_) editor_grid_.remove(e->frame);
      editors_.clear(); state_ = {}; selected_.clear(); library_selected_.clear(); undo_.clear(); redo_.clear(); changed_ = false;
      rendering_ = true; title_.set_text(state_.title); rendering_ = false; render();
    });
  }); });
  title_.signal_changed().connect([this] { if (!rendering_) { state_.title = title_.get_text().raw(); changed_ = true; invalidate_output(); } });
  prepare_.signal_clicked().connect([this] { attempt([this] { prepare_output(); }); });
  acknowledge_.signal_toggled().connect([this] { publish_.set_sensitive(prepared_.has_value() && acknowledge_.get_active() && !dirty_count()); });
  publish_.signal_clicked().connect([this] { attempt([this] {
    if (!prepared_ || !acknowledge_.get_active() || dirty_count()) throw std::runtime_error("Prepare a versão atual e confirme os avisos; aplique os rascunhos antes");
    const auto prepared = *prepared_;
    ask("Criar um Documento novo e independente apenas na cópia preparada? Não sobrescreve origens e não salva vínculos vivos da montagem.", [=, this] {
      auto created = source_.create_snapshot(prepared.document); invalidate_output(); source_page();
      status_.set_text("Documento criado na cópia: " + created.title + " · reaberto e disponível em Origens / Biblioteca do editor principal. A montagem continua em memória.");
    });
  }); });
}
}
