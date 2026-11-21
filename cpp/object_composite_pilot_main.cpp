#include "object_composite_pilot.hpp"
#include <glib.h>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[1]) != "--workspace") {
    std::cerr
        << "Uso: inde_object_composite_pilot --workspace PASTA_EXPERIMENTAL\n";
    return 2;
  }
  try {
    inde::objects::pilot::Session session(argv[2]);
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
    std::cerr << "P2-A: " << e.what() << '\n';
    return 1;
  }
}
