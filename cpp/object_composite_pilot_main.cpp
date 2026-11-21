#include "object_composite_pilot.hpp"
#include <glib.h>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
  try {
    std::filesystem::path workspace, seed;
    auto profile = inde::objects::pilot::Profile::P2A;
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (i + 1 >= argc)
        throw std::runtime_error("missing_argument");
      const std::string value = argv[++i];
      if (arg == "--workspace")
        workspace = value;
      else if (arg == "--seed-p2a")
        seed = value;
      else if (arg == "--profile" && value == "p2b")
        profile = inde::objects::pilot::Profile::P2B;
      else if (arg == "--profile" && value == "p2a")
        profile = inde::objects::pilot::Profile::P2A;
      else
        throw std::runtime_error("invalid_argument");
    }
    if (workspace.empty())
      throw std::runtime_error("workspace_required");
    inde::objects::pilot::Session session(workspace, profile, seed);
    std::string line;
    while (true) {
      line.clear();
      bool oversized = false;
      char ch{};
      while (std::cin.get(ch) && ch != '\n') {
        if (line.size() < 12 * 1024 * 1024)
          line += ch;
        else
          oversized = true;
      }
      if (line.empty() && !std::cin && !oversized)
        break;
      using V = inde::persistence::json::Value;
      V response;
      try {
        if (oversized)
          throw std::runtime_error("request_limit");
        if (!g_utf8_validate(line.data(), static_cast<gssize>(line.size()),
                             nullptr))
          throw std::runtime_error("invalid_utf8");
        int depth = 0;
        bool quoted = false, escaped = false;
        for (char c : line) {
          if (quoted) {
            if (escaped)
              escaped = false;
            else if (c == '\\')
              escaped = true;
            else if (c == '"')
              quoted = false;
          } else if (c == '"')
            quoted = true;
          else if (c == '[' || c == '{') {
            if (++depth > 12)
              throw std::runtime_error("request_depth");
          } else if (c == ']' || c == '}')
            --depth;
        }
        response = session.request(inde::persistence::json::parse(line));
      } catch (const std::exception &e) {
        response = V::Object{{"accepted", false}, {"error", e.what()}};
      }
      std::cout << inde::objects::pilot::encode_json(response) << '\n'
                << std::flush;
    }
  } catch (const std::exception &e) {
    std::cerr << "OBJ-01: " << e.what() << '\n';
    return 1;
  }
}
