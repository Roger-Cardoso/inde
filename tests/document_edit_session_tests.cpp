#include "inde/application/document_edit_session.hpp"
#include "inde/project/manifest.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace inde::application;
using namespace inde::project;
namespace {
DocumentEditState state(std::string text) {
  Document d;
  d.id = new_uuid(); d.title = "Rascunho"; d.content = std::move(text);
  d.created_at = d.updated_at = "2026-10-03T12:00:00Z";
  return {d, {0, 0}};
}
template<class F> void rejects(F f) {
  bool rejected = false;
  try { f(); } catch (const std::exception &) { rejected = true; }
  assert(rejected);
}
void search_unicode() {
  auto r = find_text("Água água AGUA águaX\n🙂 água fim.", {"água", false, true});
  assert(r.matches.size() == 3 && r.matches[0].start == 0 && r.matches[1].start == 5);
  assert(r.matches[2].start == 23 && r.matches[2].end == 27);
  assert(find_text("Straße STRASSE", {"strasse", false, true}).matches.size() == 2);
  assert(find_text("ß", {"s", false, false}).matches.empty());
  assert(find_text("ß", {"ss", false, false}).matches.front().end == 1);
  assert(find_text("café café", {"café", true, true}).matches.front().start == 6);
  assert(find_text("café", {"cafe", true, true}).matches.empty());
  assert(find_text("a_a a-a 'a'", {"a", true, true}).matches.size() == 3);
  assert(find_text("aaaa", {"aa", true, false}).matches.size() == 2);
  assert(find_text("Água água", {"água", true, true}).matches.size() == 1);
  assert(find_text("a a a", {"a", true, true, 2}).truncated);
  assert(!find_text("a a", {"a", true, true, 2}).truncated);
  assert(find_text("text", {""}).matches.empty());
  rejects([] { return find_text(std::string("\xff", 1), {"a"}); });
  rejects([] { return find_text("text", {"a", true, false, 0}); });
  std::stop_source cancelled; cancelled.request_stop();
  assert(find_text(std::string(1000000, 'a'), {"a"}, cancelled.get_token()).matches.empty());
}
void replacement_layers() {
  auto a = state("ábc ábc");
  a.selection = {7, 4};
  a.document.formatting = {{DocumentTextStyle::Bold, 0, 1}, {DocumentTextStyle::Italic, 1, 3},
                           {DocumentTextStyle::Underline, 4, 7}};
  const auto id = new_uuid();
  a.document.anchors = {{id, "Trecho", 1, 2}, {new_uuid(), "Ponto", 3, 3}};
  a.document.entity_references = {{new_uuid(), new_uuid(), id, "Autoral"}};
  auto b = replace_text(a, find_text(a.document.content, {"ábc"}), "🙂X");
  assert(b.document.content == "🙂X 🙂X");
  assert(b.document.formatting.size() == 2);
  assert(b.document.formatting[0].start_offset == 0 && b.document.formatting[0].end_offset == 2);
  assert(b.document.formatting[1].start_offset == 3 && b.document.formatting[1].end_offset == 5);
  assert(b.document.anchors[0].id == id && b.document.anchors[0].start_offset == 0 && b.document.anchors[0].end_offset == 2);
  assert(b.document.entity_references[0].anchor_id == id && b.document.entity_references[0].notes == "Autoral");
  assert(b.selection.cursor == 5 && b.selection.bound == 3);
  auto erased = replace_text(a, find_text(a.document.content, {"ábc"}), "");
  assert(erased.document.content == " " && erased.document.formatting.empty());
  assert(erased.document.anchors[0].start_offset == 0 && erased.document.anchors[0].end_offset == 0);
  auto no_op = replace_text(a, find_text(a.document.content, {"ábc"}), "ábc");
  assert(no_op.document.anchors[0].start_offset == 1 && no_op.document.anchors[0].end_offset == 2);
  auto mixed = replace_text(state("A a A"), find_text("A a A", {"a"}), "A");
  assert(mixed.document.content == "A A A");
  rejects([&] { return replace_text(a, {{{0, 3}, {2, 4}}, false}, "x"); });
  rejects([&] { return replace_text(a, {{{0, 100}}, false}, "x"); });
  rejects([&] { return replace_text(a, {{{0, 3}}, true}, "x"); });
  rejects([&] { return replace_text(a, {{{0, 3}}, false}, std::string("\xff", 1)); });
  auto adjacent = replace_text(state("aa"), {{{0, 1}, {1, 2}}, false}, "bb");
  assert(adjacent.document.content == "bbbb");
}
void session_history() {
  auto a = state("one one"); a.selection = {7, 4};
  DocumentEditSession session; session.open(a);
  assert(!session.dirty() && !session.can_undo());
  auto b = replace_text(a, find_text(a.document.content, {"one"}), "two");
  assert(session.commit(b, "Substituir tudo") && session.undo_count() == 1 && session.dirty());
  session.mark_saved(b.document); assert(!session.dirty());
  auto undo = session.undo(); assert(undo->document.content == a.document.content && undo->selection.bound == 4 && session.dirty());
  auto redo = session.redo(); assert(redo->document.content == b.document.content && !session.dirty());
  static_cast<void>(session.undo()); assert(!session.commit(a) && session.can_redo());
  session.note_mutation(); assert(!session.can_redo() && session.pending());
  auto typing = a; typing.document.content += "!"; typing.selection = {8, 8};
  assert(session.commit(typing));
  auto transient = typing; transient.document.title.clear();
  assert(session.commit(transient));
  assert(session.undo()->document.title == "Rascunho");
  auto bad = typing; bad.document.id = new_uuid();
  rejects([&] { session.commit(bad); });
  bad = typing; bad.selection = {100, 0};
  rejects([&] { session.commit(bad); });
  assert(session.undo()->document.content == a.document.content);
  session.reset(); assert(!session.can_undo() && !session.can_redo() && !session.dirty());
}
void compact_budget() {
  auto a = state(std::string(1024 * 1024, 'x'));
  DocumentEditSession session; session.open(a);
  for (int i = 0; i < 50; ++i) { a.document.content += "🙂"; assert(session.commit(a)); }
  assert(session.undo_count() == 50 && session.history_text_bytes() >= 200 &&
         session.history_text_bytes() < 512 * 1024);
  const auto bytes = session.history_text_bytes();
  static_cast<void>(session.undo()); static_cast<void>(session.redo()); assert(session.history_text_bytes() == bytes);
  DocumentEditSession small(10); auto short_state = state("abc"); small.open(short_state);
  auto large = short_state; large.document.content = std::string(100, 'y');
  rejects([&] { small.commit(large); }); assert(!small.dirty() && !small.can_undo());
  auto unicode = state("aáx"); session.open(unicode);
  auto next = unicode; next.document.content = "aéx";
  session.commit(next); assert(session.undo()->document.content == "aáx");
  assert(session.redo()->document.content == "aéx");
  auto metadata=state("abc");
  metadata.document.anchors={{new_uuid(),std::string(8192,'n'),0,1}};
  DocumentEditSession bounded_metadata(4096); bounded_metadata.open(metadata);
  auto changed=metadata; changed.document.content += "x";
  rejects([&] { bounded_metadata.commit(changed); });
  assert(!bounded_metadata.can_undo() && !bounded_metadata.dirty());
}
}
int main() {
  search_unicode(); replacement_layers(); session_history(); compact_budget();
  std::cout << "PASS: Unicode search, replacement layers, reversible session, compact budget\n";
}
