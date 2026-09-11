#include "inde/ui/context_menu.hpp"
#include "inde/ui/accessibility.hpp"

namespace inde::ui {
namespace {

class ContextMenuGesture final : public Gtk::GestureClick {
public:
  static Glib::RefPtr<ContextMenuGesture> create(Gtk::Widget &target,
                                                  Gtk::Popover &popover) {
    return Glib::make_refptr_for_instance<ContextMenuGesture>(
        new ContextMenuGesture(target, popover));
  }

  ~ContextMenuGesture() noexcept override {
    if (popover_ && popover_->get_parent())
      popover_->unparent();
  }

private:
  ContextMenuGesture(Gtk::Widget &target, Gtk::Popover &popover)
      : Glib::ObjectBase(typeid(ContextMenuGesture)), Gtk::GestureClick(),
        target_(&target), popover_(&popover), had_tooltip_(target.get_has_tooltip()) {
    popover.set_parent(target);
    popover.signal_closed().connect([this] {
      if (target_)
        target_->set_has_tooltip(had_tooltip_);
    });
    set_button(3);
    signal_pressed().connect([this](int, double x, double y) {
      if (!popover_)
        return;
      if (target_)
        target_->set_has_tooltip(false);
      const Gdk::Rectangle anchor(static_cast<int>(x), static_cast<int>(y), 1,
                                  1);
      popover_->set_pointing_to(anchor);
      popover_->popup();
    });
  }

  Gtk::Widget *target_{};
  Gtk::Popover *popover_{};
  bool had_tooltip_{};
};

Gtk::Box *build_action_list(std::vector<ContextMenuAction> actions,
                            Gtk::Popover *popover) {
  auto *list = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
  list->set_margin(6);
  list->add_css_class("context-menu-list");
  for (auto &action : actions) {
    if (action.separator_before) {
      auto *separator = Gtk::make_managed<Gtk::Separator>();
      separator->set_margin_top(4);
      separator->set_margin_bottom(4);
      list->append(*separator);
    }
    auto *button = Gtk::make_managed<Gtk::Button>();
    auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 9);
    if (!action.icon_name.empty()) {
      auto *icon = Gtk::make_managed<Gtk::Image>();
      icon->set_from_icon_name(action.icon_name);
      row->append(*icon);
    }
    auto *label = Gtk::make_managed<Gtk::Label>(action.label);
    label->set_halign(Gtk::Align::START);
    label->set_hexpand(true);
    row->append(*label);
    button->set_child(*row);
    button->set_sensitive(action.enabled);
    set_accessible_label(*button, action.label);
    button->set_halign(Gtk::Align::FILL);
    button->add_css_class("context-menu-item");
    if (action.destructive)
      button->add_css_class("destructive-action");
    button->signal_clicked().connect(
        [popover, activate = std::move(action.activate)] {
          popover->popdown();
          if (activate)
            activate();
        });
    list->append(*button);
  }
  return list;
}

} // namespace

Gtk::Popover *create_action_popover(std::vector<ContextMenuAction> actions) {
  auto *popover = Gtk::make_managed<Gtk::Popover>();
  popover->set_autohide(true);
  popover->set_has_arrow(true);
  popover->add_css_class("context-menu");
  popover->set_child(*build_action_list(std::move(actions), popover));
  return popover;
}

void attach_context_menu(Gtk::Widget &target,
                         std::vector<ContextMenuAction> actions) {
  auto *popover = create_action_popover(std::move(actions));
  target.add_controller(ContextMenuGesture::create(target, *popover));
}

void configure_overflow_button(Gtk::MenuButton &button,
                               std::vector<ContextMenuAction> actions,
                               const Glib::ustring &tooltip) {
  button.set_icon_name("view-more-symbolic");
  button.set_tooltip_text(tooltip);
  set_accessible_label(button, tooltip);
  set_accessible_description(button,
                             "Abre ações adicionais para o item atual.");
  button.add_css_class("flat");
  button.add_css_class("circular");
  button.set_popover(*create_action_popover(std::move(actions)));
}

} // namespace inde::ui
