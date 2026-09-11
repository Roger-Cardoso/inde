#include "inde/ui/dictionary_manager_dialog.hpp"

#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/proofreading_controller.hpp"

#include <algorithm>
#include <sigc++/adaptors/track_obj.h>
#include <sstream>

namespace inde::ui {
namespace {

class DictionaryManagerPresenter final : public sigc::trackable {
public:
  DictionaryManagerPresenter(Gtk::Window &owner,
                             application::ProofreadingService &service,
                             ProofreadingController &controller)
      : service_(service), controller_(controller) {
    dialog_ = new OverlayDialog("Dicionários e revisão textual", owner, true);
    dialog_->set_secondary_text(
        "O português do Brasil é a base do sistema. Dicionários pessoais "
        "acrescentam nomes, neologismos e vocabulários da obra sem modificar "
        "o texto automaticamente.");
    auto *content = dialog_->get_content_area();
    content->set_spacing(12);

    provider_ = Gtk::make_managed<Gtk::Label>(service_.provider_description());
    provider_->set_xalign(0.0f);
    provider_->set_wrap(true);
    provider_->add_css_class(service_.spelling_available() ? "success"
                                                           : "warning");
    content->append(*provider_);

    error_ = Gtk::make_managed<Gtk::Label>();
    error_->set_xalign(0.0f);
    error_->set_wrap(true);
    error_->add_css_class("error");
    error_->set_visible(false);
    content->append(*error_);
    if (!service_.storage_error().empty())
      show_error("O arquivo de vocabulários não será sobrescrito até ser "
                 "recuperado: " +
                 service_.storage_error());

    auto *create_heading =
        Gtk::make_managed<Gtk::Label>("Criar dicionário pessoal");
    create_heading->add_css_class("heading");
    create_heading->set_xalign(0.0f);
    content->append(*create_heading);
    auto *create_row =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    create_name_ = Gtk::make_managed<Gtk::Entry>();
    create_name_->set_placeholder_text("Ex.: Vocabulário de Arvoredo");
    create_name_->set_hexpand(true);
    auto *language = Gtk::make_managed<Gtk::ComboBoxText>();
    language->append("pt-BR", "Português (Brasil)");
    language->set_active_id("pt-BR");
    language->set_sensitive(false);
    auto *create = Gtk::make_managed<Gtk::Button>("Criar");
    create->add_css_class("suggested-action");
    create->signal_clicked().connect([this] {
      try {
        static_cast<void>(service_.create_dictionary(create_name_->get_text()));
        create_name_->set_text("");
        show_error({});
        schedule_rebuild();
      } catch (const std::exception &problem) {
        show_error(problem.what());
      }
    });
    create_row->append(*create_name_);
    create_row->append(*language);
    create_row->append(*create);
    content->append(*create_row);

    auto *heading = Gtk::make_managed<Gtk::Label>("Dicionários ativos");
    heading->add_css_class("heading");
    heading->set_xalign(0.0f);
    content->append(*heading);
    scroll_ = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll_->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scroll_->set_min_content_height(260);
    scroll_->set_max_content_height(430);
    list_ = Gtk::make_managed<Gtk::ListBox>();
    list_->add_css_class("boxed-list");
    list_->set_selection_mode(Gtk::SelectionMode::NONE);
    scroll_->set_child(*list_);
    content->append(*scroll_);
    rebuild();

    dialog_->add_button("Fechar", Gtk::ResponseType::CLOSE);
    dialog_->signal_response().connect([this](int) { delete this; });
    dialog_->present();
  }

private:
  void show_error(const std::string &message) {
    error_->set_text(message);
    error_->set_visible(!message.empty());
  }

  std::string words_preview(const project::LexicalDictionary &dictionary) {
    if (dictionary.words.empty())
      return "Nenhuma palavra adicionada.";
    std::ostringstream out;
    const auto count = std::min<std::size_t>(dictionary.words.size(), 12);
    for (std::size_t index = 0; index < count; ++index) {
      if (index)
        out << ", ";
      out << dictionary.words[index];
    }
    if (dictionary.words.size() > count)
      out << " e mais " << dictionary.words.size() - count;
    return out.str();
  }

