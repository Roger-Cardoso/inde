#pragma once

#include <filesystem>
#include <string_view>

namespace inde::ui {

// Narrow compatibility rule: the PCI device observed in this host's i915
// GPU-hang records. Do not disable acceleration for all Intel GPUs.
constexpr bool needs_cairo_renderer(std::string_view vendor,
                                    std::string_view device) noexcept {
  return vendor == "0x8086" && device == "0x0102";
}

bool detect_legacy_intel_graphics(const std::filesystem::path &drm_root);

// Call before GTK initialization. Explicit GSK_RENDERER always takes priority.
// This affects only the current process, never system or desktop settings.
// When the effective backend is Cairo, skip unused accelerator initialization.
bool configure_rendering_backend(
    const std::filesystem::path &drm_root = "/sys/class/drm");

} // namespace inde::ui
