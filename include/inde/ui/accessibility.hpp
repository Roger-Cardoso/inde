#pragma once

#include <gtkmm.h>

namespace inde::ui {

// gtkmm 4.10 ainda não expõe todos os setters da API de acessibilidade do GTK
// 4. Usar a API C aqui evita que controles apenas com ícone dependam de tooltip
// para serem compreendidos por AT-SPI/leitores de tela.
inline void set_accessible_label(Gtk::Widget &widget,
                                 const Glib::ustring &label) {
  gtk_accessible_update_property(GTK_ACCESSIBLE(widget.gobj()),
                                 GTK_ACCESSIBLE_PROPERTY_LABEL,
                                 label.c_str(), -1);
}

inline void set_accessible_description(Gtk::Widget &widget,
                                       const Glib::ustring &description) {
  gtk_accessible_update_property(GTK_ACCESSIBLE(widget.gobj()),
                                 GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                 description.c_str(), -1);
}

inline void set_accessible_modal(Gtk::Widget &widget, bool modal) {
  gtk_accessible_update_property(GTK_ACCESSIBLE(widget.gobj()),
                                 GTK_ACCESSIBLE_PROPERTY_MODAL, modal, -1);
}

// Placeholders desaparecem ao digitar e não oferecem uma relação de rótulo
// consistente a tecnologias assistivas. Use este auxiliar nos formulários:
// deixa o rótulo visível, cria o atalho mnemônico e comunica o mesmo nome ao
// AT-SPI, inclusive para ComboBoxText e seletores compostos.
inline void append_labeled_form_field(Gtk::Box &form,
                                      const Glib::ustring &label_text,
                                      Gtk::Widget &field,
                                      const Glib::ustring &description = {}) {
  auto *label = Gtk::make_managed<Gtk::Label>(label_text);
  label->set_halign(Gtk::Align::START);
  label->set_xalign(0.0F);
  label->set_mnemonic_widget(field);
  label->add_css_class("heading");
  set_accessible_label(field, label_text);
  if (!description.empty())
    set_accessible_description(field, description);
  form.append(*label);
  form.append(field);
}

} // namespace inde::ui
