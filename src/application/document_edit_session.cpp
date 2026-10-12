#include "inde/application/document_edit_session.hpp"

#include <algorithm>
#include <glib.h>
#include <stdexcept>
#include <tuple>

namespace inde::application {
namespace {
std::uint64_t fingerprint(const std::string &text) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char byte : text) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  return hash;
}
void release_text(project::Document &document) {
  std::string{}.swap(document.content);
}
template <class T, class Key>
bool equal(const std::vector<T> &a, const std::vector<T> &b, Key key) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
                                            [&](const T &left, const T &right) {
                                              return key(left) == key(right);
                                            });
}
bool same_layers(const project::Document &a, const project::Document &b) {
  return std::tie(a.id, a.title, a.editorial_node_id, a.word_goal, a.group_id,
                  a.purpose, a.revision_of_id, a.revision_label,
                  a.perspective) == std::tie(b.id, b.title, b.editorial_node_id,
                                             b.word_goal, b.group_id, b.purpose,
                                             b.revision_of_id, b.revision_label,
                                             b.perspective) &&
         a.inline_commands == b.inline_commands &&
         equal(a.formatting, b.formatting,
               [](const auto &v) {
                 return std::tuple(v.style, v.start_offset, v.end_offset,
                                   v.value);
               }) &&
         equal(a.anchors, b.anchors,
               [](const auto &v) {
                 return std::tie(v.id, v.label, v.start_offset, v.end_offset);
               }) &&
         equal(a.entity_references, b.entity_references,
               [](const auto &v) {
                 return std::tie(v.id, v.entity_id, v.anchor_id, v.notes);
               }) &&
         project::same_semantics(a.semantics, b.semantics);
}
void validate_state(const DocumentEditState &state) {
  if (!g_utf8_validate(state.document.content.data(),
                       static_cast<gssize>(state.document.content.size()),
                       nullptr))
    throw std::runtime_error("A sessão exige texto UTF-8 válido");
  // Draft metadata (for example an temporarily empty title) is undoable.
  // Persistence still performs the complete domain validation before saving.
  if (state.document.id.empty())
    throw std::runtime_error("Documento sem identidade");
  const auto size = project::utf8_character_count(state.document.content);
  for (const auto &span : state.document.formatting) {
    project::validate_format_span(span);
    if (span.start_offset >= span.end_offset || span.end_offset > size)
      throw std::runtime_error("Formatação fora do Documento");
  }
  project::validate_inline_commands(state.document.inline_commands);
  for (const auto &anchor : state.document.anchors)
    if (anchor.start_offset > anchor.end_offset || anchor.end_offset > size)
      throw std::runtime_error("Âncora fora do Documento");
  if (state.selection.cursor > size || state.selection.bound > size)
    throw std::runtime_error("Seleção fora do Documento");
  project::validate_semantic_structure(state.document);
}
EditTransaction transaction(const DocumentEditState &before,
                            const DocumentEditState &after, std::string label) {
  EditTransaction result{std::move(label),
                         before,
                         after,
                         {},
                         fingerprint(before.document.content),
                         fingerprint(after.document.content)};
  const auto &a = before.document.content, &b = after.document.content;
  auto delta = text_delta(a, b);
  const auto changed = delta.removed.size() + delta.inserted.size();
  if (changed <= (a.size() + b.size()) / 2) {
    result.delta = std::move(delta);
    release_text(result.before.document);
    release_text(result.after.document);
  }
  return result;
}
DocumentEditState replay(const DocumentEditState &current,
                         const EditTransaction &entry, bool forward) {
  const auto expected = forward ? entry.before_hash : entry.after_hash;
  const auto &layers = forward ? entry.before.document : entry.after.document;
  if (fingerprint(current.document.content) != expected ||
      !same_layers(current.document, layers))
    throw std::runtime_error(
        "O histórico não corresponde ao estado atual do Documento");
  auto target = forward ? entry.after : entry.before;
  if (entry.delta) {
    const auto &removed =
        forward ? entry.delta->removed : entry.delta->inserted;
    const auto &inserted =
        forward ? entry.delta->inserted : entry.delta->removed;
    target.document.content = current.document.content;
    if (entry.delta->byte_offset > target.document.content.size() ||
        target.document.content.compare(entry.delta->byte_offset,
                                        removed.size(), removed) != 0)
      throw std::runtime_error("Delta textual incompatível com o Documento");
    target.document.content.replace(entry.delta->byte_offset, removed.size(),
                                    inserted);
  }
  // Timestamps belong to persistence, not undo. Identity/layers stay
  // historical.
  target.document.created_at = current.document.created_at;
  target.document.updated_at = current.document.updated_at;
  validate_state(target);
  return target;
}
} // namespace