  void schedule_rebuild() {
    Glib::signal_idle().connect_once(
        sigc::track_object([this] { rebuild(); }, *this));
  }

  void rebuild() {
    while (auto *child = list_->get_first_child())
      list_->remove(*child);
    for (const auto &dictionary : service_.dictionaries()) {
      auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
      row->set_margin(10);
      auto *top = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
      auto *name = Gtk::make_managed<Gtk::Label>(dictionary.name);
      name->set_xalign(0.0f);
      name->set_hexpand(true);
      name->add_css_class("heading");
      top->append(*name);
      if (dictionary.category == project::DictionaryCategory::System) {
        auto *category = Gtk::make_managed<Gtk::Label>("Sistema · pt-BR");
        category->add_css_class("dim-label");
        top->append(*category);
      } else {
        auto *enabled = Gtk::make_managed<Gtk::CheckButton>("Ativo");
        enabled->set_active(dictionary.enabled);
        enabled->signal_toggled().connect([this, id = dictionary.id, enabled] {
          try {
            service_.set_dictionary_enabled(id, enabled->get_active());
            controller_.refresh_all();
            show_error({});
          } catch (const std::exception &problem) {
            show_error(problem.what());
          }
        });
        top->append(*enabled);
        auto *remove = Gtk::make_managed<Gtk::Button>("Remover");
        remove->add_css_class("destructive-action");
        remove->signal_clicked().connect([this, id = dictionary.id] {
          try {
            service_.remove_dictionary(id);
            controller_.refresh_all();
            show_error({});
            schedule_rebuild();
          } catch (const std::exception &problem) {
            show_error(problem.what());
          }
        });
        top->append(*remove);
      }
      row->append(*top);

      if (dictionary.category == project::DictionaryCategory::System) {
        auto *detail = Gtk::make_managed<Gtk::Label>(
            "Ortografia brasileira fornecida pelo sistema; as regras "
            "gramaticais do INDE permanecem ativas em conjunto.");
        detail->set_xalign(0.0f);
        detail->set_wrap(true);
        detail->add_css_class("dim-label");
        row->append(*detail);
      } else {
        auto *preview =
            Gtk::make_managed<Gtk::Label>(words_preview(dictionary));
        preview->set_xalign(0.0f);
        preview->set_wrap(true);
        preview->add_css_class("dim-label");
        row->append(*preview);
        auto *add_row =
            Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
        auto *word = Gtk::make_managed<Gtk::Entry>();
        word->set_placeholder_text("Adicionar palavra");
        word->set_hexpand(true);
        auto *add = Gtk::make_managed<Gtk::Button>("Adicionar");
        add->signal_clicked().connect([this, id = dictionary.id, word] {
          try {
            service_.add_word(id, word->get_text());
            controller_.refresh_all();
            show_error({});
            schedule_rebuild();
          } catch (const std::exception &problem) {
            show_error(problem.what());
          }
        });
        add_row->append(*word);
        add_row->append(*add);
        row->append(*add_row);
      }
      list_->append(*row);
    }
    ProofreadingController::watch_with_global(*list_);
  }

  application::ProofreadingService &service_;
  ProofreadingController &controller_;
  OverlayDialog *dialog_{};
  Gtk::Label *provider_{};
  Gtk::Label *error_{};
  Gtk::Entry *create_name_{};
  Gtk::ScrolledWindow *scroll_{};
  Gtk::ListBox *list_{};
};

} // namespace

void show_dictionary_manager(Gtk::Window &owner,
                             application::ProofreadingService &service,
                             ProofreadingController &controller) {
  static_cast<void>(new DictionaryManagerPresenter(owner, service, controller));
}

} // namespace inde::ui
