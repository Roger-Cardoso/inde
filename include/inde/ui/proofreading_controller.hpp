#pragma once

#include "inde/application/proofreading_service.hpp"
#include "inde/project/dictionary.hpp"

#include <gtkmm.h>
#include <memory>

namespace inde::ui {

class ProofreadingController {
public:
  explicit ProofreadingController(application::ProofreadingService &service);
  ~ProofreadingController();

  void watch(Gtk::Widget &root);
  void refresh_all();
  void show_current_issue();

  static void set_global(ProofreadingController *controller);
  static void watch_with_global(Gtk::Widget &root);

private:
  void watch_entry(Gtk::Entry &entry);
  void watch_text_view(Gtk::TextView &view);
  void refresh_entry(Gtk::Entry &entry);
  void refresh_text_view(Gtk::TextView &view);
  void remember_focus(Gtk::Widget *widget);
  void show_issue_popover(Gtk::Widget &widget, const project::TextIssue *issue,
                          double x = 0.0, double y = 0.0);
  void close_popover();
  void replace_issue(Gtk::Widget &widget, const project::TextIssue &issue,
                     const std::string &replacement);

  application::ProofreadingService &service_;
  Gtk::Widget *root_{};
  Gtk::Widget *focused_{};
  std::unique_ptr<Gtk::Popover> popover_;
};

} // namespace inde::ui
