#include "inde/ui/author_diagnostics.hpp"
#include "editor_interaction_ui_support.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include <stdexcept>
namespace inde::ui {
using namespace interaction_ui;
std::vector<project::NarrativeEntity>
WritingWorkspace::inline_project_entities(const std::string &token) {
  auto parsed = application::project_inline_query(token);
  if (!parsed)
    return {};
  persistence::EntityQuery q;
  q.search = parsed->search;
  q.limit = 6;
  if (parsed->type_key) {
    const auto types = service_.narrative().entity_types();
    auto t = std::find_if(types.begin(), types.end(), [&](const auto &t) {
      return t.key == *parsed->type_key;
    });
    if (t == types.end())
      return {};
    q.entity_type_ids = {t->id};
  }
  return service_.narrative().entities(q);
}
void WritingWorkspace::append_project_inline(
    const application::InlineQuery &query) {
  if (!application::project_inline_query(query.token))
    return;
  const auto candidates = inline_project_entities(query.token);
  const auto types = service_.narrative().entity_types();
  if (candidates.empty()) {
    label(inline_suggestions_, "Projeto: nenhum resultado. Use @nome, "
                               "@personagem:nome ou @informacao:termo.");
    return;
  }
  label(inline_suggestions_,
        "Dados do Projeto — escolha e revise antes de inserir");
  const auto epoch = edit_epoch_, revision = edit_revision_;
  const auto document = current_document_->id;
  const auto project_id = service_.current()->manifest().project_id;
  const std::weak_ptr<int> life = clipboard_lifetime_;
  for (const auto &e : candidates) {
    auto t = std::find_if(types.begin(), types.end(), [&](const auto &t) {
      return t.id == e.entity_type_id;
    });
    if (t == types.end())
      continue;
    auto *button = Gtk::make_managed<Gtk::Button>(clipped(e.name, 70) + " — " +
                                                  t->name + " (Projeto)");
    button->set_tooltip_text(clipped(e.summary, 300));
    button->signal_clicked().connect([this, life, epoch, revision, document,
                                      project_id, query, e, type = *t] {
      if (life.expired())
        return;
      close_inline_suggestions();
      try {
        auto c = interaction_context(false);
        if (epoch != c.epoch || revision != c.revision ||
            document != c.state.document.id || project_id != c.project_id ||
            c.state.selection.cursor != query.end ||
            c.state.selection.bound != query.end ||
            application::selected_text(c.state.document,
                                       {query.start, query.end}) != query.token)
          throw std::runtime_error(
              "O comando ou contexto mudou; solicite novamente as sugestões");
        preview_project_inline(c, {query.start, query.end}, e, type);
      } catch (const std::exception &error) {
        show_error("Sugestão não aplicada", error);
      }
    });
    inline_suggestions_.append(*button);
  }
}
void WritingWorkspace::preview_project_inline(InteractionContext c,
                                              application::TextRange token,
                                              project::NarrativeEntity entity,
                                              project::EntityType type) {
  if (!owner_window())
    return;
  auto *dialog = new OverlayDialog("Inserir dados do Projeto no texto",
                                   *owner_window(), true);
  dialog->set_secondary_text(
      "Sugestão a partir dos cadastros da história, não geração de texto. Escolha se quer apenas texto ou também uma associação. O comando digitado será substituído em uma única operação reversível.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Inserir escolha", Gtk::ResponseType::ACCEPT);
  auto &box = *dialog->get_content_area();
  label(box, entity.name + " — " + type.name)->add_css_class("title-3");
  label(box, "Informação do Projeto: " + (entity.summary.empty()
                                              ? "Nenhum resumo cadastrado"
                                              : clipped(entity.summary, 1000)));
  label(box, "Substituir: “" +
                 application::selected_text(c.state.document, token) + "”");
  auto *mode = Gtk::make_managed<Gtk::ComboBoxText>();
  mode->append("", "Escolha explicitamente o que inserir");
  mode->append("name", "Nome apenas — sem vínculo");
  mode->append("name-link", "Nome e vínculo com o elemento da história");
  if (!entity.summary.empty()) {
    mode->append("summary", "Resumo / informação apenas — sem vínculo");
    mode->append("summary-link",
                 "Resumo / informação como apresentação da ficha no texto");
  }
  mode->set_active(0);
  box.append(*mode);
  set_accessible_label(*mode, "Modo de inserção dos dados do Projeto");
  auto *preview = label(box, "Prévia: escolha uma opção acima.");
  mode->signal_changed().connect([mode, preview, entity] {
    const auto key = mode->get_active_id().raw();
    preview->set_text(key.empty() ? "Prévia: escolha uma opção acima."
                                  : "Texto a inserir: “" +
                                        clipped(key.starts_with("name")
                                                    ? entity.name
                                                    : entity.summary,
                                                1200) +
                                        "”");
  });
  persistence::RelationQuery q;
  q.entity_id = entity.id;
  q.limit = 12;
  const auto relations = service_.narrative().relations(q);
  if (!relations.empty()) {
    auto *extra = Gtk::make_managed<Gtk::Expander>(
        "Contexto: relações deste elemento da história (até 12)");
    auto *details = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    const auto relation_types = service_.narrative().relation_types();
    for (const auto &r : relations) {
      const auto other = service_.narrative().entity(
          r.source_entity_id == entity.id ? r.target_entity_id
                                          : r.source_entity_id);
      auto rt = std::find_if(
          relation_types.begin(), relation_types.end(),
          [&](const auto &t) { return t.id == r.relation_type_id; });
      const auto verb = rt == relation_types.end() ? "Relação indisponível"
                        : r.source_entity_id == entity.id ||
                                rt->directionality ==
                                    project::RelationDirectionality::Symmetric
                            ? rt->name
                            : rt->inverse_name;
      label(*details, entity.name + " — " + verb + " — " +
                          (other ? other->name : "Elemento da história indisponível") +
                          "\n" + clipped(r.description));
    }
    extra->set_child(*details);
    box.append(*extra);
  }
  const std::weak_ptr<int> life = clipboard_lifetime_;
  dialog->signal_response().connect([this, life, dialog, c, token, entity, type,
                                     mode](int response) {
    if (life.expired())
      return;
    if (response != Gtk::ResponseType::ACCEPT) {
      dialog->hide();
      return;
    }
    try {
      require_interaction_context(c);
      const auto key = mode->get_active_id().raw();
      if (key.empty())
        throw std::runtime_error("Escolha um modo de inserção");
      auto current = service_.narrative().entity(entity.id);
      const auto types = service_.narrative().entity_types();
      auto t = std::find_if(types.begin(), types.end(),
                            [&](const auto &t) { return t.id == type.id; });
      if (!current || !same_entity(entity, *current) || t == types.end() ||
          t->key != type.key || t->name != type.name)
        throw std::runtime_error(
            "Os dados do Projeto mudaram; revise uma nova sugestão");
      const auto text = key.starts_with("name") ? entity.name : entity.summary;
      auto state =
          application::replace_selection(c.state, token.start, token.end, text);
      const application::TextRange range{
          token.start, token.start + project::utf8_character_count(text)};
      if (key == "name-link")
        state.document = application::link_text_entity(
            state.document, range, entity.id, entity.name,
            "Escolha inline explícita");
      if (key == "summary-link")
        state.document = application::materialize_entity(
            state.document, range, entity.id, "materialization",
            "Informação do cadastro da história inserida por escolha inline explícita");
      service_.writing().validate_semantic_document(state.document);
      apply_edit_transaction(std::move(state),
                             "Inserir dados do Projeto: " + entity.name);
      refresh_semantic_lenses();
      dialog->hide();
      signal_status_message_.emit("Dados do Projeto inseridos por escolha "
                                  "explícita; operação reversível");
    } catch (const std::exception &e) {
      const auto message = std::string(author::diagnostic(e.what()));
      dialog->hide();
      Glib::signal_idle().connect_once([this, life, message] {
        if (!life.expired())
          show_error("Inserção não aplicada", std::runtime_error(message));
      });
    }
  });
  dialog->present();
}
} // namespace inde::ui
