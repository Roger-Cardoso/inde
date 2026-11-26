#include "object_composite_pilot.hpp"
#include "inde/project/document_paragraphs.hpp"
#include "inde/project/manifest.hpp"
#include "inde/project/writing.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using Session = inde::objects::pilot::Session;
using V = inde::persistence::json::Value;
namespace fs = std::filesystem;

V command(Session &s, const char *name, V::Object args = {}) {
  args["op"] = name;
  args["expected"] = s.request(V::Object{{"op", "view"}}).at("state").at("revision");
  const auto reply = s.request(args);
  if (!reply.at("accepted").as_boolean())
    throw std::runtime_error(reply.at("error").as_string());
  return reply.at("state");
}

int main() try {
  char pattern[] = "/tmp/inde-live-asan-XXXXXX";
  const auto *folder = ::mkdtemp(pattern);
  if (!folder) throw std::runtime_error("mkdtemp");
  const fs::path root(folder);
  inde::project::Document original;
  original.id = inde::project::new_uuid();
  original.title = "Somente teste sintético";
  original.created_at = original.updated_at = "2026-11-26T14:00:00Z";
  original.content = "Abertura\n\n\nTexto preservado 🙂.";
  original.paragraphs = inde::project::identify_paragraphs(original.content);
  const auto owner = inde::project::new_uuid();
  auto after = original;
  after.paragraphs = inde::project::edit_paragraphs(original.paragraphs, 8, 9, "");
  after.content = inde::project::render_paragraphs(after.paragraphs);
  {
    Session s(root / "objects", owner, original, inde::objects::pilot::DocumentBinding::Live);
    auto state = command(s, "set_context", {{"context", "composition"}});
    const auto id = state.at("paragraphs").as_array()[0].at("id");
    for (int i = 0; i < 32; ++i)
      command(s, "set_presentation", {{"id", id}, {"width", 20 + i},
        {"horizontal", "start"}, {"text_align", "left"}, {"vertical", "start"}});
    s.synchronize_document(after);
    for (const auto *action : {"undo", "redo"})
      for (int i = 0; i < 32; ++i) {
        command(s, action);
        if (s.current_document().content != after.content)
          throw std::runtime_error("stale owner restored by history");
      }
    auto foreign = after;
    foreign.id = inde::project::new_uuid();
    bool refused = false;
    try { s.synchronize_document(foreign); }
    catch (const std::exception &) { refused = true; }
    if (!refused || s.current_document().content != after.content)
      throw std::runtime_error("foreign owner was accepted or changed current text");
    command(s, "save");
  }
  {
    Session reopened(root / "objects", owner, after, inde::objects::pilot::DocumentBinding::Live);
    if (reopened.current_document().content != after.content)
      throw std::runtime_error("saved owner mismatch");
  }
  fs::remove_all(root);
  std::cout << "Live owner: 32 undo/redo, foreign refusal and saved readback OK\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
