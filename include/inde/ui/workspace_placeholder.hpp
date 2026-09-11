#pragma once

#include <gtkmm.h>

namespace inde::ui {

class WorkspacePlaceholder final : public Gtk::Box {
public:
    WorkspacePlaceholder(const Glib::ustring& title,
                         const Glib::ustring& purpose,
                         const Glib::ustring& next_milestone);

private:
    Gtk::Label eyebrow_{"Espaço de trabalho"};
    Gtk::Label title_;
    Gtk::Label purpose_;
    Gtk::Separator separator_;
    Gtk::Label milestone_label_{"Próximo marco"};
    Gtk::Label milestone_;
};

} // namespace inde::ui

