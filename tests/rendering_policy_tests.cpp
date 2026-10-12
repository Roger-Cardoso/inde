#include "../src/ui/rendering_policy.hpp"

#include <filesystem>
#include <fstream>
#include <glib.h>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
struct Fixture {
  std::filesystem::path root;
  std::optional<std::string> original;
  std::optional<std::string> debug, disable;
  Fixture() {
    if (const char *value = g_getenv("GSK_RENDERER"))
      original = value;
    if (const char *value = g_getenv("GDK_DEBUG"))
      debug = value;
    if (const char *value = g_getenv("GDK_DISABLE"))
      disable = value;
    auto *directory = g_dir_make_tmp("inde-renderer-tests-XXXXXX", nullptr);
    require(directory, "Temporary directory unavailable");
    root = directory;
    g_free(directory);
  }
  ~Fixture() {
    if (debug)
      g_setenv("GDK_DEBUG", debug->c_str(), true);
    else
      g_unsetenv("GDK_DEBUG");
    if (disable)
      g_setenv("GDK_DISABLE", disable->c_str(), true);
    else
      g_unsetenv("GDK_DISABLE");
    if (original)
      g_setenv("GSK_RENDERER", original->c_str(), true);
    else
      g_unsetenv("GSK_RENDERER");
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
};
} // namespace

int main() {
  try {
    Fixture fixture;
    require(inde::ui::needs_cairo_renderer("0x8086", "0x0102"),
            "Observed GPU not matched");
    require(!inde::ui::needs_cairo_renderer("0x8086", "0x1234"),
            "Unknown Intel GPU matched");
    require(!inde::ui::needs_cairo_renderer("0x1002", "0x0102"),
            "Other vendor matched");
    require(!inde::ui::detect_legacy_intel_graphics(fixture.root / "missing"),
            "Missing sysfs matched");
    auto device = fixture.root / "card3/device";
    std::filesystem::create_directories(device);
    std::ofstream(device / "vendor") << "0x8086\n";
    std::ofstream(device / "device") << "0x0102\n";
    require(inde::ui::detect_legacy_intel_graphics(fixture.root),
            "Sysfs fixture not detected");
    g_unsetenv("GSK_RENDERER");
    require(inde::ui::configure_rendering_backend(fixture.root),
            "Compatibility default not applied");
    require(std::string(g_getenv("GSK_RENDERER")) == "cairo",
            "Wrong renderer default");
    const std::string feature_flags =
        std::string(g_getenv("GDK_DISABLE") ? g_getenv("GDK_DISABLE") : "") +
        " " + std::string(g_getenv("GDK_DEBUG") ? g_getenv("GDK_DEBUG") : "");
    require(feature_flags.find("gl") != std::string::npos &&
                feature_flags.find("vulkan") != std::string::npos,
            "Cairo still probes accelerators");
    const auto before_debug = std::string(
                   g_getenv("GDK_DEBUG") ? g_getenv("GDK_DEBUG") : ""),
               before_disable = std::string(
                   g_getenv("GDK_DISABLE") ? g_getenv("GDK_DISABLE") : "");
    require(!inde::ui::configure_rendering_backend(fixture.root),
            "Explicit Cairo overwritten");
    require(before_debug == std::string(g_getenv("GDK_DEBUG")
                                            ? g_getenv("GDK_DEBUG")
                                            : "") &&
                before_disable == std::string(g_getenv("GDK_DISABLE")
                                                  ? g_getenv("GDK_DISABLE")
                                                  : ""),
            "Repeated policy duplicates flags");
    g_setenv("GSK_RENDERER", "gl", true);
    require(!inde::ui::configure_rendering_backend(fixture.root),
            "Explicit choice overwritten");
    require(std::string(g_getenv("GSK_RENDERER")) == "gl",
            "Explicit choice lost");
    g_unsetenv("GSK_RENDERER");
    std::ofstream(device / "device") << "0x1234\n";
    require(!inde::ui::configure_rendering_backend(fixture.root),
            "Unknown GPU default overridden");
    require(!g_getenv("GSK_RENDERER"), "Unknown GPU changed environment");
    std::cout << "PASS: renderer detection, missing/unknown GPU and explicit "
                 "override\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
