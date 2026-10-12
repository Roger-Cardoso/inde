#include "inde/ui/context_menu.hpp"
#include "inde/ui/accessibility.hpp"
#include <algorithm>
#include <memory>

namespace inde::ui {
namespace {
Gdk::Rectangle context_anchor(Gtk::Widget &target, double x, double y) {
  if (auto *text = dynamic_cast<Gtk::TextView *>(&target)) {
    auto buffer = text->get_buffer();
    Gtk::TextIter start, end;
    if (buffer->get_selection_bounds(start, end)) {
      // Anchor after the last selected glyph's line, not on top of the word
      // that requested the menu. Applies equally to right-click and keyboard.
      end.backward_char();
      Gdk::Rectangle rect;
      text->get_iter_location(end, rect);
      int wx{}, wy{};
      text->buffer_to_window_coords(Gtk::TextWindowType::WIDGET,rect.get_x(),rect.get_y()+rect.get_height()+4,wx,wy);
      return Gdk::Rectangle(wx,wy,1,1);
    }
  }
  return Gdk::Rectangle(static_cast<int>(x),static_cast<int>(y),1,1);
}

class ContextMenuGesture final : public Gtk::GestureClick {
public:
  static Glib::RefPtr<ContextMenuGesture> create(Gtk::Widget &target,
                                                  Gtk::Popover &popover) {
    return Glib::make_refptr_for_instance<ContextMenuGesture>(
        new ContextMenuGesture(target, popover));
  }

  ~ContextMenuGesture() noexcept override {
    if (popover_) {
      g_object_remove_weak_pointer(G_OBJECT(popover_->gobj()), reinterpret_cast<gpointer *>(&popover_));
      closed_.disconnect();
      if (popover_->get_parent()) popover_->unparent();
    }
  }

private:
  ContextMenuGesture(Gtk::Widget &target, Gtk::Popover &popover)
      : Glib::ObjectBase(typeid(ContextMenuGesture)), Gtk::GestureClick(),
        target_(&target), popover_(&popover), had_tooltip_(target.get_has_tooltip()) {
    popover.set_parent(target);
    g_object_add_weak_pointer(G_OBJECT(popover.gobj()), reinterpret_cast<gpointer *>(&popover_));
    closed_ = popover.signal_closed().connect([this] {
      if (target_)
        target_->set_has_tooltip(had_tooltip_);
    });
    set_button(3);
    set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    signal_pressed().connect([this](int, double x, double y) {
      if (!popover_)
        return;
      if (target_)
        target_->set_has_tooltip(false);
      const auto anchor = target_ ? context_anchor(*target_,x,y) : Gdk::Rectangle(static_cast<int>(x),static_cast<int>(y),1,1);
      popover_->set_pointing_to(anchor);
      popover_->popup();
      set_state(Gtk::EventSequenceState::CLAIMED);
    });
  }

  Gtk::Widget *target_{};
  Gtk::Popover *popover_{};
  bool had_tooltip_{};
  sigc::connection closed_;
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
    if (action.available)
      popover->signal_show().connect([button, available = std::move(action.available)] {
        button->set_sensitive(available());
      });
    set_accessible_label(*button, action.label);
    button->set_halign(Gtk::Align::FILL);
    button->add_css_class("context-menu-item");
    if (action.destructive)
      button->add_css_class("destructive-action");
    button->signal_clicked().connect(
        [popover, activate = std::move(action.activate)] {
          // A ação pode reconstruir a lista e destruir este botão/popover.
          // Sua função precisa permanecer viva até o término da invocação.
          const auto run = activate;
          popover->popdown();
          if (run)
            run();
        });
    list->append(*button);
  }
  return list;
}

} // namespace

Gtk::Popover *create_action_popover(std::vector<ContextMenuAction> actions) {
  auto *popover = Gtk::make_managed<Gtk::Popover>();
  popover->set_autohide(true);
  popover->set_has_arrow(false);
  popover->set_position(Gtk::PositionType::BOTTOM);
  popover->add_css_class("context-menu");
  auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
  scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  scroll->set_max_content_height(360);
  scroll->set_propagate_natural_height(true);
  scroll->set_child(*build_action_list(std::move(actions), popover));
  popover->set_child(*scroll);
  return popover;
}

