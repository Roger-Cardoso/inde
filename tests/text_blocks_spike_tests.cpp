#include "inde/experimental/text_blocks.hpp"
#include "inde/experimental/text_buffer.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/project/manifest.hpp"
#include "inde/project/document_state.hpp"
#include <cassert>
#include <iostream>
#include <random>
using namespace inde;
using namespace inde::experimental;
template <class F> void rejects(F action) {
  bool caught{}; try { action(); } catch (const std::exception &) { caught = true; }
  assert(caught);
}
project::Document document(std::string text) {
  project::Document d; d.id = project::new_uuid(); d.title = "Origem QA";
  d.created_at = d.updated_at = project::utc_now(); d.content = std::move(text); return d;
}
void buffers() {
  const std::string unicode = "á🙂e\xcc\x81 RTL العربية\r\nfim";
  std::string large;
  for (int i = 0; i < 450; ++i) large += unicode;
  for (auto backend : {TextBackend::Contiguous, TextBackend::ImmutableChunks, TextBackend::PieceTable}) {
    TextBuffer b(backend, large); const auto baseline = b;
    assert(b.text() == large);
    rejects([&] { b.replace(1, 1, "split"); });
    rejects([&] { b.replace(0, 0, std::string("\xff", 1)); });
    rejects([&] { b.replace(0, 0, std::string("x\0y", 3)); });
    rejects([&] { b.replace(0, b.bytes() + 1, ""); });
    assert(b.text() == large);
    auto d = document(large); d.formatting = {{project::DocumentTextStyle::Bold, 0, 8}};
    d.anchors = {{project::new_uuid(), "Unicode", 2, 10}};
    std::mt19937 generator(23);
    application::DocumentEditState state{d, {0, 0}};
    for (int i = 0; i < 150; ++i) {
      const auto count = project::utf8_character_count(state.document.content);
      const auto left = generator() % (count + 1);
      const auto right = std::min(count, left + generator() % 5);
      const auto next = application::replace_selection(state, left, right, i % 3 ? unicode : "");
      const auto delta = application::text_delta(state.document.content, next.document.content);
      b.replace(delta.byte_offset, delta.byte_offset + delta.removed.size(), delta.inserted);
      assert(b.text() == next.document.content); state = next;
    }
    auto changed = b; b = baseline; assert(b.text() == large);
    b = changed; assert(b.text() == state.document.content);
    assert(TextBuffer::retained_bytes({baseline, changed}) > 0);
    b.replace(0, b.bytes(), ""); assert(b.text().empty());
    b.replace(0, 0, unicode); assert(b.text() == unicode);
    std::cout << "PASS buffer " << name(backend) << ": UTF-8, randomized edits, metadata oracle, snapshot undo\n";
  }
}
void blocks() {
  auto d = document("Água 🙂\nSegunda linha.\n"); const auto original = d;
  TextBlocksLab lab;
  const std::size_t split = 7;
  const auto count = project::utf8_character_count(d.content);
  const auto a = lab.import_document(d, 0, split), b = lab.import_document(d, split, count);
  const auto p = lab.place(a), q = lab.place(b), duplicate = lab.place(a);
  auto profile = ReadingProfile{};
  assert(lab.project(profile).text == d.content + "Água 🙂\n");
  const auto initial = lab.block(a).current_revision;
  const auto pinned = lab.place(a, PlacementMode::Pinned, initial);
  const auto reference = lab.place(a, PlacementMode::ReferenceOnly);
  auto impact = lab.impact(a);
  assert(impact.changed_live.size() == 2 && impact.preserved_pinned.size() == 1 && impact.navigation_only.size() == 1);
  rejects([&] { lab.place(b, PlacementMode::Pinned, initial); });
  rejects([&] { lab.place(a, PlacementMode::Forked); });
  rejects([&] { lab.place(a, PlacementMode::Live, initial); });
  rejects([&] { lab.revise(a, "stale", "other"); });
  lab.fork(duplicate, initial); const auto fork = lab.placements()[2].block_id;
  assert(fork != a && lab.block(fork).fork_origin->first == a);
  lab.revise(a, initial, "Novo texto 🙂");
  assert(lab.revision(lab.placements()[0]).text == "Novo texto 🙂");
  assert(lab.revision(lab.placements()[2]).text == "Água 🙂\n");
  assert(lab.revision(lab.placements()[3]).id == initial);
  assert(!lab.revision(lab.placements()[0]).exact_source);
  assert(lab.revision(lab.placements()[2]).exact_source->source_body_sha256.size() == 64);
  const auto projection = lab.project(profile);
  assert(projection.segments.back().reference_only && projection.segments.back().start == projection.segments.back().end);
  assert(projection.segments.front().end == project::utf8_character_count("Novo texto 🙂"));
  const auto base = lab;
  rejects([&] { lab.reorder({p, p, duplicate, pinned, reference}); });
  assert(lab.project(profile).text == base.project(profile).text);
  lab.reorder({q, p, duplicate, pinned, reference});
  assert(lab.project(profile).text.starts_with("Segunda linha."));
  assert(lab.composition_id() == base.composition_id());
  assert(lab.blocks().size() == 3 && lab.block(a).current_revision == base.block(a).current_revision);
  const auto v = lab.add_variant("Alternativa"), layer = lab.add_layer("Notas");
  lab.classify(duplicate, v, layer);
  const auto before = lab.project(profile).text;
  profile.variant = v; assert(lab.project(profile).text == before);
  profile.visible_layers.insert(layer); assert(lab.project(profile).text != before);
  rejects([&] { lab.classify(p, "absent", {}); });
  rejects([&] { (void)lab.project({"absent", {}, {}}); });
  rejects([&] { (void)lab.project({{}, {"absent"}, {}}); });
  rejects([&] { lab.remove_block(a); });
  lab.remove_placement(p); lab.remove_placement(pinned); lab.remove_placement(reference);
  rejects([&] { lab.remove_block(a); }); // Fork still records this source identity.
  lab.remove_placement(duplicate); lab.remove_block(fork); lab.remove_block(a);
  assert(project::same_document_state(d, original));
  auto empty = document(""); const auto empty_block = lab.import_document(empty, 0, 0);
  const auto empty_placement = lab.place(empty_block); lab.pin(empty_placement, lab.block(empty_block).current_revision);
  assert(lab.project({}).segments.back().start == lab.project({}).segments.back().end);
  rejects([&] { lab.import_document(d, 0, count + 1); });
  rejects([&] { lab.add_layer(std::string("\xff", 1)); });
  std::cout << "PASS blocks: identity, revisions, modes, fork provenance, variants, layers, order, deletion guards; original intact\n";
}
int main() { buffers(); blocks(); }
