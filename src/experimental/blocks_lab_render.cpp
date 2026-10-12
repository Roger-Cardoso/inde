#include "inde/experimental/blocks_lab_window.hpp"
#include <algorithm>
#include <sstream>
namespace inde::experimental {
namespace {
std::string mode_name(PlacementMode mode) {
  switch (mode) {
  case PlacementMode::Live: return "vinculada · acompanha alterações";
  case PlacementMode::Pinned: return "versão fixa · não acompanha alterações";
  case PlacementMode::Forked: return "bloco independente · acompanha suas versões";
  case PlacementMode::ReferenceOnly: return "referência · sem texto na leitura";
  } return {};
}
void clear(Gtk::ListBox &box) { while (auto child = box.get_first_child()) box.remove(*child); }
void row(Gtk::ListBox &box, const std::string &text) {
  auto label = Gtk::make_managed<Gtk::Label>(text); label->set_xalign(0); label->set_wrap(true); label->set_margin(8); box.append(*label);
}
void formats(Gtk::TextView &view, const project::Document &d) {
  auto buffer = view.get_buffer(); buffer->set_text(d.content);
  // A fresh table avoids accumulating tags across repeated rendering.
  int i{};
  for (const auto &s : d.formatting) {
    auto tag = buffer->create_tag("workshop-format-" + std::to_string(++i));
    using S = project::DocumentTextStyle;
    switch (s.style) {
    case S::Bold: tag->property_weight() = static_cast<int>(Pango::Weight::BOLD); break;
    case S::Italic: tag->property_style() = Pango::Style::ITALIC; break;
    case S::Underline: tag->property_underline() = Pango::Underline::SINGLE; break;
    case S::Strikethrough: tag->property_strikethrough() = true; break;
    case S::Foreground: tag->property_foreground() = s.value; break;
    case S::FontFamily: tag->property_family() = s.value; break;
    case S::FontSize: tag->property_size_points() = std::stod(s.value); break;
    case S::AlignCenter: tag->property_justification() = Gtk::Justification::CENTER; break;
    case S::AlignRight: tag->property_justification() = Gtk::Justification::RIGHT; break;
    case S::AlignJustify: tag->property_justification() = Gtk::Justification::FILL; break;
    case S::AlignLeft: tag->property_justification() = Gtk::Justification::LEFT; break;
    case S::Heading: case S::Heading2: case S::Heading3: case S::Heading4: case S::Heading5: case S::Heading6:
      tag->property_weight() = static_cast<int>(Pango::Weight::BOLD); tag->property_scale() = 1.25; break;
    case S::Subheading: tag->property_scale() = 1.1; break;
    case S::Quote: tag->property_style() = Pango::Style::ITALIC; break;
    }
    buffer->apply_tag(tag, buffer->get_iter_at_offset(s.start_offset), buffer->get_iter_at_offset(s.end_offset));
  }
}
}
std::string BlocksLabWindow::origin_title(const std::string &id) const {
  auto source = sources_.find(id); return source == sources_.end() ? "Origem não carregada" : source->second.title;
}
std::string BlocksLabWindow::version_label(const std::string &block, const std::string &id) const {
  const auto &b = state_.lab.block(block); auto r = std::find_if(b.revisions.begin(), b.revisions.end(), [&](auto &v) { return v.id == id; });
  return r == b.revisions.end() ? "versão ausente" : "v" + std::to_string(std::distance(b.revisions.begin(), r) + 1);
}
std::string BlocksLabWindow::occurrence_label(const TextPlacement &p, std::size_t index) const {
  const auto &b = state_.lab.block(p.block_id); const auto &r = state_.lab.revision(p);
  const auto visibility = p.hidden ? "OCULTA por escolha" : state_.lab.visible(p, state_.profile) ? "NA LEITURA" : "OCULTA por alternativa/camada";
  auto text = std::to_string(index) + ". " + b.label + " · " + version_label(b.id, r.id) + "\n" + mode_name(p.mode) + "\n" + origin_title(b.origin_document_id) + "\n" + visibility;
  if (p.variant) text += " · " + state_.lab.variants().at(*p.variant);
  if (p.layer) text += " · camada “" + state_.lab.layers().at(*p.layer) + "”";
  return text;
}
void BlocksLabWindow::source_page() {
  attempt([this] {
    const auto current = documents_.get_active_id(); const auto docs = source_.list(source_page_);
    rendering_ = true; documents_.remove_all();
    for (auto &d : examples_) documents_.append(d.id, d.title);
    for (auto &d : docs) documents_.append(d.id, d.title);
    if (!documents_.set_active_id(current)) documents_.set_active(0);
    rendering_ = false;
    previous_.set_sensitive(source_page_ != 0); next_.set_sensitive(docs.size() == 50); load_source();
  });
}
void BlocksLabWindow::load_source() {
  const auto id = documents_.get_active_id().raw();
  if (!sources_.contains(id)) {
    sources_[id] = source_.load(id); source_references_[id] = source_.outgoing_references(id);
  }
  source_document_ = sources_.at(id); source_text_.get_buffer()->set_text(source_document_.content);
  name_.set_text(source_document_.title);
  status_.set_text("Origem em leitura: " + source_document_.title + " · montagem e rascunhos preservados ao trocar Documento");
}
void BlocksLabWindow::render_versions() {
  rendering_ = true; const auto old = versions_.get_active_id(); versions_.remove_all();
  if (!library_selected_.empty() && state_.lab.blocks().contains(library_selected_)) {
    const auto &b = state_.lab.block(library_selected_);
    for (auto &r : b.revisions) versions_.append(r.id, version_label(b.id, r.id) + (r.id == b.current_revision ? " · atual" : " · histórica"));
    if (!versions_.set_active_id(old)) versions_.set_active_id(b.current_revision);
  }
  rendering_ = false;
}
void BlocksLabWindow::select_occurrence(const std::string &id, bool open) {
  const auto p = state_.lab.occurrence(id); selected_ = id; library_selected_ = p.block_id; render();
  rendering_ = true; versions_.set_active_id(state_.lab.revision(p).id); rendering_ = false;
  if (open && p.mode != PlacementMode::ReferenceOnly) open_editor(p.block_id, state_.lab.revision(p).id);
  const auto projection = state_.lab.project(state_.profile);
  for (auto &segment : projection.segments) if (segment.placement_id == id && !segment.reference_only) {
    auto buffer = read_text_.get_buffer(); buffer->select_range(buffer->get_iter_at_offset(segment.start), buffer->get_iter_at_offset(segment.end));
    auto iter = buffer->get_iter_at_offset(segment.start); read_text_.scroll_to(iter, .1); break;
  }
}
void BlocksLabWindow::render() {
  rendering_ = true; invalidate_output(); clear(library_); clear(occurrences_); library_ids_.clear(); occurrence_ids_.clear();
  std::vector<std::string> ids; for (auto &[id, block] : state_.lab.blocks()) ids.push_back(id);
  std::stable_sort(ids.begin(), ids.end(), [&](auto &a, auto &b) { return state_.lab.block(a).label < state_.lab.block(b).label; });
  for (const auto &id : ids) {
    const auto &b = state_.lab.block(id); const auto uses = state_.lab.impact(id);
    row(library_, b.label + "\n" + origin_title(b.origin_document_id) + " · " + std::to_string(b.revisions.size()) + " versão(ões) · " +
        std::to_string(uses.changed_live.size() + uses.preserved_pinned.size() + uses.navigation_only.size()) + " uso do bloco(s)");
    library_ids_.push_back(id);
    if (id == library_selected_) library_.select_row(*library_.get_row_at_index(library_ids_.size() - 1));
  }
  if (!library_selected_.empty() && !state_.lab.blocks().contains(library_selected_)) library_selected_.clear();
  auto focused = std::find_if(state_.lab.placements().begin(), state_.lab.placements().end(), [&](auto &p) { return p.id == selected_; });
  if (focused == state_.lab.placements().end()) selected_.clear();
  const bool has_use = !selected_.empty(), has_block = !library_selected_.empty();
  for (auto *button : {&repeat_, &follow_, &fork_, &ref_, &origin_, &up_, &down_, &hide_, &remove_}) button->set_sensitive(has_use);
  for (auto *button : {&open_, &reuse_, &rename_}) button->set_sensitive(has_block);
  const bool text_use = has_use && focused->mode != PlacementMode::ReferenceOnly;
  for (auto *button : {&split_, &cursor_split_, &pin_, &alternative_}) button->set_sensitive(text_use);
  promote_.set_sensitive(text_use && focused->variant.has_value()); rename_variant_.set_sensitive(has_use && focused->variant.has_value());
  assign_layer_.set_sensitive(has_use && !state_.lab.layers().empty());
  rename_layer_.set_sensitive(!state_.lab.layers().empty()); layer_visibility_.set_sensitive(!state_.lab.layers().empty());
  auto focus_text = has_use ? "EM FOCO · " + occurrence_label(*focused, std::distance(state_.lab.placements().begin(), focused) + 1) :
      has_block ? "BLOCO EM FOCO · " + state_.lab.block(library_selected_).label + "\nSem uso do bloco selecionado. Reutilize o bloco para colocá-lo na montagem." :
      "SEM TRECHO EM FOCO\nExtraia uma seleção em Origens ou escolha um uso do bloco.";
  std::replace(focus_text.begin(), focus_text.end(), '\n', ' '); focus_info_.set_text(focus_text);
  std::string map, impact;
  if (!library_selected_.empty()) {
    const auto &b = state_.lab.block(library_selected_); impact = "USOS DE “" + b.label + "”\nAplicar uma versão atualiza vinculadas e independentes deste bloco; versões fixas ficam preservadas.\n\n";
  }
  std::size_t index{}; const auto projection = state_.lab.project(state_.profile);
  for (const auto &p : state_.lab.placements()) {
    const auto label = occurrence_label(p, ++index); row(occurrences_, label); occurrence_ids_.push_back(p.id);
    if (p.id == selected_) occurrences_.select_row(*occurrences_.get_row_at_index(index - 1));
    map += label + "\n";
    auto segment = std::find_if(projection.segments.begin(), projection.segments.end(), [&](auto &s) { return s.placement_id == p.id; });
    map += segment == projection.segments.end() ? "Não entra na leitura atual.\n\n" :
        segment->reference_only ? "Vínculo de referência: zero caracteres. Use Ver origem.\n\n" :
        "Faixa na leitura: " + std::to_string(segment->start) + "–" + std::to_string(segment->end) + " caracteres.\n\n";
    if (p.block_id == library_selected_) impact += label + "\n\n";
  }
  for (const auto &id : ids) {
    const auto &b = state_.lab.block(id);
    if (std::none_of(state_.lab.placements().begin(), state_.lab.placements().end(), [&](auto &p) { return p.block_id == id; }))
      map += "SEM OCORRÊNCIA · " + b.label + "\nBloco e " + std::to_string(b.revisions.size()) +
          " versão(ões) preservados na biblioteca; não entram na leitura. Reutilizar cria um uso do bloco, não recupera texto apagado.\n\n";
  }
  const auto alt_active = alternatives_.get_active_id(); alternatives_.remove_all(); alternatives_.append("base", "Usar Base em todas as famílias");
  for (auto &[id, label] : state_.lab.variants()) alternatives_.append(id, label);
  if (state_.profile.selected_variants.empty()) alternatives_.set_active_id("base");
  else {
    const bool active_is_chosen = std::any_of(state_.profile.selected_variants.begin(), state_.profile.selected_variants.end(),
        [&](auto &choice) { return choice.second == alt_active.raw(); });
    alternatives_.set_active_id(active_is_chosen ? alt_active.raw() : state_.profile.selected_variants.begin()->second);
  }
  const auto layer_active = layers_.get_active_id(); layers_.remove_all();
  for (auto &[id, label] : state_.lab.layers()) layers_.append(id, label + (state_.profile.visible_layers.contains(id) ? " · visível" : " · oculta"));
  if (!layers_.set_active_id(layer_active) && !state_.lab.layers().empty()) layers_.set_active(0);
  rendering_ = false; render_versions(); render_editors();
  map_text_.get_buffer()->set_text(map.empty() ? "Montagem vazia. Extraia um trecho em Origens para começar." : map);
  impact_text_.get_buffer()->set_text(impact.empty() ? "Escolha um bloco ou uso do bloco para ver todos os usos e as versões preservadas." : impact);
  auto preview = materialize(state_.lab, projection, state_.title.empty() ? "Montagem sem título" : state_.title);
  // Replace the text buffer to give each rendering a bounded, fresh tag table.
  read_text_.set_buffer(Gtk::TextBuffer::create()); formats(read_text_, preview.document);
  std::string info = "Leitura: " + std::to_string(projection.segments.size()) + " uso do bloco(s) visível(is) · " +
      std::to_string(state_.lab.placements().size() - projection.segments.size()) + " ocultas · " + std::to_string(dirty_count()) + " rascunho(s) fora da leitura.";
  if (!selected_.empty()) {
    auto selected = std::find_if(state_.lab.placements().begin(), state_.lab.placements().end(), [&](auto &p) { return p.id == selected_; });
    if (selected != state_.lab.placements().end()) info += " Seleção: “" + state_.lab.block(selected->block_id).label + "” · " + version_label(selected->block_id, state_.lab.revision(*selected).id) + " · " + mode_name(selected->mode);
  }
  for (auto &[group, variant] : state_.profile.selected_variants) info += " | " + state_.lab.variants().at(variant);
  if (!preview.warnings.empty()) info += " · " + std::to_string(preview.warnings.size()) + " aviso(s) de conservação: consulte Preparar novo Documento.";
  read_info_.set_text(info); undo_button_.set_sensitive(!undo_.empty()); redo_button_.set_sensitive(!redo_.empty());
}
}
