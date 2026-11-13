#pragma once
#include <gtkmm.h>
#include <filesystem>
int run_main_window_smoke(const Glib::RefPtr<Gtk::Application> &,const std::filesystem::path &,const std::filesystem::path &);

int run_catalog_window_smoke(const Glib::RefPtr<Gtk::Application> &,const std::filesystem::path &,const std::filesystem::path &);

int run_import_review(const Glib::RefPtr<Gtk::Application> &,const std::filesystem::path &,bool);

int run_imported_window_smoke(const Glib::RefPtr<Gtk::Application> &,const std::filesystem::path &,const std::filesystem::path &);

int run_comic_window_smoke(const Glib::RefPtr<Gtk::Application> &,const std::filesystem::path &,const std::filesystem::path &);

int run_comic_workspace(const Glib::RefPtr<Gtk::Application> &,const std::filesystem::path &);
int run_review_integration_smoke(const Glib::RefPtr<Gtk::Application> &,const std::filesystem::path &);
