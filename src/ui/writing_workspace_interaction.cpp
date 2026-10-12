#include "inde/ui/author_diagnostics.hpp"
#include "editor_interaction_ui_support.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include <stdexcept>
namespace inde::ui {
using namespace interaction_ui;
WritingWorkspace::InteractionContext
WritingWorkspace::interaction_context(bool selection) {
  require_completed_composition();
  if (!current_document_ || !service_.current())
    throw std::runtime_error("Abra um Documento no editor");
  commit_undo_group();
  InteractionContext c;
  c.state = capture_edit_state();
  c.epoch = edit_epoch_;
  c.revision = edit_revision_;
  c.project_id = service_.current()->manifest().project_id;
  c.range = {std::min(c.state.selection.cursor, c.state.selection.bound),
             std::max(c.state.selection.cursor, c.state.selection.bound)};
  if (selection)
    (void)application::selected_text(c.state.document, c.range);
  return c;
}
void WritingWorkspace::require_interaction_context(
    const InteractionContext &c) {
  require_completed_composition();
  if (!current_document_ || !service_.current() || edit_epoch_ != c.epoch ||
      edit_revision_ != c.revision ||
      current_document_->id != c.state.document.id ||
      service_.current()->manifest().project_id != c.project_id ||
      !project::same_document_state(c.state.document, capture_editor_state(),
                                    false))
    throw std::runtime_error(
        "O Documento ou Projeto mudou; selecione o trecho e reabra a ação");
  // Focus/PRIMARY ownership can collapse GTK's live selection. The captured
  // Unicode interval is authoritative for this modal, provided text is
  // unchanged.
}
void WritingWorkspace::commit_text_entity(
    const InteractionContext &c, project::Document d,
    const project::TextEntityCreation &plan) {
  require_interaction_context(c);
  flush_changes();
  require_interaction_context(c);
  auto after = c.state;
  after.document = std::move(d);
  auto history = edit_session_;
  history.commit(after, "Criar elemento da história e vincular trecho");
  auto persisted = service_.writing().create_entity_from_text(
      after.document, *saved_document_, plan);
  // The drained worker may still own a durable baseline from an earlier
  // autosave. This compound transaction saved outside that worker; forget its
  // previous baseline so the next job uses the newly acknowledged Document.
  // Do this only after success: a rejected creation leaves its baseline valid.
  save_worker_.clear();
  after.document = persisted;
  try {
    restore_edit_state(after);
  } catch (const std::exception &e) {
    saved_document_ = persisted;
    history.acknowledge_saved(persisted);
    edit_session_ = std::move(history);
    throw std::runtime_error(
        std::string("O elemento da história e o vínculo foram gravados, mas a interface não conseguiu se atualizar. Não repita a criação; reabra o Documento. Detalhe: ") +
        author::diagnostic(e.what()));
  }
  ++edit_revision_;
  history.acknowledge_saved(persisted);
  edit_session_ = std::move(history);
  saved_document_ = persisted;
  saved_revision_ = edit_revision_;
  save_submitted_ = false;
  save_error_.clear();
  current_document_->created_at = persisted.created_at;
  current_document_->updated_at = persisted.updated_at;
  update_undo_actions();
  update_editor_status();
  refresh_semantic_lenses();
  schedule_recovery();
  signal_status_message_.emit("Elemento criado no Projeto e trecho vinculado. Desfazer remove o vínculo, não o cadastro.");
}
void WritingWorkspace::commit_text_date(
    const InteractionContext &c, project::Document d,
    const project::TextDateCreation &plan) {
  require_interaction_context(c);
  flush_changes();
  require_interaction_context(c);
  auto after = c.state;
  after.document = std::move(d);
  auto history = edit_session_;
  history.commit(after, "Criar data e vincular trecho");
  auto persisted = service_.writing().create_date_from_text(
      after.document, *saved_document_, plan);
  // The drained worker may still own a durable baseline from an earlier
  // autosave. This compound transaction saved outside that worker; forget its
  // previous baseline so the next job uses the newly acknowledged Document.
  // Do this only after success: a rejected creation leaves its baseline valid.
  save_worker_.clear();
  after.document = persisted;
  try {
    restore_edit_state(after);
  } catch (const std::exception &e) {
    saved_document_ = persisted;
    history.acknowledge_saved(persisted);
    edit_session_ = std::move(history);
    throw std::runtime_error(
        std::string("A data e o vínculo foram gravados, mas a interface "
                    "não conseguiu se atualizar. Não repita a criação; reabra "
                    "o Documento. Detalhe: ") +
        author::diagnostic(e.what()));
  }
  ++edit_revision_;
  history.acknowledge_saved(persisted);
  edit_session_ = std::move(history);
  saved_document_ = persisted;
  saved_revision_ = edit_revision_;
  save_submitted_ = false;
  save_error_.clear();
  current_document_->created_at = persisted.created_at;
  current_document_->updated_at = persisted.updated_at;
  update_undo_actions();
  update_editor_status();
  refresh_semantic_lenses();
  schedule_recovery();
  signal_status_message_.emit("Data criada no Projeto e trecho vinculado. "
                              "Desfazer remove o vínculo, não a referência.");
}
void WritingWorkspace::show_text_entity(bool create, bool materialization) {
  if (!owner_window())
    return;
  try {
    const auto c = interaction_context();
    const auto types = service_.narrative().entity_types();
    auto *dialog =
        new OverlayDialog(create ? "Adicionar elemento da história a partir do texto"
                          : materialization ? "Associar trecho à ficha"
                                            : "Vincular elemento da história à seleção",
                          *owner_window(), true);
    dialog->set_secondary_text(
        create ? "Cria um cadastro no Projeto e vincula o trecho. Desfazer no "
                 "editor remove o vínculo, mas mantém o cadastro."
               : "Escolha o cadastro a vincular. O texto não será reescrito e "
                 "nenhuma informação será atribuída automaticamente.");
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    dialog->add_button(create            ? "Criar elemento da história e vincular"
                       : materialization ? "Associar trecho"
                                         : "Vincular seleção",
                       Gtk::ResponseType::ACCEPT);
    auto &box = *dialog->get_content_area();
    label(box,
          "Trecho selecionado: “" +
              clipped(application::selected_text(c.state.document, c.range)) +
              "”");
    auto *advanced = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    label(*advanced, "Intervalo: " + std::to_string(c.range.start) + "–" +
                   std::to_string(c.range.end) + " · " +
                   std::to_string(c.range.end - c.range.start) +
                   " caracteres Unicode");
    Gtk::Entry *name{}, *summary{}, *new_name{}, *new_key{}, *new_description{};
    Gtk::ComboBoxText *type{};
    Gtk::CheckButton *new_type{};
    IncrementalSelector *picker{};
    auto chosen = std::make_shared<std::optional<project::NarrativeEntity>>();
    if (create) {
      name = entry(
          box, "Nome do elemento da história",
          clipped(application::selected_text(c.state.document, c.range), 120));
      summary = entry(box, "Resumo / informação do Projeto");
      label(box,"Já existe? Use ‘Vincular elemento da história existente’. Nomes iguais não são fundidos.");
      persistence::EntityQuery possible;possible.search=name->get_text().raw();possible.limit=5;
      const auto matches=service_.narrative().entities(possible);
      if(!matches.empty()) {
        std::string hint="Possíveis cadastros para este nome (até 5; não vinculados automaticamente): ";
        for(const auto &e:matches) hint+=clipped(e.name,60)+" — "+type_name(types,e.entity_type_id)+"; ";
        label(box,hint);
      }
      label(box, "Categoria de elemento *");
      type = Gtk::make_managed<Gtk::ComboBoxText>();
      type->append("", "Escolha um tipo");
      for (const auto &t : types)
        type->append(t.id, author::category_label(t));
      type->set_active(0);
      box.append(*type);
      set_accessible_label(*type, "Categoria de elemento");
      new_type = Gtk::make_managed<Gtk::CheckButton>(
          "Criar uma categoria pessoal nesta operação");
      advanced->append(*new_type);
      auto *fields = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
      new_name = entry(*fields, "Nome da categoria");
      new_key = Gtk::make_managed<Gtk::Entry>();
      new_key->set_text(author::new_category_key());
      auto *technical = Gtk::make_managed<Gtk::Expander>("Identificador técnico (opcional)");
      auto *technical_body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
      append_labeled_form_field(*technical_body, "Identificador técnico", *new_key);
      technical->set_child(*technical_body); fields->append(*technical);
      new_description = entry(*fields, "Descrição do novo tipo");
      fields->set_visible(false);
      advanced->append(*fields);
      new_type->signal_toggled().connect([new_type, type, fields] {
        fields->set_visible(new_type->get_active());
        type->set_sensitive(!new_type->get_active());
      });
    } else {
      picker = Gtk::make_managed<IncrementalSelector>(
          "Pesquisar elemento da história ou informação do Projeto",
          [this, types](const std::string &search, std::size_t limit) {
            persistence::EntityQuery q;
            q.search = search;
            q.limit = limit;
            std::vector<IncrementalSelection> result;
            for (const auto &e : service_.narrative().entities(q))
              result.push_back(
                  {e.id, e.name + " — " + type_name(types, e.entity_type_id),
                   clipped(e.summary)});
            return result;
          });
      box.append(*picker);
      auto *preview = label(box, "Nenhum elemento escolhido. Nomes iguais não são vinculados automaticamente.");
      picker->signal_selection_changed().connect(
          [this, picker, chosen, preview, types] {
            chosen->reset();
            if (auto id = picker->selected_id())
              *chosen = service_.narrative().entity(*id);
            preview->set_text(
                *chosen ? "Vincular a: " + (**chosen).name + " · " +
                              type_name(types, (**chosen).entity_type_id) +
                              "\n" + clipped((**chosen).summary, 400)
                        : "Elemento da história indisponível; escolha novamente.");
          });
    }
    auto *as_materialization = Gtk::make_managed<Gtk::CheckButton>(
        "Escolher como o trecho se relaciona com a ficha");
    as_materialization->set_active(materialization);
    advanced->append(*as_materialization);
    auto *purpose = Gtk::make_managed<Gtk::ComboBoxText>();
    purpose->append("materialization", "Apresenta o elemento ou informação");
    purpose->append(
        "evidence",
        "Serve de evidência — não confirma um fato automaticamente");
    purpose->append("mention", "Menção contextual");
    purpose->append("contradiction", "Contradição / contraponto textual");
    purpose->set_active_id("materialization");
    purpose->set_visible(materialization);
    advanced->append(*purpose);
    as_materialization->signal_toggled().connect([as_materialization, purpose] {
      purpose->set_visible(as_materialization->get_active());
    });
    auto *note = entry(*advanced, "Nota da associação (opcional)");
    label(*advanced, "A associação é do texto: não cria uma ocorrência temporal e não altera as informações do elemento da história.");
    auto *options = Gtk::make_managed<Gtk::Expander>("Opções da associação");
    options->set_child(*advanced); options->set_expanded(materialization); box.append(*options);
    const std::weak_ptr<int> life = clipboard_lifetime_;
    dialog->signal_response().connect([this, life, dialog, c, types, create,
                                       name, summary, type, new_type, new_name,
                                       new_key, new_description, chosen,
                                       as_materialization, purpose,
                                       note](int response) {
      if (life.expired())
        return;
      if (response != Gtk::ResponseType::ACCEPT) {
        dialog->hide();
        return;
      }
      try {
        require_interaction_context(c);
        const bool mat = as_materialization->get_active();
        const auto role = purpose->get_active_id().raw();
        const auto notes = note->get_text().raw();
        project::NarrativeEntity entity;
        project::TextEntityCreation plan;
        if (create) {
          const auto now = project::utc_now();
          plan.create_type = new_type->get_active();
          if (plan.create_type)
            plan.type = {project::new_uuid(),
                         new_key->get_text().raw(),
                         new_name->get_text().raw(),
                         new_description->get_text().raw(),
                         false,
                         now,
                         now};
          else {
            auto t =
                std::find_if(types.begin(), types.end(), [&](const auto &t) {
                  return t.id == type->get_active_id().raw();
                });
            if (t == types.end())
              throw std::runtime_error(
                  "Escolha a categoria do elemento ou crie uma nova categoria");
            plan.type = *t;
          }
          entity = {
              project::new_uuid(),       plan.type.id, name->get_text().raw(),
              summary->get_text().raw(), now,          now};
          plan.entity = entity;
        } else {
          if (!*chosen)
            throw std::runtime_error("Escolha um elemento da história nos resultados");
          entity = **chosen;
          auto current = service_.narrative().entity(entity.id);
          if (!current || !same_entity(entity, *current))
            throw std::runtime_error(
                "O elemento da história mudou; revise os dados em um novo formulário");
        }
        auto d =
            mat ? application::materialize_entity(c.state.document, c.range,
                                                  entity.id, role, notes)
                : application::link_text_entity(c.state.document, c.range,
                                                entity.id, entity.name, notes);
        if (create)
          commit_text_entity(c, std::move(d), plan);
        else {
          apply_semantic_document(std::move(d),
                                  mat ? "Associar trecho à ficha"
                                      : "Vincular elemento da história à seleção");
          signal_status_message_.emit(
              mat ? "Trecho associado; nenhum fato ou tempo da história foi criado"
                  : "Seleção vinculada ao elemento da história do projeto");
        }
        dialog->hide();
      } catch (const std::exception &e) {
        const auto message = std::string(author::diagnostic(e.what()));
        dialog->hide();
        Glib::signal_idle().connect_once([this, life, message] {
          if (!life.expired())
            show_error("Associação não aplicada", std::runtime_error(message));
        });
      }
    });
    dialog->present();
  } catch (const std::exception &e) {
    show_error("Ação sobre o trecho indisponível", e);
  }
}
} // namespace inde::ui
