#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/document_paragraphs.hpp"
#include "inde/project/manifest.hpp"
#include "inde/project/writing.hpp"
#include "object_composite_pilot.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using V = inde::persistence::json::Value;
using Session = inde::objects::pilot::Session;
namespace p = inde::project;
void check(bool ok, const char *reason) {
  if (!ok)
    throw std::runtime_error(reason);
}
bool equal(const V &a, const V &b) {
  return inde::persistence::json::serialize(a) ==
         inde::persistence::json::serialize(b);
}
V view(Session &s) { return s.request(V::Object{{"op", "view"}}).at("state"); }
V request(Session &s, const char *op, V::Object args = {}) {
  args["op"] = op;
  args["expected"] = view(s).at("revision");
  return s.request(args);
}
V apply(Session &s, const char *op, V::Object args = {}) {
  const auto reply = request(s, op, std::move(args));
  if (!reply.at("accepted").as_boolean())
    throw std::runtime_error(reply.at("error").as_string());
  return reply.at("state");
}
void refused(Session &s, const char *op, V::Object args, const char *reason) {
  const auto before = inde::persistence::json::serialize(view(s));
  const auto reply = request(s, op, std::move(args));
  check(!reply.at("accepted").as_boolean(), "invalid move accepted");
  check(reply.at("error").as_string() == reason, "unexpected refusal");
  check(inde::persistence::json::serialize(view(s)) == before,
        "refusal changed state/revision");
}
p::Document document() {
  p::Document d;
  d.id = p::new_uuid();
  d.title = "Structural fixture";
  d.created_at = d.updated_at = "2026-11-26T15:00:00Z";
  d.content = "Zero\nOne\nTwo\nThree\nFour";
  d.paragraphs = p::identify_paragraphs(d.content);
  d.formatting = {{p::DocumentTextStyle::Bold, 0, 4, {}}};
  d.anchors = {{p::new_uuid(), "Zero", 0, 4}};
  return d;
}
std::string id(const V &s, std::size_t index) {
  return s.at("paragraphs").as_array()[index].at("id").as_string();
}
int main() try {
  char pattern[] = "/tmp/inde-structural-unit-XXXXXX";
  const auto *folder = ::mkdtemp(pattern);
  check(folder, "mkdtemp");
  const std::filesystem::path root(folder);
  const auto owner = p::new_uuid();
  auto d = document();
  {
    Session s(root / "plain", owner, d,
              inde::objects::pilot::DocumentBinding::Live);
    auto state = apply(s, "set_context", {{"context", "structure"}});
    const auto a = id(state, 0), b = id(state, 1), c = id(state, 2);
    const auto author = state.at("author_order");
    state =
        apply(s, "place_beside", {{"id", a}, {"other", b}, {"side", "right"}});
    check(equal(state.at("author_order"), author),
          "pairing promoted visual to reading order");
    check(s.current_document().content == d.content,
          "pairing changed owner text");
    apply(s, "set_context", {{"context", "composition"}});
    state = apply(s, "arrange_objects",
                  {{"id", a},
                   {"other", b},
                   {"side", "right"},
                   {"first", .35},
                   {"second", .65},
                   {"gap", 24}});
    const auto row = state.at("rows").as_array()[0];
    apply(s, "set_context", {{"context", "structure"}});
    state =
        apply(s, "place_beside", {{"id", a}, {"other", b}, {"side", "left"}});
    check(state.at("rows").as_array()[0].at("id").as_string() ==
              row.at("id").as_string(),
          "column swap replaced row identity");
    state =
        apply(s, "place_beside", {{"id", a}, {"other", b}, {"side", "right"}});
    check(equal(state.at("rows").as_array()[0], row),
          "column swap lost custom proportions/gap");
    const auto revision = state.at("revision").as_integer();
    const auto stale = s.request(V::Object{{"op", "move_unit"},
                                           {"expected", revision - 1},
                                           {"id", a},
                                           {"reference", c},
                                           {"after", true}});
    check(!stale.at("accepted").as_boolean() &&
              stale.at("error").as_string() == "stale_session_revision",
          "stale move accepted");
    check(view(s).at("revision").as_integer() == revision,
          "stale move changed revision");
    refused(s, "place_beside", {{"id", c}, {"other", a}, {"side", "left"}},
            "row_target_occupied");
    refused(s, "move_unit", {{"id", a}, {"reference", b}, {"after", false}},
            "reading_group_target_selected");
    state =
        apply(s, "move_unit", {{"id", b}, {"reference", c}, {"after", true}});
    check(equal(state.at("rows").as_array()[0], row),
          "vertical move changed row identity/weights");
    check(s.current_document().content == "Two\nZero\nOne\nThree\nFour",
          "paired owner move order");
    check(s.current_document().anchors[0].start_offset == 4 &&
              s.current_document().anchors[0].end_offset == 8,
          "owner move lost semantic offsets");
    check(state.at("visual_order").as_array()[1].as_string() == b &&
              state.at("visual_order").as_array()[2].as_string() == a,
          "visual member order changed");
    apply(s, "undo");
    check(s.current_document().content == d.content, "undo owner move");
    state = apply(s, "redo");
    check(equal(state.at("rows").as_array()[0], row), "redo lost row");
    state =
        apply(s, "move_unit", {{"id", a}, {"reference", ""}, {"after", false}});
    check(s.current_document().content == "Two\nThree\nFour\nZero\nOne",
          "whole row to end");
    apply(s, "save");
    d = s.current_document();
  }
  {
    Session reopened(root / "plain", owner, d,
                     inde::objects::pilot::DocumentBinding::Live);
    check(reopened.current_document().content == d.content, "saved row reopen");
    check(view(reopened).at("rows").as_array().size() == 1, "saved row lost");
  }
  {
    auto rich = document();
    const auto top = p::new_uuid();
    auto covered = std::make_shared<p::DocumentLeafContent>();
    covered->text = "Private alternative";
    rich.resmas = {
        {p::new_uuid(),
         "Indivisible",
         top,
         rich.paragraphs.rows[1].start,
         rich.paragraphs.rows[2].end,
         {{top, "Current", {}}, {p::new_uuid(), "Private", covered}}}};
    Session s(root / "groups", owner, rich,
              inde::objects::pilot::DocumentBinding::Live);
    auto state = apply(s, "set_context", {{"context", "structure"}});
    auto group =
        std::find_if(state.at("reading_groups").as_array().begin(),
                     state.at("reading_groups").as_array().end(),
                     [](const V &g) { return g.as_array().size() > 1; });
    check(group != state.at("reading_groups").as_array().end(),
          "indivisible fixture missing");
    const auto member = group->as_array().back().as_string(),
               target = id(state, 4);
    refused(s, "place_beside",
            {{"id", member}, {"other", target}, {"side", "left"}},
            "reading_group_split");
    apply(s, "move_unit",
          {{"id", member}, {"reference", target}, {"after", true}});
    check(s.current_document().content == "Zero\nThree\nFour\nOne\nTwo",
          "indivisible group was split");
    check(s.current_document().resmas[0].leaves[1].covered->text ==
              "Private alternative",
          "private alternative lost");
    apply(s, "undo");
    check(s.current_document().content == rich.content, "indivisible undo");
  }
  std::filesystem::remove_all(root);
  std::cout << "Structural units, independent orders, row identity, semantics, "
               "indivisible groups, refusal and reopen: OK\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
