#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/proofreading_controller.hpp"

#include <algorithm>
#include <memory>
#include <utility>

namespace inde::ui {

void set_overlay_revealer_open(Gtk::Revealer &revealer, bool open) {
  revealer.set_can_target(open);
  revealer.set_reveal_child(open);
}

ModalOverlayHost::ModalOverlayHost() {
  g_weak_ref_init(&return_focus_,nullptr);
  modal_layer_.set_hexpand(true);
  modal_layer_.set_vexpand(true);
  modal_layer_.set_halign(Gtk::Align::FILL);
  modal_layer_.set_valign(Gtk::Align::FILL);
  modal_layer_.set_can_target(true);
  modal_layer_.set_focusable(true);
  modal_layer_.add_css_class("modal-scrim");
  set_accessible_label(modal_layer_, "Janela modal");
  set_accessible_description(
      modal_layer_,
      "O conteúdo principal está indisponível até fechar este formulário.");
  set_accessible_modal(modal_layer_, true);

  modal_card_.set_size_request(680, -1);
  modal_card_.set_halign(Gtk::Align::CENTER);
  modal_card_.set_valign(Gtk::Align::CENTER);
  modal_card_.set_margin(28);
  modal_card_.add_css_class("modal-card");

  modal_header_.set_spacing(12);
  modal_header_.add_css_class("modal-header");
  modal_heading_.set_hexpand(true);
  modal_heading_.set_spacing(4);
  modal_title_.set_halign(Gtk::Align::START);
  modal_title_.set_xalign(0.0F);
  modal_title_.set_wrap(true);
  modal_title_.add_css_class("title-2");
  modal_secondary_.set_halign(Gtk::Align::START);
  modal_secondary_.set_xalign(0.0F);
  modal_secondary_.set_wrap(true);
  modal_secondary_.set_max_width_chars(72);
  modal_secondary_.add_css_class("modal-secondary");
  modal_heading_.append(modal_title_);
  modal_heading_.append(modal_secondary_);
  close_button_.set_icon_name("window-close-symbolic");
  close_button_.set_tooltip_text("Fechar");
  set_accessible_label(close_button_, "Fechar formulário");
  close_button_.add_css_class("flat");
  close_button_.add_css_class("circular");
  modal_header_.append(modal_heading_);
  modal_header_.append(close_button_);

  modal_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  modal_scroll_.set_propagate_natural_height(true);
  modal_scroll_.set_min_content_height(90);
  modal_scroll_.set_max_content_height(620);
  modal_scroll_.set_child(modal_body_);
  modal_body_.add_css_class("modal-body");
  // Keep the final control fully reachable above the fixed footer when
  // GTK's entry minimum/natural allocations differ inside the viewport.
  modal_body_.set_margin_bottom(24);

  modal_footer_.set_halign(Gtk::Align::END);
  modal_footer_.set_spacing(8);
  modal_footer_.add_css_class("modal-footer");

  modal_card_.append(modal_header_);
  modal_card_.append(modal_scroll_);
  modal_card_.append(modal_footer_);
  modal_layer_.append(modal_card_);
  add_overlay(modal_layer_);
  modal_layer_.set_visible(false);

  close_button_.signal_clicked().connect([this] {
    if (active_token_ != 0)
      respond(Gtk::ResponseType::CANCEL, active_token_);
  });
  modal_keys_ = Gtk::EventControllerKey::create();
  modal_keys_->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  modal_keys_->signal_key_pressed().connect(
      [this](guint keyval, guint, Gdk::ModifierType modifiers) {
        if (active_token_ == 0)
          return false;
        if(auto *window=dynamic_cast<Gtk::Window *>(get_root()))
          for(auto *focus=window->get_focus();focus;focus=focus->get_parent())
            if(GTK_IS_POPOVER(focus->gobj()) || g_object_get_data(G_OBJECT(focus->gobj()),"inde-preedit-active"))return false;
        const auto mask=static_cast<unsigned>(modifiers)&(GDK_CONTROL_MASK|GDK_SHIFT_MASK|GDK_ALT_MASK|GDK_SUPER_MASK|GDK_META_MASK|GDK_HYPER_MASK);
        if((keyval==GDK_KEY_Return || keyval==GDK_KEY_KP_Enter) && mask==GDK_CONTROL_MASK && primary_response_!=Gtk::ResponseType::NONE) {
          respond(primary_response_,active_token_);return true;
        }
        if (keyval == GDK_KEY_Escape && mask==0) {
          respond(Gtk::ResponseType::CANCEL, active_token_);
          return true;
        }
        if ((keyval == GDK_KEY_Tab || keyval==GDK_KEY_ISO_Left_Tab) &&
            (mask==0 || mask==GDK_SHIFT_MASK || mask==GDK_CONTROL_MASK || mask==(GDK_CONTROL_MASK|GDK_SHIFT_MASK))) {
          focus_next_control(
              static_cast<unsigned int>(modifiers &
                                        Gdk::ModifierType::SHIFT_MASK) != 0U);
          return true;
        }
        return false;
      },
      false);
  modal_layer_.add_controller(modal_keys_);
}

void ModalOverlayHost::set_content(Gtk::Widget &content) { set_child(content); }
ModalOverlayHost::~ModalOverlayHost(){g_weak_ref_clear(&return_focus_);}

void ModalOverlayHost::size_allocate_vfunc(int width, int height,
                                           int baseline) {
  // Reserve actual header/footer space before allocating the scrollable body.
  // A fixed 620px body could overflow a 720px window and hide the last
  // commands.
  const int card_width = std::max(240, std::min(680, width - 56));
  modal_card_.set_size_request(card_width, -1);
  int header{}, footer{}, ignored{};
  gtk_widget_measure(GTK_WIDGET(modal_header_.gobj()), GTK_ORIENTATION_VERTICAL,
                     card_width, &ignored, &header, nullptr, nullptr);
  gtk_widget_measure(GTK_WIDGET(modal_footer_.gobj()), GTK_ORIENTATION_VERTICAL,
                     card_width, &ignored, &footer, nullptr, nullptr);
  const int budget = std::max(40, height - 56 - header - footer - 4);
  modal_scroll_.set_min_content_height(std::min(90, budget));
  modal_scroll_.set_max_content_height(std::min(620, budget));
  Gtk::Overlay::size_allocate_vfunc(width, height, baseline);
}

std::uint64_t ModalOverlayHost::present_dialog(
    const Glib::ustring &title, const Glib::ustring &secondary,
    Gtk::Widget &content, const std::vector<OverlayDialogButton> &buttons,
    const sigc::slot<void(int)> &response) {
  clear_dialog();
  if(auto *window=dynamic_cast<Gtk::Window *>(get_root()))
    g_weak_ref_set(&return_focus_,window->get_focus() ? G_OBJECT(window->get_focus()->gobj()) : nullptr);
  active_token_ = ++generation_;
  response_ = response;
  active_content_ = &content;
  modal_title_.set_text(title);
  set_accessible_label(modal_layer_, "Janela modal: " + title);
  modal_secondary_.set_text(secondary);
  modal_secondary_.set_visible(!secondary.empty());
  modal_body_.append(content);
  const auto *content_box = dynamic_cast<const Gtk::Box *>(&content);
  modal_scroll_.set_visible(!content_box || content_box->get_first_child());

  for (const auto &action : buttons) {
    if(action.shortcut_safe && action.response==Gtk::ResponseType::ACCEPT)primary_response_=action.response;
    auto *button = Gtk::make_managed<Gtk::Button>(action.label);
    if (action.response == Gtk::ResponseType::ACCEPT) {
      button->add_css_class("suggested-action");
    }
    if (action.response == Gtk::ResponseType::REJECT ||
        action.label.lowercase().find("remover") != Glib::ustring::npos)
      button->add_css_class("destructive-action");
    const auto token = active_token_;
    button->signal_clicked().connect([this, response = action.response, token] {
      respond(response, token);
    });
    modal_footer_.append(*button);
  }

  modal_layer_.set_visible(true);
  active_changed_.emit(true);
  focus_initial_control();
  return active_token_;
}

void ModalOverlayHost::dismiss_dialog(std::uint64_t token) {
  if (token == 0 || token != active_token_)
    return;
  clear_dialog();
}

bool ModalOverlayHost::has_dialog() const noexcept {
  return active_token_ != 0;
}

void ModalOverlayHost::respond(int response, std::uint64_t token) {
  if (token == 0 || token != active_token_)
    return;
  auto callback = response_;
  if (callback)
    callback(response);
  // O callback pode apresentar um erro em outro overlay. Nesse caso sua nova
  // geração deve permanecer visível.
  if (active_token_ == token)
    clear_dialog();
}

void ModalOverlayHost::clear_dialog() {
  // Release modal focus while its controls are still alive. GTK must not
  // animate state changes on a focused child after the content is disposed.
  if (active_token_ != 0)
    if (auto *window = dynamic_cast<Gtk::Window *>(get_root()))
      gtk_window_set_focus(GTK_WINDOW(window->gobj()), nullptr);
  if (active_content_ && active_content_->get_parent() == &modal_body_)
    modal_body_.remove(*active_content_);
  active_content_ = nullptr;
  while (auto *child = modal_footer_.get_first_child())
    modal_footer_.remove(*child);
  response_ = {};
  active_token_ = 0;
  primary_response_=Gtk::ResponseType::NONE;
  modal_layer_.set_visible(false);
  active_changed_.emit(false);
  if(auto *object=g_weak_ref_get(&return_focus_)) {
    auto *widget=GTK_WIDGET(object);
    if(gtk_widget_get_mapped(widget) && gtk_widget_get_sensitive(widget))gtk_widget_grab_focus(widget);
    g_object_unref(object);
  }
  g_weak_ref_set(&return_focus_,nullptr);
}

std::vector<Gtk::Widget *> ModalOverlayHost::focusable_controls() const {
  std::vector<Gtk::Widget *> controls;
  const auto collect = [&controls](const auto &self,
                                   Gtk::Widget &widget) -> void {
    if (!widget.get_mapped())
      return; // Inactive Stack pages are visible properties, not visible UI.
    if (widget.get_sensitive() && widget.get_can_focus() &&
        widget.get_focusable())
      controls.push_back(&widget);
    for (auto *child = widget.get_first_child(); child;
         child = child->get_next_sibling())
      self(self, *child);
  };
  collect(collect, const_cast<Gtk::Box &>(modal_card_));
  return controls;
}

void ModalOverlayHost::focus_initial_control() {
  // Never make Return an accidental destructive consent. Forms prefer their
  // first field; confirmations without editable content start at Close.
  if (modal_body_.child_focus(Gtk::DirectionType::TAB_FORWARD)) {
    bool dangerous=false;
    if(auto *window=dynamic_cast<Gtk::Window *>(get_root()))
      for(auto *w=window->get_focus();w && w!=&modal_body_;w=w->get_parent())dangerous|=w->has_css_class("destructive-action");
    if(!dangerous)return;
  }
  close_button_.grab_focus();
}

bool ModalOverlayHost::focus_next_control(bool reverse) {
  const auto controls = focusable_controls();
  if (controls.empty())
    return false;

  Gtk::Widget *current = nullptr;
  if (const auto *window = dynamic_cast<const Gtk::Window *>(get_root()))
    current = const_cast<Gtk::Widget *>(window->get_focus());
  const auto found = std::find(controls.begin(), controls.end(), current);
  const auto index =
      found == controls.end()
          ? (reverse ? controls.size() - 1 : 0)
          : static_cast<std::size_t>(std::distance(controls.begin(), found));
  const auto next = reverse ? (index + controls.size() - 1) % controls.size()
                            : (index + 1) % controls.size();
  controls[next]->grab_focus();
  return true;
}

ModalOverlayHost *find_modal_overlay(Gtk::Window &owner) {
  return dynamic_cast<ModalOverlayHost *>(owner.get_child());
}

OverlayDialog::OverlayDialog(const Glib::ustring &title, Gtk::Window &owner,
                             bool)
    : owner_(owner), title_(title) {}

OverlayDialog::OverlayDialog(Gtk::Window &owner, const Glib::ustring &message,
                             bool, Gtk::MessageType message_type,
                             Gtk::ButtonsType buttons, bool)
    : owner_(owner),
      title_(message_type == Gtk::MessageType::ERROR ? "Erro" : "Confirmação"),
      secondary_(message) {
  if (buttons == Gtk::ButtonsType::YES_NO) {
    add_button("Cancelar", Gtk::ResponseType::NO);
    add_button("Remover", Gtk::ResponseType::YES);
  } else {
    add_button("Fechar", Gtk::ResponseType::CLOSE);
  }
}

OverlayDialog::~OverlayDialog() { hide(); }

void OverlayDialog::add_button(const Glib::ustring &label, int response) {
  buttons_.push_back({label, response});
}
void OverlayDialog::allow_primary_shortcut() {
  const auto count=std::count_if(buttons_.begin(),buttons_.end(),[](const auto &b){return b.response==Gtk::ResponseType::ACCEPT;});
  if(count==1)for(auto &button:buttons_)button.shortcut_safe=button.response==Gtk::ResponseType::ACCEPT;
}

Gtk::Box *OverlayDialog::get_content_area() { return &content_; }

sigc::signal<void(int)> &OverlayDialog::signal_response() {
  return response_signal_;
}

void OverlayDialog::set_title(const Glib::ustring &title) { title_ = title; }

void OverlayDialog::set_secondary_text(const Glib::ustring &text) {
  secondary_ = text;
}

void OverlayDialog::present() {
  auto *host = find_modal_overlay(owner_);
  if (!host)
    return;
  ProofreadingController::watch_with_global(content_);
  token_ = host->present_dialog(
      title_, secondary_, content_, buttons_, [this](int response) {
        keep_open_ = false;
        response_signal_.emit(response);
        if (keep_open_) {
          // The host clears this generation after the footer signal returns.
          // Pending validation must not retain a dangling Window reference
          // when the author closes the window before the idle retry runs.
          token_ = 0;
          struct Retry {
            OverlayDialog *dialog{};
            GWeakRef window;
            explicit Retry(GtkWindow *owner) {
              g_weak_ref_init(&window, G_OBJECT(owner));
            }
            ~Retry() {
              g_weak_ref_clear(&window);
              delete dialog;
            }
          };
          auto retry = std::make_shared<Retry>(owner_.gobj());
          retry->dialog = this;
          Glib::signal_idle().connect_once(sigc::track_object(
              [retry] {
                const auto window =
                    std::unique_ptr<GObject, decltype(&g_object_unref)>(
                        static_cast<GObject *>(g_weak_ref_get(&retry->window)),
                        &g_object_unref);
                // close() can destroy the native GtkWindow before its C++
                // wrapper/trackable is destroyed. Guard both lifetimes.
                if (!window ||
                    reinterpret_cast<GObject *>(retry->dialog->owner_.gobj()) !=
                        window.get())
                  return;
                if (!find_modal_overlay(retry->dialog->owner_))
                  return; // Retained form is disposed with the retry capsule.
                auto *dialog = std::exchange(retry->dialog, nullptr);
                dialog->present();
              },
              owner_));
          return;
        }
        hide();
        // Do not dispose the emitting footer/control during its GTK signal.
        Glib::signal_idle().connect_once([this] { delete this; });
      });
}

void OverlayDialog::show() { present(); }

void OverlayDialog::hide() {
  if (token_ == 0)
    return;
  if (auto *host = find_modal_overlay(owner_))
    host->dismiss_dialog(token_);
  token_ = 0;
}

} // namespace inde::ui
