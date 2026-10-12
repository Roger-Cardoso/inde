#include "inde/ui/author_diagnostics.hpp"
#include "editor_interaction_ui_support.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include <stdexcept>
namespace inde::ui {
using namespace interaction_ui;
void WritingWorkspace::show_text_relation() {
  if (!owner_window())
    return;
  try {
    const auto c = interaction_context();
    const auto linked =
        application::entities_in_text(c.state.document, c.range);
    if (linked.empty())
      throw std::runtime_error(
          "O trecho não contém um elemento da história associado e localizado. Adicione ou vincule o elemento da história primeiro.");
    const auto types = service_.narrative().relation_types();
    const auto entity_types = service_.narrative().entity_types();
    std::vector<project::NarrativeEntity> sources;
    for (const auto &id : linked)
      if (auto e = service_.narrative().entity(id))
        sources.push_back(*e);
    if (sources.empty())
      throw std::runtime_error("Elementos da história do trecho indisponíveis");
    auto *dialog = new OverlayDialog("Adicionar relação ao elemento da história do trecho",
                                     *owner_window(), true);
    dialog->set_secondary_text(
        "Cria uma relação no Projeto. Desfazer no editor não a apaga. "
        "O trecho não será interpretado como prova de um fato.");
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    dialog->add_button("Criar relação no Projeto", Gtk::ResponseType::ACCEPT);
    auto &box = *dialog->get_content_area();
    label(box,
          "Trecho: “" +
              clipped(application::selected_text(c.state.document, c.range)) +
              "”");
    label(box, "Elemento da história associado ao trecho *");
    auto *source = Gtk::make_managed<Gtk::ComboBoxText>();
    source->append("", "Escolha o elemento da história (pode haver mais de uma)");
    for (const auto &e : sources)
      source->append(e.id, e.name + " — " +
                               type_name(entity_types, e.entity_type_id));
    source->set_active(0);
    box.append(*source);
    set_accessible_label(*source, "Elemento da história do trecho");
    auto chosen = std::make_shared<std::optional<project::NarrativeEntity>>();
    auto *target = Gtk::make_managed<IncrementalSelector>(
        "Pesquisar outro elemento da relação",
        [this, entity_types](const std::string &search, std::size_t limit) {
          persistence::EntityQuery q;
          q.search = search;
          q.limit = limit;
          std::vector<IncrementalSelection> result;
          for (const auto &e : service_.narrative().entities(q))
            result.push_back(
                {e.id,
                 e.name + " — " + type_name(entity_types, e.entity_type_id),
                 clipped(e.summary)});
          return result;
        });
    box.append(*target);
    target->signal_selection_changed().connect([this, target, chosen] {
      chosen->reset();
      if (auto id = target->selected_id())
        *chosen = service_.narrative().entity(*id);
    });
    label(box, "Tipo de relação *");
    auto *type = Gtk::make_managed<Gtk::ComboBoxText>();
    type->append("", "Escolha um tipo");
    for (const auto &t : types)
      type->append(t.id, t.name);
    type->set_active(0);
    box.append(*type);
    set_accessible_label(*type, "Tipo de relação");
    auto *advanced = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    auto *new_type = Gtk::make_managed<Gtk::CheckButton>(
        "Criar um novo tipo de relação nesta operação");
    advanced->append(*new_type);
    auto *fields = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    auto *name = entry(*fields, "Nome do novo tipo de relação");
    auto *key = Gtk::make_managed<Gtk::Entry>();
    key->set_text(author::new_category_key());
    auto *technical = Gtk::make_managed<Gtk::Expander>("Identificador técnico (opcional)");
    auto *technical_body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    append_labeled_form_field(*technical_body, "Identificador técnico", *key);
    technical->set_child(*technical_body); fields->append(*technical);
    auto *inverse =
        entry(*fields, "Nome no sentido inverso (ex.: é protegido por)");
    auto *symmetric = Gtk::make_managed<Gtk::CheckButton>(
        "Relação simétrica — o sentido não muda seu significado");
    fields->append(*symmetric);
    fields->set_visible(false);
    advanced->append(*fields);
    new_type->signal_toggled().connect([new_type, type, fields] {
      fields->set_visible(new_type->get_active());
      type->set_sensitive(!new_type->get_active());
    });
    auto *direction = Gtk::make_managed<Gtk::ComboBoxText>();
    direction->append("outgoing", "Elemento da história do trecho → outro elemento");
    direction->append("incoming", "Outro elemento → elemento da história do trecho");
    direction->set_active_id("outgoing");
    box.append(*direction);
    auto *description =
        entry(*advanced, "Descrição / contexto da relação (opcional)");
    auto *options = Gtk::make_managed<Gtk::Expander>("Opções da relação");
    options->set_child(*advanced); box.append(*options);
    auto *preview = label(
        box, "Prévia: escolha elementos da história e o tipo de relação para ver o sentido da relação.");
    const auto refresh = [source, chosen, type, types, sources, direction,
                          new_type, name, symmetric, preview] {
      auto e = std::find_if(sources.begin(), sources.end(), [&](const auto &e) {
        return e.id == source->get_active_id().raw();
      });
      auto t = std::find_if(types.begin(), types.end(), [&](const auto &t) {
        return t.id == type->get_active_id().raw();
      });
      if (e == sources.end() || !*chosen ||
          (!new_type->get_active() && t == types.end())) {
        preview->set_text(
            "Prévia: escolha elementos da história e o tipo de relação para ver o sentido da relação.");
        return;
      }
      bool sym =
          new_type->get_active()
              ? symmetric->get_active()
              : t->directionality == project::RelationDirectionality::Symmetric;
      const auto title =
          new_type->get_active() ? name->get_text().raw() : t->name;
      const bool outgoing = direction->get_active_id().raw() == "outgoing";
      preview->set_text("Prévia: " + (outgoing ? e->name : (**chosen).name) +
                        (sym ? " ↔ " : " → ") + title + (sym ? " ↔ " : " → ") +
                        (outgoing ? (**chosen).name : e->name));
    };
    source->signal_changed().connect(refresh);
    target->signal_selection_changed().connect(refresh);
    type->signal_changed().connect(refresh);
    direction->signal_changed().connect(refresh);
    new_type->signal_toggled().connect(refresh);
    symmetric->signal_toggled().connect(refresh);
    name->signal_changed().connect(refresh);
    const std::weak_ptr<int> life = clipboard_lifetime_;
    dialog->signal_response().connect([this, life, dialog, c, sources, source,
                                       chosen, types, type, new_type, name, key,
                                       inverse, symmetric, direction,
                                       description](int response) {
      if (life.expired())
        return;
      if (response != Gtk::ResponseType::ACCEPT) {
        dialog->hide();
        return;
      }
      try {
        require_interaction_context(c);
        auto e =
            std::find_if(sources.begin(), sources.end(), [&](const auto &e) {
              return e.id == source->get_active_id().raw();
            });
        if (e == sources.end() || !*chosen)
          throw std::runtime_error("Escolha explicitamente as dois elementos da história");
        const bool outgoing = direction->get_active_id().raw() == "outgoing";
        const auto now = project::utc_now();
        project::TextRelationCreation plan;
        plan.source = outgoing ? *e : **chosen;
        plan.target = outgoing ? **chosen : *e;
        plan.create_type = new_type->get_active();
        if (plan.create_type)
          plan.type = {project::new_uuid(),
                       key->get_text().raw(),
                       name->get_text().raw(),
                       inverse->get_text().raw(),
                       {},
                       symmetric->get_active()
                           ? project::RelationDirectionality::Symmetric
                           : project::RelationDirectionality::Directed,
                       false,
                       now,
                       now};
        else {
          auto t = std::find_if(types.begin(), types.end(), [&](const auto &t) {
            return t.id == type->get_active_id().raw();
          });
          if (t == types.end())
            throw std::runtime_error(
                "Escolha o tipo de relação ou crie um novo");
          plan.type = *t;
        }
        plan.relation = {project::new_uuid(),
                         plan.type.id,
                         plan.source.id,
                         plan.target.id,
                         description->get_text().raw(),
                         now,
                         now,
                         {},
                         {},
                         {}};
        flush_changes();
        require_interaction_context(c);
        (void)service_.narrative().create_relation_from_text(plan);
        dialog->hide();
        signal_status_message_.emit(
            "Relação criada no Projeto; o texto não foi alterado. Consulte-a "
            "no Planejamento.");
      } catch (const std::exception &e) {
        const auto message = std::string(author::diagnostic(e.what()));
        dialog->hide();
        Glib::signal_idle().connect_once([this, life, message] {
          if (!life.expired())
            show_error("Relação não criada", std::runtime_error(message));
        });
      }
    });
    dialog->present();
  } catch (const std::exception &e) {
    show_error("Relação contextual indisponível", e);
  }
}
} // namespace inde::ui
