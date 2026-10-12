#pragma once

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace inde::ui {
inline bool editor_diagnostics_enabled() {
  static const bool enabled = [] {
    const auto *setting = std::getenv("INDE_EDITOR_PROFILE");
    return setting && std::string_view(setting) == "1";
  }();
  return enabled;
}
// Opt-in local diagnostics: no text, excerpts, document names or paths.
inline void editor_diagnostic(std::string_view phase,
                               std::chrono::nanoseconds elapsed) {
  if (editor_diagnostics_enabled())
    std::clog << "INDE_EDITOR_PROFILE phase=" << phase << " elapsed_ms="
              << std::chrono::duration<double, std::milli>(elapsed).count() << '\n';
}
class EditorPhaseTimer {
public:
  explicit EditorPhaseTimer(std::string_view phase) : phase_(phase) {}
  ~EditorPhaseTimer() {
    editor_diagnostic(phase_, std::chrono::steady_clock::now() - start_);
  }
private:
  std::string_view phase_;
  std::chrono::steady_clock::time_point start_{std::chrono::steady_clock::now()};
};
} // namespace inde::ui
