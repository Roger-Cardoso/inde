#pragma once
#include <gtkmm.h>
#include <memory>
namespace inde::ui {
// Native Cairo/Pango view of preserved LaTeX source. Never HTML or a TeX
// process.
class MathPreview final : public Gtk::DrawingArea {
public:
  MathPreview();
  ~MathPreview() override;
  void set_source(const std::string &, bool compact = false);
  const std::string &error() const;
  bool rendered() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
// Lexical boundary before the third-party parser; author source is never
// changed.
void validate_native_math(const std::string &);
} // namespace inde::ui
