#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/work_setup.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/bounded_choices.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/catalog_vocabulary.hpp"
namespace inde::ui {
void show_work_setup(Gtk::Window &owner, application::ProjectService &service,
                     std::function<void(const std::string &)> completed) {
  if (!service.current())
    return;
  const auto project_id = service.current()->manifest().project_id;
  auto *dialog = new OverlayDialog("Começar uma Obra", owner, true);
  dialog->set_secondary_text("Dê um nome ao trabalho que você quer planejar. "
                             "Você pode alterar os nomes depois.");
  auto *body = dialog->get_content_area();
  auto *explanation = Gtk::make_managed<Gtk::Label>(
      "Obra é o trabalho que você vai escrever ou organizar. " + author::group_name(false, true) +
      " agrupa obras relacionadas; não é um registro jurídico nem exige uma série. Uma única obra pode ter seu próprio agrupamento. A obra começa com plano e manuscrito vazios: nenhum texto, capítulo ou modelo será aplicado.");
  explanation->set_xalign(0);
  explanation->set_wrap(true);
  explanation->set_max_width_chars(72);
  body->append(*explanation);
  auto *title = Gtk::make_managed<Gtk::Entry>();
  title->set_placeholder_text("Título da minha Obra");
  append_labeled_form_field(*body, "Obra", *title);
  auto *universe = Gtk::make_managed<Gtk::ComboBoxText>();
  bound_choice_width(*universe, 44);
  universe->append("new", "Criar um " + author::group_name() + " para esta obra");
  for (const auto &ip : service.catalog().intellectual_properties)
    universe->append(ip.id, ip.title);
  universe->set_active_id("new");
  append_labeled_form_field(*body, author::group_name(false, true),
                            *universe);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text(
      "Nome do " + author::group_name() + " (pode ser igual ao título da obra)");
  auto *field = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
  append_labeled_form_field(*field, "Nome do novo " + author::group_name(), *name,
                            "Se ficar vazio, será usado o título da Obra.");
  body->append(*field);
  universe->signal_changed().connect(sigc::track_obj(
      [universe, field] {
        field->set_visible(universe->get_active_id() == "new");
      },
      *field));
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar Obra", Gtk::ResponseType::ACCEPT);
  dialog->allow_primary_shortcut();
  dialog->signal_response().connect(
      [&service, dialog, title, universe, name, project_id,
       completed = std::move(completed)](int response) {
        if (response != Gtk::ResponseType::ACCEPT)
          return;
        std::string work_id;
        try {
          if (!service.current() ||
              service.current()->manifest().project_id != project_id)
            throw std::runtime_error(
                "O Projeto mudou. Reabra este formulário no Projeto desejado.");
          const auto id = universe->get_active_id().raw();
          const auto value = service.start_work(
              title->get_text().raw(), id == "new" ? "" : id,
              name->get_text().empty() ? title->get_text().raw()
                                       : name->get_text().raw());
          work_id = value.id;
        } catch (const std::exception &e) {
          dialog->set_secondary_text(author::diagnostic(e.what()));
          dialog->keep_open_on_response();
          return;
        }
        if (completed)
          completed(work_id);
      });
  dialog->present();
}
} // namespace inde::ui
