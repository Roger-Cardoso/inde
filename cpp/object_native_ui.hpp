#pragma once
#include "inde/application/project_service.hpp"
#include <gtkmm.h>
#include <memory>
namespace inde::ui {
// Real project-bound editor. Content is stored inside the project's documents
// directory; the authoring source is captured losslessly, never overwritten.
class ObjectDocumentWindow final : public Gtk::Window {
public:
  ObjectDocumentWindow(application::ProjectService &, Gtk::Window &,
                       const std::string &document);
  ~ObjectDocumentWindow() override;
  void save_for_project(const std::filesystem::path &);

private:
  friend struct ObjectNativeProbe;
  struct Impl;
  std::unique_ptr<Impl> impl_;
  bool on_close_request() override;
};
} // namespace inde::ui
