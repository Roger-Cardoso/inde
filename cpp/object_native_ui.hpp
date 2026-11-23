#pragma once
#include "inde/application/project_service.hpp"
#include <functional>
#include <gtkmm.h>
#include <map>
#include <memory>
#include <optional>
namespace inde::ui {
// One mixed sheet owned by WritingWorkspace. Paragraph viewports edit the
// original Writing buffer; specialties change instruments, never the content.
class ObjectDocumentEditor final : public Gtk::Box {
public:
  ObjectDocumentEditor(application::ProjectService &, Gtk::Window &,
                       const std::string &document);
  ~ObjectDocumentEditor() override;
  void synchronize_document(const project::Document &);
  void refresh_text_views();
  void focus_text(int offset, bool reveal = true);
  std::function<Gtk::Widget *(const std::string &)> paragraph_view;
  std::function<std::optional<std::map<std::string, std::string>>(
      const project::Document &)>
      visible_paragraphs;
  std::function<bool()> text_composing;
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
