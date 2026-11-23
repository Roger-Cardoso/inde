#pragma once
#include "inde/application/project_service.hpp"
#include <functional>
#include <gtkmm.h>
#include <map>
#include <memory>
#include <optional>
namespace inde::ui {
// Composition surface owned by WritingWorkspace. The original TextView remains
// the text authoring surface; both specialties use the same complete Document.
class ObjectDocumentEditor final : public Gtk::Box {
public:
  ObjectDocumentEditor(application::ProjectService &, Gtk::Window &,
                       const std::string &document);
  ~ObjectDocumentEditor() override;
  void synchronize_document(const project::Document &);
  void set_specialty(const std::string &);
  bool can_leave() const;
  void show_tools(bool);
  void set_visible_text(std::optional<std::map<std::string, std::string>>);
  std::function<void(const std::string &)> edit_paragraph;
  std::function<void(bool)> editing_changed;
  std::function<void()> save_document;
  std::function<project::Document(const project::Document &)> commit_document;
  void save_for_project(const std::filesystem::path &);

private:
  friend struct ObjectNativeProbe;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace inde::ui
