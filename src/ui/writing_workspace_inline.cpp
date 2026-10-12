#include "inde/ui/author_diagnostics.hpp"
#include "inde/application/inline_commands.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "writing_workspace_util.hpp"
#include <algorithm>
#include <stdexcept>
namespace inde::ui {
void WritingWorkspace::close_inline_suggestions() {
  inline_debounce_.disconnect();
  inline_popover_.popdown();
}
void WritingWorkspace::schedule_inline_suggestions() {
  inline_debounce_.disconnect();
  if (loading_editor_ || restoring_undo_ || ime_active_ || !current_document_ ||
      !editor_.has_focus()) {
    inline_popover_.popdown();
    return;
  }
  const std::weak_ptr<int> life = clipboard_lifetime_;
  inline_debounce_ = Glib::signal_timeout().connect(
      [this, life] {
        if (!life.expired())
          refresh_inline_suggestions();
        return false;
      },
      120);
}
void WritingWorkspace::refresh_inline_suggestions() try {
  inline_popover_.popdown();
  if (!current_document_ || ime_active_ || !editor_.has_focus())
    return;
  auto buffer = editor_.get_buffer();
  Gtk::TextIter a, b;
  if (buffer->get_selection_bounds(a, b))
    return;
  auto cursor = buffer->get_iter_at_mark(buffer->get_insert());
  const int tail_start=std::max(0,cursor.get_offset()-129);
  const auto tail=buffer->get_text(buffer->get_iter_at_offset(tail_start),cursor).raw();
  auto query=application::inline_query(tail,static_cast<std::size_t>(cursor.get_offset()-tail_start));
  if (!query)
    return;
  query->start+=tail_start;query->end+=tail_start;
  auto candidates = application::inline_candidates(
      current_document_->inline_commands, *query);
  const bool native = query->token.starts_with("@") &&
                      std::string("@inde").starts_with(query->token);
  const bool project_query=application::project_inline_query(query->token).has_value();
  if (candidates.empty() && !native && !project_query)
    return;
  while (auto *child = inline_suggestions_.get_first_child())
    inline_suggestions_.remove(*child);
  const auto epoch = edit_epoch_, revision = edit_revision_;
  const application::TextSelection selection{static_cast<std::size_t>(cursor.get_offset()),
    static_cast<std::size_t>(buffer->get_iter_at_mark(buffer->get_selection_bound()).get_offset())};
  const std::weak_ptr<int> life = clipboard_lifetime_;
  auto *title =
      Gtk::make_managed<Gtk::Label>("Ações no texto — escolha explícita");
  inline_suggestions_.append(*title);
  if(project_query) append_project_inline(*query);
  for (std::size_t n = 0; n < std::min<std::size_t>(8, candidates.size());
       ++n) {
    auto command = current_document_->inline_commands[candidates[n]];
    auto *button = Gtk::make_managed<Gtk::Button>(command.label);
    button->set_tooltip_text(Glib::ustring(command.content).substr(0,160));
    button->signal_clicked().connect(
        [this, life, epoch, revision, selection, q = *query, command] {
          if (life.expired())
            return;
          close_inline_suggestions();
          try {
            require_completed_composition();
            auto state = capture_edit_state();
            if (epoch != edit_epoch_ || revision != edit_revision_ ||
                state.selection.cursor != selection.cursor ||
                state.selection.bound != selection.bound)
              throw std::runtime_error(
                  "O trecho mudou; solicite novamente as sugestões no texto");
            apply_edit_transaction(application::replace_selection(
                                       state, q.start, q.end, command.content),
                                   "Ação no texto: " + command.label);
          } catch (const std::exception &e) {
            show_error("Não foi possível inserir o comando", e);
          }
        });
    inline_suggestions_.append(*button);
  }
  auto *more = Gtk::make_managed<Gtk::Button>("Abrir comandos e ferramentas…");
  more->signal_clicked().connect([this, life] {
    if (life.expired())
      return;
    close_inline_suggestions();
    show_inline_commands();
  });
  inline_suggestions_.append(*more);
  Gdk::Rectangle rect;
  editor_.get_iter_location(cursor, rect);
  int x{}, y{};
  editor_.buffer_to_window_coords(Gtk::TextWindowType::WIDGET, rect.get_x(),
                                  rect.get_y(), x, y);
  rect.set_x(x);
  rect.set_y(y);
  inline_popover_.set_pointing_to(rect);
  inline_popover_.popup();
} catch (const std::exception &e) {
  signal_status_message_.emit(author::diagnostic(e.what()));
}
void WritingWorkspace::show_inline_commands(bool manager) {
  if (!current_document_ || !owner_window())
    return;
  close_inline_suggestions();
  const std::weak_ptr<int> life = clipboard_lifetime_;
  const auto epoch = edit_epoch_, revision = edit_revision_;
  const auto before = capture_edit_state();
  const auto selection=before.selection;
  auto query = application::inline_query(before.document.content,
                                         before.selection.cursor);
  if (before.selection.cursor != before.selection.bound)
    query.reset();
  auto *dialog = new OverlayDialog(manager ? "Textos prontos do documento"
                                           : "Ações no texto",
                                   *owner_window(), true);
  dialog->set_secondary_text(
      "Modelos são texto literal, não scripts. Prefixo e palavra-chave oferecem sugestões durante a escrita; só a escolha insere texto. Ações de elementos da história, comentários e camadas continuam em Ferramentas e no menu da seleção, sem duplicação neste formulário.");
  dialog->add_button("Fechar", Gtk::ResponseType::CANCEL);
  auto *list = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
  if(!manager) {
    auto *help=Gtk::make_managed<Gtk::Label>("Sugestões do Projeto: digite @nome, @personagem:nome, @acontecimento:termo ou @informacao:termo. A escolha abre uma prévia; nada é vinculado silenciosamente.");
    help->set_wrap(false);help->set_ellipsize(Pango::EllipsizeMode::END);
    help->set_max_width_chars(55);help->set_tooltip_text(help->get_text());
    set_accessible_label(*help,help->get_text());
    help->set_xalign(0);list->append(*help);
  }
  for (std::size_t i = 0; i < before.document.inline_commands.size(); ++i) {
    const auto c = before.document.inline_commands[i];
    auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
    auto *button = Gtk::make_managed<Gtk::Button>();
    auto *caption = Gtk::make_managed<Gtk::Label>(c.prefix + c.keyword + " — " + c.label);
    caption->set_wrap(false);
    caption->set_max_width_chars(55);
    caption->set_lines(1);
    caption->set_ellipsize(Pango::EllipsizeMode::END);
    caption->set_xalign(0);
    button->set_child(*caption);
    set_accessible_label(*button, c.prefix + c.keyword + " — " + c.label);
    button->set_hexpand(true);
    button->set_tooltip_text(c.prefix + c.keyword + " — " + c.label + "\n" + Glib::ustring(c.content).substr(0,200));
    button->signal_clicked().connect(
        [this, life, dialog, c, selection, epoch, revision, query, manager, i] {
          if (life.expired())
            return;
          dialog->hide();
          Glib::signal_idle().connect_once([dialog] { delete dialog; });
          if (epoch != edit_epoch_ || revision!=edit_revision_)
            return;
          if (manager) {
            edit_inline_command(i);
            return;
          }
          try {
            auto now = capture_edit_state();
            require_completed_composition();
            if (revision != edit_revision_ ||
                now.selection.cursor != selection.cursor ||
                now.selection.bound != selection.bound)
              throw std::runtime_error(
                  "Documento ou seleção mudou; reabra Ações no texto");
            const auto start =
                query && query->token == c.prefix + c.keyword
                    ? query->start
                    : std::min(selection.cursor, selection.bound);
            apply_edit_transaction(
                application::replace_selection(
                    now, start,
                    std::max(selection.cursor, selection.bound),
                    c.content),
                "Ação no texto: " + c.label);
          } catch (const std::exception &e) {
            show_error("Não foi possível inserir o modelo", e);
          }
        });
    row->append(*button);
    if (manager) {
      auto *remove = Gtk::make_managed<Gtk::Button>("Remover modelo");
      remove->signal_clicked().connect(
          [this, life, dialog, i, epoch, revision] {
            if (life.expired())
              return;
            dialog->hide();
            Glib::signal_idle().connect_once([dialog] { delete dialog; });
            if (epoch != edit_epoch_ || revision != edit_revision_)
              return;
            auto state = capture_edit_state();
            state.document.inline_commands.erase(
                state.document.inline_commands.begin() + i);
            apply_edit_transaction(std::move(state), "Remover texto pronto");
            show_inline_commands(true);
          });
      row->append(*remove);
    }
    list->append(*row);
  }
  auto *manage = Gtk::make_managed<Gtk::Button>(
      manager ? "Novo texto pronto" : "Gerenciar textos prontos");
  manage->signal_clicked().connect([this, life, dialog, manager, epoch] {
    if (life.expired())
      return;
    dialog->hide();
    Glib::signal_idle().connect_once([dialog] { delete dialog; });
    if (epoch != edit_epoch_)
      return;
    if (manager)
      edit_inline_command();
    else
      show_inline_commands(true);
  });
  list->append(*manage);
  dialog->get_content_area()->append(*list);
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->present();
}
void WritingWorkspace::edit_inline_command(std::optional<std::size_t> index) {
  if (!current_document_ || !owner_window())
    return;
  auto before = capture_edit_state();
  if (index && *index >= before.document.inline_commands.size())
    return;
  const auto original = index ? before.document.inline_commands[*index]
                              : project::DocumentInlineCommand{"@", "", "", ""};
  const std::weak_ptr<int> life = clipboard_lifetime_;
  const auto epoch = edit_epoch_, revision = edit_revision_;
  auto *dialog =
      new OverlayDialog(index ? "Editar texto pronto" : "Novo texto pronto",
                        *owner_window(), true);
  auto *prefix = Gtk::make_managed<Gtk::Entry>();
  prefix->set_text(original.prefix);
  prefix->set_placeholder_text("Prefixo (ex.: @ ou #)");
  auto *keyword = Gtk::make_managed<Gtk::Entry>();
  keyword->set_text(original.keyword);
  keyword->set_placeholder_text("Palavra-chave opcional, sem espaços");
  auto *label = Gtk::make_managed<Gtk::Entry>();
  label->set_text(original.label);
  label->set_placeholder_text("Nome do modelo");
  auto *content = Gtk::make_managed<Gtk::TextView>();
  content->get_buffer()->set_text(original.content);
  content->set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
  content->set_size_request(400, 140);
  append_labeled_form_field(*dialog->get_content_area(), "Prefixo", *prefix);
  append_labeled_form_field(*dialog->get_content_area(), "Palavra-chave",
                            *keyword);
  append_labeled_form_field(*dialog->get_content_area(), "Nome", *label);
  append_labeled_form_field(*dialog->get_content_area(),
                            "Texto literal a inserir", *content);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar modelo", Gtk::ResponseType::ACCEPT);
  dialog->signal_response().connect([this, life, dialog, index, prefix, keyword,
                                     label, content, epoch,
                                     revision](int response) {
    if (response == Gtk::ResponseType::ACCEPT && !life.expired())
      try {
        require_completed_composition();
        if (epoch != edit_epoch_ || revision != edit_revision_)
          throw std::runtime_error("O Documento mudou; reabra o texto pronto");
        auto state = capture_edit_state();
        project::DocumentInlineCommand c{prefix->get_text(),
                                         keyword->get_text(), label->get_text(),
                                         content->get_buffer()->get_text()};
        if (index)
          state.document.inline_commands[*index] = std::move(c);
        else
          state.document.inline_commands.push_back(std::move(c));
        project::validate_inline_commands(state.document.inline_commands);
        apply_edit_transaction(std::move(state), "Salvar texto pronto");
      } catch (const std::exception &e) {
        show_error("Modelo inline não foi salvo", e);
        return;
      }
    dialog->hide();
  });
  dialog->present();
}
} // namespace inde::ui
