#include "inde/ui/author_diagnostics.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/review_workspace.hpp"
#include <algorithm>
#include <glib.h>
namespace inde::ui {
namespace {
std::string status_label(const std::string &s) {
  if (s == "draft")
    return "Em escrita";
  if (s == "in_review")
    return "Em revisão";
  if (s == "changes_requested")
    return "Ajustes solicitados";
  if (s == "approved")
    return "Revisão aprovada";
  return "Estado desconhecido";
}
Gtk::Label *label(Gtk::Box &body, const std::string &text,
                  const char *css = nullptr) {
  auto *value = Gtk::make_managed<Gtk::Label>(text);
  value->set_xalign(0);
  value->set_wrap(true);
  if (css)
    value->add_css_class(css);
  body.append(*value);
  return value;
}
std::string excerpt(const std::string &s, std::size_t max = 320) {
  if (project::utf8_character_count(s) <= max)
    return s;
  return s.substr(0,
                  static_cast<std::size_t>(
                      g_utf8_offset_to_pointer(s.c_str(), max) - s.c_str())) +
         "…";
}
} // namespace
void ReviewWorkspace::fail(const std::string &title,
                           const std::exception &error) {
  auto *owner = dynamic_cast<Gtk::Window *>(get_root());
  if (!owner) {
    summary_.set_text(author::diagnostic(error.what()));
    return;
  }
  auto *dialog = new OverlayDialog(title, *owner, true);
  dialog->set_secondary_text(author::diagnostic(error.what()));
  dialog->add_button("Fechar", Gtk::ResponseType::CLOSE);
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->present();
}
void ReviewWorkspace::decide(const project::Document &expected,
                             const project::Document &after,
                             const std::string &name) {
  if (!service_.current() ||
      service_.current()->manifest().project_id != project_id_ || !change_)
    throw std::runtime_error(
        "O Projeto mudou ou a autoridade de edição está indisponível");
  const auto id = expected.id;
  (void)change_(expected, after, name);
  show_document(id);
}
void ReviewWorkspace::show_document(const std::string &id) {
  try {
    scan_.disconnect();
    const auto value = service_.writing().document(id);
    if (!value) {
      selected_document_.clear();
      pages_.set_visible_child("queue");
      load_page();
      return;
    }
    if (selected_document_ != id)
      item_offset_ = 0;
    selected_document_ = id;
    project_id_ = service_.current()->manifest().project_id;
    const auto d = *value;
    while (auto *child = detail_body_.get_first_child())
      detail_body_.remove(*child);
    auto *actions =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    auto *back = Gtk::make_managed<Gtk::Button>("Voltar à fila");
    auto *open = Gtk::make_managed<Gtk::Button>("Abrir texto");
    auto *comparison = Gtk::make_managed<Gtk::Button>("Comparar versões…");
    auto *more = Gtk::make_managed<Gtk::MenuButton>();
    more->set_label("Documento");
    auto *popover = Gtk::make_managed<Gtk::Popover>();
    auto *options = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    options->set_margin(8);
    for (const auto &name :
         {"Registrar estado editorial…", "Criar revisão independente…",
          "Atualizar revisão"}) {
      auto *button = Gtk::make_managed<Gtk::Button>(name);
      options->append(*button);
      button->signal_clicked().connect(
          [this, d, popover, name = std::string(name)] {
            popover->popdown();
            if (name == "Registrar estado editorial…")
              status_dialog(d);
            else if (name == "Criar revisão independente…")
              revision_dialog(d);
            else
              show_document(d.id);
          });
    }
    popover->set_child(*options);
    more->set_popover(*popover);
    actions->append(*back);
    actions->append(*open);
    actions->append(*comparison);
    actions->append(*more);
    detail_body_.append(*actions);
    back->signal_clicked().connect([this] {
      pages_.set_visible_child("queue");
      load_page();
    });
    open->signal_clicked().connect([this, id] { open_.emit(id); });
    comparison->signal_clicked().connect([this, d] { comparison_dialog(d); });
    label(detail_body_, d.title, "title-1");
    const auto state = application::editorial_state(d);
    const auto counts = application::review_counts(d);
    label(
        detail_body_,
        status_label(state.state) +
            (state.outdated ? " — decisão desatualizada; conteúdo mudou" : "") +
            " · " + std::to_string(counts.open_comments) +
            " comentários abertos · " +
            std::to_string(counts.pending_suggestions) + " sugestões",
        "review-kind");
    if (!state.reviewer.empty())
      label(detail_body_,
            "Registrado por " + state.reviewer + " em " + state.decided_at +
                ". " + state.note,
            "dim-label");
    auto *history =
        Gtk::make_managed<Gtk::Expander>("Identidade e histórico das decisões");
    auto *history_body =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    label(*history_body,
          "Documento: " + d.id + " · Atualizado: " + d.updated_at, "dim-label");
    std::vector<project::TextAnnotation> decisions;
    for (const auto &a : d.semantics.annotations)
      if (a.type_key == "inde.editor.review-state")
        decisions.push_back(a);
    std::sort(decisions.begin(), decisions.end(),
              [](const auto &a, const auto &b) {
                return std::stoll(application::review_field(a, "sequence")) >
                       std::stoll(application::review_field(b, "sequence"));
              });
    for (std::size_t i = 0; i < std::min<std::size_t>(decisions.size(), 20);
         ++i) {
      const auto &a = decisions[i];
      label(*history_body,
            status_label(application::review_field(a, "state")) + " — " +
                application::review_field(a, "reviewer") + " · " +
                a.created_at + "\n" + application::review_field(a, "note"));
    }
    if (decisions.size() > 20)
      label(*history_body,
            "Últimas 20 decisões exibidas; o histórico completo permanece no "
            "Documento.",
            "dim-label");
    history->set_child(*history_body);
    detail_body_.append(*history);
    std::vector<project::TextAnnotation> items;
    for (const auto &a : d.semantics.annotations)
      if (a.status != "archived" && (a.type_key == "inde.editor.comment" ||
                                     a.type_key == "inde.editor.suggestion"))
        items.push_back(a);
    if (item_offset_ >= items.size())
      item_offset_ = 0;
    for (std::size_t i = item_offset_;
         i < std::min(items.size(), item_offset_ + 30); ++i) {
      const auto a = items[i];
      auto *card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
      card->add_css_class("content-card");
      card->add_css_class("review-item");
      card->set_margin(4);
      const bool comment = a.type_key == "inde.editor.comment";
      const auto state =
          application::review_field(a, "state", comment ? "open" : "pending");
      label(*card,
            comment ? (state == "resolved" ? "Comentário resolvido"
                                           : "Comentário aberto")
                    : (state == "pending"    ? "Sugestão pendente"
                       : state == "accepted" ? "Sugestão aplicada"
                                             : "Sugestão recusada"),
            "heading");
      if (comment)
        label(*card, excerpt(application::review_field(a, "note"), 1000));
      else {
        label(*card, "Observado: " +
                         excerpt(application::review_field(a, "expected")));
        label(*card, "Proposto: " + excerpt(application::review_field(
                                        a, "replacement", "")));
        label(*card, excerpt(application::review_field(a, "note")),
              "dim-label");
      }
      const auto anchor =
          std::find_if(d.anchors.begin(), d.anchors.end(),
                       [&](const auto &v) { return v.id == a.anchor_id; });
      if (anchor != d.anchors.end())
        label(*card,
              "Trecho Unicode " + std::to_string(anchor->start_offset) + "–" +
                  std::to_string(anchor->end_offset),
              "dim-label");
      if (comment) {
        auto *decide_button = Gtk::make_managed<Gtk::Button>(
            state == "resolved" ? "Reabrir comentário" : "Resolver comentário");
        decide_button->set_halign(Gtk::Align::START);
        card->append(*decide_button);
        decide_button->signal_clicked().connect([this, d, a, state] {
          try {
            const auto after =
                application::decide_comment(d, a.id, state != "resolved");
            decide(d, after, "Decisão de comentário editorial");
          } catch (const std::exception &e) {
            fail("Comentário não decidido", e);
          }
        });
      } else if (state == "pending") {
        auto *row =
            Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
        auto *accept = Gtk::make_managed<Gtk::Button>("Examinar e aplicar…"),
             *reject = Gtk::make_managed<Gtk::Button>("Recusar sugestão…");
        row->append(*accept);
        row->append(*reject);
        card->append(*row);
        accept->signal_clicked().connect(
            [this, d, a] { suggestion_dialog(d, a, true); });
        reject->signal_clicked().connect(
            [this, d, a] { suggestion_dialog(d, a, false); });
      }
      detail_body_.append(*card);
    }
    if (items.empty())
      label(detail_body_,
            "Nenhum comentário ou sugestão. Abra o texto para comentar ou "
            "propor uma alteração.",
            "dim-label");
    if (items.size() > 30) {
      auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
      auto *previous = Gtk::make_managed<Gtk::Button>("Itens anteriores"),
           *next = Gtk::make_managed<Gtk::Button>("Próximos itens");
      previous->set_sensitive(item_offset_ > 0);
      next->set_sensitive(item_offset_ + 30 < items.size());
      row->append(*previous);
      row->append(*next);
      detail_body_.append(*row);
      previous->signal_clicked().connect([this, id] {
        item_offset_ = item_offset_ >= 30 ? item_offset_ - 30 : 0;
        show_document(id);
      });
      next->signal_clicked().connect([this, id] {
        item_offset_ += 30;
        show_document(id);
      });
    }
    const auto diagnostics =
        project::semantic_diagnostics(d, service_.writing().semantic_types());
    if (!diagnostics.empty()) {
      auto *issues = Gtk::make_managed<Gtk::Expander>(
          "Pendências de marcação — " + std::to_string(diagnostics.size()));
      auto *body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
      for (std::size_t i = 0; i < std::min<std::size_t>(30, diagnostics.size());
           ++i)
        label(*body, diagnostics[i].message);
      issues->set_child(*body);
      detail_body_.append(*issues);
    }
    pages_.set_visible_child("detail");
  } catch (const std::exception &e) {
    fail("Não foi possível abrir a revisão", e);
  }
}
void ReviewWorkspace::status_dialog(const project::Document &d) {
  auto *owner = dynamic_cast<Gtk::Window *>(get_root());
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Registrar estado editorial", *owner, true);
  dialog->set_secondary_text(
      "Decisão humana sobre este estado do Documento. Mudanças posteriores a "
      "tornam desatualizada; não publica nem bloqueia a escrita. Aprovar exige "
      "decidir comentários e sugestões pendentes.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Registrar decisão", Gtk::ResponseType::ACCEPT);
  auto *state = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &s : {"draft", "in_review", "changes_requested", "approved"})
    state->append(s, status_label(s));
  state->set_active_id(application::editorial_state(d).state);
  set_accessible_label(*state, "Novo estado editorial");
  auto *reviewer = Gtk::make_managed<Gtk::Entry>();
  reviewer->set_placeholder_text("Responsável pela decisão");
  reviewer->set_max_length(80);
  auto *note = Gtk::make_managed<Gtk::Entry>();
  note->set_placeholder_text("Parecer ou orientação (opcional)");
  note->set_max_length(1500);
  dialog->get_content_area()->append(*state);
  dialog->get_content_area()->append(*reviewer);
  dialog->get_content_area()->append(*note);
  const auto project = project_id_;
  dialog->signal_response().connect(
      [this, dialog, d, state, reviewer, note, project](int r) {
        dialog->hide();
        if (r != Gtk::ResponseType::ACCEPT)
          return;
        try {
          if (project != project_id_ || !service_.current() ||
              project != service_.current()->manifest().project_id)
            throw std::runtime_error("O Projeto mudou");
          const auto after = application::set_editorial_state(
              d, state->get_active_id().raw(), reviewer->get_text().raw(),
              note->get_text().raw());
          decide(d, after, "Registrar decisão editorial");
        } catch (const std::exception &e) {
          fail("Decisão não registrada", e);
        }
      });
  dialog->present();
}
void ReviewWorkspace::suggestion_dialog(const project::Document &d,
                                        const project::TextAnnotation &a,
                                        bool accept) {
  auto *owner = dynamic_cast<Gtk::Window *>(get_root());
  if (!owner)
    return;
  auto *dialog = new OverlayDialog(
      accept ? "Aplicar sugestão editorial — prévia" : "Recusar sugestão",
      *owner, true);
  dialog->set_secondary_text(
      accept ? "A confirmação substitui somente o trecho ainda reconhecido. Formatação, trechos marcados e camadas usam a transformação do editor; marcações afetadas podem ficar pendentes. Não há incorporação automática de outra versão."
             : "Registra a recusa e conserva a proposta. O texto não muda.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button(accept ? "Aplicar ao trecho" : "Confirmar recusa",
                     Gtk::ResponseType::ACCEPT);
  label(*dialog->get_content_area(),
        "Observado:\n" + application::review_field(a, "expected"));
  label(*dialog->get_content_area(),
        "Proposto:\n" + application::review_field(a, "replacement"));
  const auto project = project_id_;
  dialog->signal_response().connect(
      [this, dialog, d, a, accept, project](int r) {
        dialog->hide();
        if (r != Gtk::ResponseType::ACCEPT)
          return;
        try {
          if (project != project_id_)
            throw std::runtime_error("O Projeto mudou");
          const auto after = application::decide_suggestion(d, a.id, accept);
          decide(d, after,
                 accept ? "Aplicar sugestão editorial"
                        : "Recusar sugestão editorial");
        } catch (const std::exception &e) {
          fail("Sugestão não decidida", e);
        }
      });
  dialog->present();
}
void ReviewWorkspace::revision_dialog(const project::Document &d) {
  auto *owner = dynamic_cast<Gtk::Window *>(get_root());
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Criar revisão independente", *owner, true);
  dialog->set_secondary_text(
      "Cria outro Documento com texto, formatação, textos prontos, perspectiva livre e organização. Não duplica comentários, sugestões, decisões, trechos marcados, vínculos ou marcações do texto; eles permanecem na fonte. Nada é apagado. A comparação usa os estados atuais dos dois Documentos, não um histórico imutável.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar revisão", Gtk::ResponseType::ACCEPT);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Identificação da revisão");
  name->set_max_length(80);
  dialog->get_content_area()->append(*name);
  const auto project = project_id_;
  dialog->signal_response().connect([this, dialog, d, name, project](int r) {
    dialog->hide();
    if (r != Gtk::ResponseType::ACCEPT)
      return;
    try {
      if (project != project_id_ || !service_.current() ||
          project != service_.current()->manifest().project_id)
        throw std::runtime_error("O Projeto mudou");
      const auto created = service_.writing().create_editorial_revision(
          d, name->get_text().raw());
      show_document(created.id);
    } catch (const std::exception &e) {
      fail("Revisão não criada", e);
    }
  });
  dialog->present();
}
void ReviewWorkspace::comparison_dialog(const project::Document &d) {
  auto *owner = dynamic_cast<Gtk::Window *>(get_root());
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog("Escolher Documento para comparação", *owner, true);
  dialog->set_secondary_text(
      "Base observada → Documento aberto. Comparação textual somente leitura, sem mesclagem de versões, cópia automática ou escolha de versão vencedora.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Comparar", Gtk::ResponseType::ACCEPT);
  const auto project = project_id_;
  auto *picker = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar base de comparação",
      [this, id = d.id, project](const std::string &s, std::size_t limit) {
        if (!service_.current() ||
            project != service_.current()->manifest().project_id)
          return std::vector<IncrementalSelection>{};
        persistence::DocumentQuery q;
        q.search = s;
        q.limit = limit;
        std::vector<IncrementalSelection> out;
        for (const auto &v : service_.writing().document_summaries(q))
          if (v.id != id)
            out.push_back({v.id, v.title, v.revision_label});
        return out;
      });
  if (d.revision_of_id) {
    const auto base = service_.writing().document(*d.revision_of_id);
    if (base)
      picker->set_selected(base->id, base->title);
  }
  dialog->get_content_area()->append(*picker);
  dialog->signal_response().connect([this, dialog, d, picker, project](int r) {
    dialog->hide();
    if (r != Gtk::ResponseType::ACCEPT)
      return;
    try {
      if (!service_.current() ||
          project != service_.current()->manifest().project_id ||
          !picker->selected_id())
        throw std::runtime_error("Escolha uma base neste Projeto");
      const auto base = service_.writing().document(*picker->selected_id()),
                 current = service_.writing().document(d.id);
      if (!base || !current || base->id == current->id)
        throw std::runtime_error(
            "Escolha dois Documentos existentes e distintos");
      compare(*base, *current);
    } catch (const std::exception &e) {
      fail("Comparação não iniciada", e);
    }
  });
  dialog->present();
}
} // namespace inde::ui
