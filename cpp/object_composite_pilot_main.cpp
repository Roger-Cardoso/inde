#include "object_composite_pilot.hpp"
#include <glib.h>
#include <iostream>
#include <string>
#ifdef OBJ01_TEST_FAULTS
#include <unistd.h>
#endif

int main(int argc, char **argv) {
  try {
    std::filesystem::path workspace, seed;
    std::string seed_option;
    inde::objects::pilot::PersistenceHook hook;
#ifdef OBJ01_TEST_FAULTS
    std::string fault;
#endif
    auto profile = inde::objects::pilot::Profile::P2A;
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (i + 1 >= argc)
        throw std::runtime_error("missing_argument");
      const std::string value = argv[++i];
      if (arg == "--workspace")
        workspace = value;
      else if (arg == "--seed-p2a" || arg == "--seed-p2b" ||
               arg == "--seed-p2c" || arg == "--seed-p2d") {
        seed = value;
        seed_option = arg;
      } else if (arg == "--profile" && value == "p2d2")
        profile = inde::objects::pilot::Profile::P2D2;
      else if (arg == "--profile" && value == "p2d")
        profile = inde::objects::pilot::Profile::P2D;
      else if (arg == "--profile" && value == "p2c")
        profile = inde::objects::pilot::Profile::P2C;
      else if (arg == "--profile" && value == "p2b")
        profile = inde::objects::pilot::Profile::P2B;
      else if (arg == "--profile" && value == "p2a")
        profile = inde::objects::pilot::Profile::P2A;
#ifdef OBJ01_TEST_FAULTS
      else if (arg == "--test-fault")
        fault = value;
#endif
      else
        throw std::runtime_error("invalid_argument");
    }
    if (workspace.empty())
      throw std::runtime_error("workspace_required");
    const auto expected_seed =
        profile == inde::objects::pilot::Profile::P2D2  ? "--seed-p2d"
        : profile == inde::objects::pilot::Profile::P2D ? "--seed-p2c"
        : profile == inde::objects::pilot::Profile::P2C ? "--seed-p2b"
                                                        : "--seed-p2a";
    if (!seed.empty() && seed_option != expected_seed)
      throw std::runtime_error("invalid_seed_profile");
#ifdef OBJ01_TEST_FAULTS
    hook = [&](std::string_view op,
               inde::objects::pilot::PersistenceStage stage) {
      using S = inde::objects::pilot::PersistenceStage;
      const std::string name = stage == S::BeforeWrite     ? "before_write"
                               : stage == S::AfterBlobs    ? "after_blobs"
                               : stage == S::AfterSnapshot ? "after_snapshot"
                               : stage == S::BeforeCommit  ? "before_commit"
                                                           : "after_commit";
      if (fault == std::string(op) + ":" + name + ":crash")
        ::_exit(86);
      if (fault == std::string(op) + ":" + name + ":throw")
        throw std::runtime_error("injected_persistence_fault");
    };
#endif
    inde::objects::pilot::Session session(workspace, profile, seed,
                                          std::move(hook));
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
