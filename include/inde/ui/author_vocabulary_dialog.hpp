#pragma once
#include "inde/ui/author_vocabulary.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/accessibility.hpp"
#include <gtkmm.h>

namespace inde::ui {
inline void show_author_vocabulary(Gtk::Window &owner) {
  auto *dialog = new OverlayDialog("Vocabulário do INDE", owner, true);
  dialog->set_secondary_text("Procure pelo nome atual ou antigo. Os novos nomes não mudam seus textos, cadastros ou decisões.");
  dialog->add_button("Fechar", Gtk::ResponseType::CLOSE);
  auto *search = Gtk::make_managed<Gtk::SearchEntry>();
  search->set_placeholder_text("Ex.: entidade, âncora, universo ou ordem");
  set_accessible_label(*search, "Pesquisar nomes atuais e antigos do INDE");
  dialog->get_content_area()->append(*search);
  auto *results = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
  dialog->get_content_area()->append(*results);
  const auto refresh = [search, results] {
    while (auto *child = results->get_first_child()) results->remove(*child);
    const auto query = search->get_text().casefold();
    for (const auto &term : author::terms) {
      const auto corpus = Glib::ustring(std::string(term.name) + " " + std::string(term.aliases) + " " + std::string(term.explanation)).casefold();
      if (!query.empty() && corpus.find(query) == Glib::ustring::npos) continue;
      auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
      auto *heading = Gtk::make_managed<Gtk::Label>(std::string(term.name));
      heading->add_css_class("heading");
      auto *explanation = Gtk::make_managed<Gtk::Label>(std::string(term.explanation));
      for (auto *label : {heading, explanation}) {
        label->set_xalign(0); label->set_wrap(true); label->set_max_width_chars(72);
        card->append(*label);
      }
      results->append(*card);
    }
    if (!results->get_first_child()) {
      auto *empty = Gtk::make_managed<Gtk::Label>("Nenhum termo encontrado. Tente outro nome ou uma palavra mais curta.");
      empty->set_wrap(true); empty->set_xalign(0); results->append(*empty);
    }
  };
  search->signal_search_changed().connect(refresh);
  refresh();
  dialog->present();
}
} // namespace inde::ui