Gtk::Popover *attach_context_menu(Gtk::Widget &target,
                         std::vector<ContextMenuAction> actions) {
  auto shared=std::make_shared<const std::vector<ContextMenuAction>>(std::move(actions));
  auto *popover = create_action_popover(*shared);
  target.add_controller(ContextMenuGesture::create(target, *popover));
  auto keys = Gtk::EventControllerKey::create();
  keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  keys->signal_key_pressed().connect([popover,&target,shared](guint key, guint, Gdk::ModifierType modifiers) {
    const auto mask=static_cast<unsigned>(modifiers)&(GDK_CONTROL_MASK|GDK_SHIFT_MASK|GDK_ALT_MASK|GDK_SUPER_MASK|GDK_META_MASK|GDK_HYPER_MASK);
    if (!(key == GDK_KEY_Menu && mask==0) && !(key == GDK_KEY_F10 && mask==GDK_SHIFT_MASK)) {
      if(GTK_IS_TEXT_VIEW(target.gobj()) || GTK_IS_EDITABLE(target.gobj()))return false;
      std::vector<std::string> labels;
      if(key==GDK_KEY_Return && mask==GDK_ALT_MASK)labels={"Editar","Editar…"};
      if(key==GDK_KEY_F2 && mask==0)labels={"Renomear","Renomear…"};
      if(key==GDK_KEY_Delete && mask==0)labels={"Remover","Remover…","Excluir…"};
      if((key==GDK_KEY_d || key==GDK_KEY_D) && mask==GDK_CONTROL_MASK)labels={"Duplicar","Duplicar…"};
      for(const auto &action:*shared)if(std::find(labels.begin(),labels.end(),action.label.raw())!=labels.end() && action.enabled && (!action.available || action.available())) {
        auto run=action.activate;run();return true;
      }
      return false;
    }
    popover->set_pointing_to(context_anchor(target,12,12));
    popover->popup(); return true;
  }, false);
  target.add_controller(keys);
  return popover;
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
  auto shared=std::make_shared<const std::vector<ContextMenuAction>>(std::move(actions));
  button.set_popover(*create_action_popover(*shared));
  // Row focus selects; Menu/Shift+F10 opens the declared actions without a
  // pointer. Connect at map time, when the enclosing row exists.
  button.signal_map().connect(sigc::track_obj([&button,shared] {
    Gtk::Widget *row=button.get_parent();
    while(row && !GTK_IS_LIST_BOX_ROW(row->gobj()) && !GTK_IS_FLOW_BOX_CHILD(row->gobj()))row=row->get_parent();
    if(!row || g_object_get_data(G_OBJECT(button.gobj()),"inde-row-keys"))return;
    g_object_set_data(G_OBJECT(button.gobj()),"inde-row-keys",GINT_TO_POINTER(1));
    auto keys=Gtk::EventControllerKey::create();keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    keys->signal_key_pressed().connect(sigc::track_obj([&button,shared](guint key,guint,Gdk::ModifierType mods) {
      const auto mask=static_cast<unsigned>(mods)&(GDK_CONTROL_MASK|GDK_SHIFT_MASK|GDK_ALT_MASK|GDK_SUPER_MASK|GDK_META_MASK|GDK_HYPER_MASK);
      if(!button.get_mapped() || !button.get_sensitive())return false;
      if((key==GDK_KEY_Menu && mask==0) || (key==GDK_KEY_F10 && mask==GDK_SHIFT_MASK)){button.popup();return true;}
      // These labels are explicit action declarations, not arbitrary visual
      // widgets. An unavailable or absent action never acquires a shortcut.
      std::vector<std::string> labels;
      if(key==GDK_KEY_F2 && mask==0)labels={"Renomear","Renomear…","Nomear"};
      if(key==GDK_KEY_Return && mask==GDK_ALT_MASK)labels={"Editar","Editar…"};
      if((key==GDK_KEY_d || key==GDK_KEY_D) && mask==GDK_CONTROL_MASK)labels={"Duplicar","Duplicar…"};
      if(key==GDK_KEY_Delete && mask==0)labels={"Remover","Remover…","Excluir…"};
      if(labels.empty())return false;
      auto *window=dynamic_cast<Gtk::Window *>(button.get_root());
      if(window)for(auto *f=window->get_focus();f;f=f->get_parent())if(GTK_IS_EDITABLE(f->gobj()) || GTK_IS_TEXT_VIEW(f->gobj()))return false;
      for(const auto &action:*shared)if(std::find(labels.begin(),labels.end(),action.label.raw())!=labels.end() && action.enabled && (!action.available || action.available())){
        auto run=action.activate;run();return true;
      }
      return false;
    },button),false);
    row->add_controller(keys);
  },button));
}

} // namespace inde::ui
