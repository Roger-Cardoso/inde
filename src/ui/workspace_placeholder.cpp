#include "inde/ui/workspace_placeholder.hpp"

namespace inde::ui {

WorkspacePlaceholder::WorkspacePlaceholder(const Glib::ustring& title,
                                           const Glib::ustring& purpose,
                                           const Glib::ustring& next_milestone)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 12), title_(title), purpose_(purpose),
      milestone_(next_milestone) {
    set_halign(Gtk::Align::CENTER);
    set_valign(Gtk::Align::CENTER);
    set_size_request(480, -1);
    set_margin(32);
    add_css_class("placeholder-card");

    eyebrow_.add_css_class("dim-label");
    eyebrow_.set_halign(Gtk::Align::START);
    title_.add_css_class("title-1");
    title_.set_halign(Gtk::Align::START);
    purpose_.set_wrap(true);
    purpose_.set_halign(Gtk::Align::START);
    purpose_.set_xalign(0.0F);
    milestone_label_.add_css_class("heading");
    milestone_label_.set_halign(Gtk::Align::START);
    milestone_.add_css_class("dim-label");
    milestone_.set_wrap(true);
    milestone_.set_halign(Gtk::Align::START);
    milestone_.set_xalign(0.0F);

    append(eyebrow_);
    append(title_);
    append(purpose_);
    append(separator_);
    append(milestone_label_);
    append(milestone_);
}

} // namespace inde::ui
