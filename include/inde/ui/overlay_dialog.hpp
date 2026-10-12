#pragma once

#include <gtkmm.h>

#include <cstdint>
#include <vector>

namespace inde::ui {

// Revealers usados como camadas de Gtk::Overlay precisam sair também do
// hit-testing quando fechados. Opacidade/tamanho visual não bastam: uma camada
// recolhida ainda pode interceptar o mouse antes do conteúdo abaixo.
void set_overlay_revealer_open(Gtk::Revealer &revealer, bool open);

struct OverlayDialogButton {
  Glib::ustring label;
  int response{Gtk::ResponseType::NONE};
  bool shortcut_safe{};
};

// Camada modal única da janela principal. Ela mantém os formulários dentro da
// interface, bloqueia o conteúdo ao fundo e oferece dimensões confortáveis.
class ModalOverlayHost final : public Gtk::Overlay {
public:
  ModalOverlayHost();
  ~ModalOverlayHost() override;

  void set_content(Gtk::Widget &content);
  std::uint64_t present_dialog(const Glib::ustring &title,
                               const Glib::ustring &secondary,
                               Gtk::Widget &content,
                               const std::vector<OverlayDialogButton> &buttons,
                               const sigc::slot<void(int)> &response);
  void dismiss_dialog(std::uint64_t token);
  bool has_dialog() const noexcept;
  sigc::signal<void(bool)> &signal_active_changed(){return active_changed_;}

private:
  void size_allocate_vfunc(int width, int height, int baseline) override;
  void respond(int response, std::uint64_t token);
  void clear_dialog();
  bool focus_next_control(bool reverse);
  void focus_initial_control();
  [[nodiscard]] std::vector<Gtk::Widget *> focusable_controls() const;

  Gtk::Box modal_layer_{Gtk::Orientation::HORIZONTAL};
  Gtk::Box modal_card_{Gtk::Orientation::VERTICAL};
  Gtk::Box modal_header_{Gtk::Orientation::HORIZONTAL};
  Gtk::Box modal_heading_{Gtk::Orientation::VERTICAL};
  Gtk::Label modal_title_;
  Gtk::Label modal_secondary_;
  Gtk::Button close_button_;
  Gtk::ScrolledWindow modal_scroll_;
  Gtk::Box modal_body_{Gtk::Orientation::VERTICAL};
  Gtk::Box modal_footer_{Gtk::Orientation::HORIZONTAL};
  Glib::RefPtr<Gtk::EventControllerKey> modal_keys_;
  Gtk::Widget *active_content_{nullptr};
  sigc::slot<void(int)> response_;
  std::uint64_t generation_{0};
  std::uint64_t active_token_{0};
  GWeakRef return_focus_{};
  int primary_response_{Gtk::ResponseType::NONE};
  sigc::signal<void(bool)> active_changed_;
};

ModalOverlayHost *find_modal_overlay(Gtk::Window &owner);

// Adaptador deliberadamente parecido com Gtk::Dialog. Isso permite que os
// formulários legados migrem para overlays sem alterar sua regra de negócio.
class OverlayDialog final {
public:
  OverlayDialog(const Glib::ustring &title, Gtk::Window &owner,
                bool modal = true);
  OverlayDialog(Gtk::Window &owner, const Glib::ustring &message,
                bool use_markup, Gtk::MessageType message_type,
                Gtk::ButtonsType buttons, bool modal = true);
  ~OverlayDialog();

  void add_button(const Glib::ustring &label, int response);
  void allow_primary_shortcut();
  Gtk::Box *get_content_area();
  sigc::signal<void(int)> &signal_response();
  void set_title(const Glib::ustring &title);
  void set_secondary_text(const Glib::ustring &text);
  void present();
  void show();
  void hide();
  // Validation failed: retain the draft and re-present after the emitting
  // footer finishes, instead of disposing the controls inside their signal.
  void keep_open_on_response() { keep_open_ = true; }

private:
  Gtk::Window &owner_;
  Glib::ustring title_;
  Glib::ustring secondary_;
  Gtk::Box content_{Gtk::Orientation::VERTICAL, 10};
  std::vector<OverlayDialogButton> buttons_;
  sigc::signal<void(int)> response_signal_;
  std::uint64_t token_{0};
  bool keep_open_{};
};

} // namespace inde::ui