std::size_t EditTransaction::retained_text_bytes() const {
  // Legacy API name; budget includes retained author metadata as well as text.
  // Conservative accounting, not an allocator/RSS measurement. Otherwise large
  // anchor labels or reference notes could evade the long-session limit.
  const auto metadata = [](const project::Document &d) {
    std::size_t n =
        sizeof(d) + d.title.capacity() + d.id.capacity() +
        d.created_at.capacity() + d.updated_at.capacity() +
        d.perspective.capacity() + d.revision_label.capacity() +
        d.formatting.capacity() * sizeof(project::DocumentFormatSpan) +
        d.anchors.capacity() * sizeof(project::DocumentAnchor) +
        d.entity_references.capacity() *
            sizeof(project::DocumentEntityReference);
    n += d.inline_commands.capacity() * sizeof(project::DocumentInlineCommand);
    for (const auto &f : d.formatting)
      n += f.value.capacity();
    for (const auto &c : d.inline_commands)
      n += c.prefix.capacity() + c.keyword.capacity() + c.label.capacity() +
           c.content.capacity();
    for (const auto *optional :
         {&d.editorial_node_id, &d.group_id, &d.revision_of_id})
      if (*optional)
        n += (*optional)->capacity();
    for (const auto &a : d.anchors)
      n += a.id.capacity() + a.label.capacity();
    for (const auto &r : d.entity_references) {
      n += r.id.capacity() + r.entity_id.capacity() + r.notes.capacity();
      if (r.anchor_id)
        n += r.anchor_id->capacity();
    }
    return n;
  };
  const auto semantic_bytes = [](const project::DocumentSemantics &s) {
    std::size_t n = s.anchors.size() * sizeof(project::SemanticAnchorEvidence) +
                    s.annotations.size() * sizeof(project::TextAnnotation) +
                    s.relations.size() * sizeof(project::AnnotationRelation);
    for (auto &a : s.annotations) {
      n += a.id.size() + a.anchor_id.size() + a.type_key.size() +
           a.source.size() + a.notes.size() + a.opaque_payload.size() +
           a.created_at.size() + a.updated_at.size();
      for (auto &[key, f] : a.fields) {
        n += key.size() + sizeof(f);
        for (auto &v : f.values)
          n += sizeof(std::string) + v.size();
      }
    }
    for (auto &e : s.anchors)
      n += e.anchor_id.size() + e.selected_hash.size() + e.before_hash.size() +
           e.after_hash.size() + e.reason.size();
    for (auto &r : s.relations)
      n += r.id.size() + r.source_annotation_id.size() +
           r.target_annotation_id.size() + r.type_key.size() + r.notes.size() +
           r.created_at.size();
    return n;
  };
  const auto temporal_bytes = [](const project::DocumentSemantics &s) {
    std::size_t n{};
    for (const auto &a : s.annotations) {
      n += a.dates.capacity()*sizeof(project::SemanticDateBinding);
      for(const auto &b:a.dates)n+=b.id.capacity()+b.reference_id.capacity()+b.role.capacity()+b.notes.capacity()+b.created_at.capacity();
      n += a.fictional_time.capacity() *
           sizeof(project::SemanticTemporalBinding);
      for (const auto &b : a.fictional_time) {
        n += b.id.capacity() + b.fictional_axis_id.capacity() +
             b.fictional_start_point_id.capacity() + b.origin.capacity() +
             b.source.capacity() + b.notes.capacity() + b.created_at.capacity();
        if (b.fictional_end_point_id)
          n += b.fictional_end_point_id->capacity();
      }
    }
    return n;
  };
  return before.document.content.size() + after.document.content.size() +
         (delta ? delta->removed.size() + delta->inserted.size() : 0) +
         metadata(before.document) + metadata(after.document) +
         label.capacity() + semantic_bytes(before.document.semantics) +
         semantic_bytes(after.document.semantics) +
         temporal_bytes(before.document.semantics) +
         temporal_bytes(after.document.semantics);
}
DocumentEditSession::DocumentEditSession(std::size_t text_budget)
    : budget_(text_budget) {
  undo_.reserve(51);
  redo_.reserve(51);
}
void DocumentEditSession::open(DocumentEditState state) {
  validate_state(state);
  reset();
  state_ = std::move(state);
  mark_saved(state_->document);
}
void DocumentEditSession::reset() {
  state_.reset();
  undo_.clear();
  redo_.clear();
  pending_ = false;
  saved_layers_ = {};
  saved_hash_ = 0;
}
void DocumentEditSession::note_mutation() {
  if (state_) {
    pending_ = true;
    redo_.clear();
  }
}
void DocumentEditSession::update_selection(TextSelection selection) {
  if (state_ && !pending_)
    state_->selection = selection;
}
bool DocumentEditSession::commit(DocumentEditState next, std::string label) {
  if (!state_ || next.document.id != state_->document.id)
    throw std::runtime_error("Transação pertence a outro Documento");
  validate_state(next);
  if (state_->document.content == next.document.content &&
      same_layers(state_->document, next.document)) {
    state_ = std::move(next);
    pending_ = false;
    return false;
  }
  auto entry = transaction(*state_, next, std::move(label));
  if (entry.retained_text_bytes() > budget_)
    throw std::runtime_error(
        "A operação excede o orçamento do histórico reversível");
  undo_.push_back(std::move(entry));
  state_ = std::move(next);
  pending_ = false;
  redo_.clear();
  const auto maximum =
      state_->document.content.size() > 2 * 1024 * 1024 ? 10U : 50U;
  while (undo_.size() > maximum || history_text_bytes() > budget_)
    undo_.erase(undo_.begin());
  return true;
}
void DocumentEditSession::mark_saved(const project::Document &document) {
  if (!state_ || pending_ || document.content != state_->document.content ||
      !same_layers(document, state_->document))
    throw std::runtime_error(
        "O estado salvo não corresponde à sessão de edição");
  acknowledge_saved(document);
}
void DocumentEditSession::acknowledge_saved(const project::Document &document) {
  if (!state_ || document.id != state_->document.id)
    throw std::runtime_error(
        "Confirmação de salvamento pertence a outro Documento");
  saved_layers_ = document;
  release_text(saved_layers_);
  saved_hash_ = fingerprint(document.content);
  state_->document.created_at = document.created_at;
  state_->document.updated_at = document.updated_at;
}
bool DocumentEditSession::dirty() const {
  return state_ &&
         (pending_ || fingerprint(state_->document.content) != saved_hash_ ||
          !same_layers(state_->document, saved_layers_));
}
std::optional<DocumentEditState> DocumentEditSession::undo() {
  if (pending_)
    throw std::runtime_error("Consolide a edição pendente antes de desfazer");
  if (undo_.empty())
    return {};
  auto target = replay(*state_, undo_.back(), false);
  auto returned = target;
  redo_.push_back(std::move(undo_.back()));
  undo_.pop_back();
  state_ = std::move(target);
  return returned;
}
std::optional<DocumentEditState> DocumentEditSession::redo() {
  if (pending_ || redo_.empty())
    return {};
  auto target = replay(*state_, redo_.back(), true);
  auto returned = target;
  undo_.push_back(std::move(redo_.back()));
  redo_.pop_back();
  state_ = std::move(target);
  return returned;
}
std::size_t DocumentEditSession::history_text_bytes() const {
  std::size_t bytes = 0;
  for (const auto &entry : undo_)
    bytes += entry.retained_text_bytes();
  for (const auto &entry : redo_)
    bytes += entry.retained_text_bytes();
  return bytes;
}
} // namespace inde::application
